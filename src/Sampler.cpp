#include "Sampler.h"
#include "Error.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#ifdef WIN32
# define USE_SSE
#endif

#ifdef USE_SSE
#include <xmmintrin.h>
#endif

namespace rmixer
{

/** Parameters for Tempo resize. */

/* Too small frame segment give no difference between pitch resampling.
 * Tested manually with 44100Hz rate. */
constexpr int kSOLASegmentFrameCount = 2048;

/* Large SOLA search size gives high-quality, slow performance.
 * Smaller SOLA search size gives low-quality, high performance.
 * \warn  Should be multiply of 8 */
constexpr int kSOLAOverlapFrameCount = 32;

/* Enable SOLA overlapping method for high quality tempo resampling */
#define SEARCH_SOLA

Sampler::Sampler(const Sound& source)
  : source_(&source), tempo_(1.0), pitch_(1.0), volume_(1.0), info_(source.get_info()) {}

Sampler::Sampler(const Sound& source, const SoundInfo& target_quality)
  : source_(&source), tempo_(1.), pitch_(1.0), volume_(1.0), info_(target_quality) {}

Sampler::~Sampler() {}

void Sampler::SetTempo(double tempo)
{
  tempo_ = tempo;
}

void Sampler::SetPitch(double pitch)
{
  pitch_ = pitch;
}

void Sampler::SetPitchConsistTempo(double length)
{
  tempo_ = length;
  pitch_ = length;
}

void Sampler::SetVolume(double volume)
{
  volume_ = volume;
}

template <typename T>
void Resample_from_u4(const Sound &source, T* p)
{
  const size_t framecnt = source.GetFrameCount();
  const int8_t* p_source = source.ptr();
  for (size_t i = 0; i < framecnt; i++)
  {
    *p = (*p_source & 0b00001111) << (sizeof(T) * 8 - 4);
    // if not last frame ...
    if (framecnt - i > 1) *(p + 1) = (*p_source & 0b11110000) << (sizeof(T) * 8 - 8);
    p += 2;
  }
}

template<typename T_TO>
void Resample_Internal(const Sound &source, Sound &newsound, const SoundInfo& newinfo, double pitch)
{
  DASSERT(sizeof(T_TO) == 2 || sizeof(T_TO) == 4);
  ASSERT(&source != &newsound);

  const T_TO* new_sound_ref_ptr = 0;
  T_TO* new_sound_mod_ptr = 0, *mod_ptr = 0;

  // 1. do byte conversion(PCM conversion), if necessary.
  bool is_bitsize_diff = source.get_info().bitsize != newinfo.bitsize;
  bool is_channelsize_diff = source.get_info().channels != newinfo.channels;
  const size_t framecount = source.GetFrameCount();
  if (is_bitsize_diff)
  {
    mod_ptr = (T_TO*)malloc(sizeof(T_TO) * framecount * source.get_info().channels);
    // check for 4 bit PCM (XXX: is this really exists?)
    if (source.get_info().bitsize == 4)
    {
      Resample_from_u4(source, mod_ptr);
    } else
    {
      if (sizeof(T_TO) == 2 /* 16bit */)
        drwav__pcm_to_s16((int16_t*)mod_ptr, (uint8_t*)source.ptr(), framecount * source.get_info().channels, source.get_info().bitsize / 8);
      else
        drwav__pcm_to_s32((int32_t*)mod_ptr, (uint8_t*)source.ptr(), framecount * source.get_info().channels, source.get_info().bitsize / 8);
    }
    new_sound_ref_ptr = new_sound_mod_ptr = mod_ptr;
  } else
  {
    new_sound_ref_ptr = reinterpret_cast<const T_TO*>(source.ptr());
  }

  // 2. channel conversion
  // downsample into single channel and averaging method
  if (is_channelsize_diff)
  {
    T_TO downsample;
    T_TO* p;
    p = mod_ptr = (T_TO*)malloc(sizeof(T_TO) * framecount * newinfo.channels);
    for (size_t i = 0; i < framecount; i++)
    {
      downsample = 0;
      for (size_t ch = 0; ch < source.get_info().channels; ch++)
        downsample += *(new_sound_ref_ptr++) / source.get_info().channels;
      for (size_t ch = 0; ch < newinfo.channels; ch++)
        *(p++) = downsample;
    }
    // replace previous cache
    if (new_sound_mod_ptr) free(new_sound_mod_ptr);
    new_sound_ref_ptr = new_sound_mod_ptr = mod_ptr;
  }

  double sample_rate = (double)newinfo.rate / source.get_info().rate / pitch;
  const size_t new_framecount = static_cast<size_t>(framecount * sample_rate);

  // 3. do Sampling / Pitch conversion
  // (may be a little coarse when downsampling it only refers two samples at most)
  if (sample_rate != 1.0)
  {
    mod_ptr = (T_TO*)malloc(sizeof(T_TO) * new_framecount * newinfo.channels);
    double a, b, original_sample_idx = 0, original_rate = 1 / sample_rate;
    long sample_idx_int = 0;
    size_t i_byte = 0;
    for (size_t i = 0; i < new_framecount; ++i)
    {
      original_sample_idx = ((double)i / new_framecount) * (framecount - 1);
      sample_idx_int = static_cast<long>(original_sample_idx);
      b = original_sample_idx - sample_idx_int;
      a = 1.0 - b;
      for (size_t ch = 0; ch < newinfo.channels; ++ch)
      {
        mod_ptr[i_byte++] = static_cast<T_TO>(new_sound_ref_ptr[sample_idx_int * newinfo.channels + ch] * a +
                                              new_sound_ref_ptr[(sample_idx_int + 1) * newinfo.channels + ch] * b);
      }
    }
    // replace previous cache
    if (new_sound_mod_ptr) free(new_sound_mod_ptr);
    new_sound_ref_ptr = new_sound_mod_ptr = mod_ptr;
  }

  // copy data to newsound
  if (!new_sound_mod_ptr)
  {
    size_t s = sizeof(T_TO) * source.GetFrameCount() * source.get_info().channels;
    new_sound_mod_ptr = (T_TO*)malloc(s);
    memcpy(new_sound_mod_ptr, new_sound_ref_ptr, s);
  }
  newsound.SetBuffer(newinfo.bitsize, newinfo.channels, new_framecount, newinfo.rate, new_sound_mod_ptr);
}

//#define SEARCH_SOLA_SSE

/** calcCrossCorr code is from SoundTouch audio processing library */

////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
double calcCrossCorr_SSE(const T *pV1, const T *pV2, double &anorm, int channels)
{
  int i;
  const float *pVec1;
  const __m128 *pVec2;
  __m128 vSum, vNorm;
  const size_t samplebytesize = sizeof(T);

  // Note. It means a major slow-down if the routine needs to tolerate 
  // unaligned __m128 memory accesses. It's way faster if we can skip 
  // unaligned slots and use _mm_load_ps instruction instead of _mm_loadu_ps.
  // This can mean up to ~ 10-fold difference (incl. part of which is
  // due to skipping every second round for stereo sound though).
  //
  // Compile-time define SOUNDTOUCH_ALLOW_NONEXACT_SIMD_OPTIMIZATION is provided
  // for choosing if this little cheating is allowed.

#ifdef SOUNDTOUCH_ALLOW_NONEXACT_SIMD_OPTIMIZATION
  // Little cheating allowed, return valid correlation only for 
  // aligned locations, meaning every second round for stereo sound.

#define _MM_LOAD    _mm_load_ps

  if (((ulongptr)pV1) & 15) return -1e50;    // skip unaligned locations

#else
  // No cheating allowed, use unaligned load & take the resulting
  // performance hit.
#define _MM_LOAD    _mm_loadu_ps
#endif 

  // ensure overlapLength is divisible by 8
  static_assert(kSOLAOverlapFrameCount % 8 == 0,
    "Overlap byte size must be divisible by 8.");

  // Calculates the cross-correlation value between 'pV1' and 'pV2' vectors
  // Note: pV2 _must_ be aligned to 16-bit boundary, pV1 need not.
  pVec1 = (const float*)pV1;
  pVec2 = (const __m128*)pV2;
  vSum = vNorm = _mm_setzero_ps();

  // Unroll the loop by factor of 4 * 4 operations. Use same routine for
  // stereo & mono, for mono it just means twice the amount of unrolling.
  for (i = 0; i < channels * kSOLAOverlapFrameCount * samplebytesize / 16; i++)
  {
    __m128 vTemp;
    // vSum += pV1[0..3] * pV2[0..3]
    vTemp = _MM_LOAD(pVec1);
    vSum = _mm_add_ps(vSum, _mm_mul_ps(vTemp, pVec2[0]));
    vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));

    // vSum += pV1[4..7] * pV2[4..7]
    vTemp = _MM_LOAD(pVec1 + 4);
    vSum = _mm_add_ps(vSum, _mm_mul_ps(vTemp, pVec2[1]));
    vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));

    // vSum += pV1[8..11] * pV2[8..11]
    vTemp = _MM_LOAD(pVec1 + 8);
    vSum = _mm_add_ps(vSum, _mm_mul_ps(vTemp, pVec2[2]));
    vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));

    // vSum += pV1[12..15] * pV2[12..15]
    vTemp = _MM_LOAD(pVec1 + 12);
    vSum = _mm_add_ps(vSum, _mm_mul_ps(vTemp, pVec2[3]));
    vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));

    pVec1 += 16;
    pVec2 += 4;
  }

  // return value = vSum[0] + vSum[1] + vSum[2] + vSum[3]
  float *pvNorm = (float*)&vNorm;
  float norm = (pvNorm[0] + pvNorm[1] + pvNorm[2] + pvNorm[3]);
  anorm = norm;

  float *pvSum = (float*)&vSum;
  return (double)(pvSum[0] + pvSum[1] + pvSum[2] + pvSum[3]) / sqrt(norm < 1e-9 ? 1.0 : norm);

  /* This is approximately corresponding routine in C-language yet without normalization:
  double corr, norm;
  uint i;

  // Calculates the cross-correlation value between 'pV1' and 'pV2' vectors
  corr = norm = 0.0;
  for (i = 0; i < channels * overlapLength / 16; i ++)
  {
      corr += pV1[0] * pV2[0] +
              pV1[1] * pV2[1] +
              pV1[2] * pV2[2] +
              pV1[3] * pV2[3] +
              pV1[4] * pV2[4] +
              pV1[5] * pV2[5] +
              pV1[6] * pV2[6] +
              pV1[7] * pV2[7] +
              pV1[8] * pV2[8] +
              pV1[9] * pV2[9] +
              pV1[10] * pV2[10] +
              pV1[11] * pV2[11] +
              pV1[12] * pV2[12] +
              pV1[13] * pV2[13] +
              pV1[14] * pV2[14] +
              pV1[15] * pV2[15];

  for (j = 0; j < 15; j ++) norm += pV1[j] * pV1[j];

      pV1 += 16;
      pV2 += 16;
  }
  return corr / sqrt(norm);
  */
}

template <typename T>
double calcCrossCorrAccumulate_SSE(const T *pV1, const T *pV2, double &norm, int channels)
{
  // call usual calcCrossCorr function because SSE does not show big benefit of 
  // accumulating "norm" value, and also the "norm" rolling algorithm would get 
  // complicated due to SSE-specific alignment-vs-nonexact correlation rules.
  return calcCrossCorr_SSE(pV1, pV2, norm, channels, samplebytesize);
}

// just a constant to prevent integer overflow...
constexpr int overlapDividerBitsNorm = 2;

template <typename T>
double calcCrossCorr_BASIC(const T *mixingPos, const T *compare, double &norm, int channels)
{
  long corr;
  unsigned long lnorm;
  int i;

  corr = lnorm = 0;

  // only will support unrolling method
  ASSERT(channels * kSOLAOverlapFrameCount % 4 == 0);

  // Same routine for stereo and mono. For stereo, unroll loop for better
  // efficiency and gives slightly better resolution against rounding. 
  // For mono it same routine, just  unrolls loop by factor of 4
  for (i = 0; i < channels * kSOLAOverlapFrameCount; i += 4)
  {
    corr += (mixingPos[i] * compare[i] +
      mixingPos[i + 1] * compare[i + 1]) >> overlapDividerBitsNorm;  // notice: do intermediate division here to avoid integer overflow
    corr += (mixingPos[i + 2] * compare[i + 2] +
      mixingPos[i + 3] * compare[i + 3]) >> overlapDividerBitsNorm;
    lnorm += (mixingPos[i] * mixingPos[i] +
      mixingPos[i + 1] * mixingPos[i + 1]) >> overlapDividerBitsNorm; // notice: do intermediate division here to avoid integer overflow
    lnorm += (mixingPos[i + 2] * mixingPos[i + 2] +
      mixingPos[i + 3] * mixingPos[i + 3]) >> overlapDividerBitsNorm;
  }

  // Normalize result by dividing by sqrt(norm) - this step is easiest 
  // done using floating point operation
  norm = (double)lnorm;
  return (double)corr / sqrt((norm < 1e-9) ? 1.0 : norm);
}

template <typename T>
double calcCrossCorrAccumulate_BASIC(const T *mixingPos, const T *compare, double &norm, int channels)
{
  long corr;
  unsigned long lnorm;
  int i;

  // cancel first normalizer tap from previous round
  lnorm = 0;
  for (i = 1; i <= channels; i++)
  {
    lnorm -= (mixingPos[-i] * mixingPos[-i]) >> overlapDividerBitsNorm;
  }

  corr = 0;
  // Same routine for stereo and mono. For stereo, unroll loop for better
  // efficiency and gives slightly better resolution against rounding. 
  // For mono it same routine, just  unrolls loop by factor of 4
  for (i = 0; i < channels * kSOLAOverlapFrameCount; i += 4)
  {
    corr += (mixingPos[i] * compare[i] +
      mixingPos[i + 1] * compare[i + 1]) >> overlapDividerBitsNorm;  // notice: do intermediate division here to avoid integer overflow
    corr += (mixingPos[i + 2] * compare[i + 2] +
      mixingPos[i + 3] * compare[i + 3]) >> overlapDividerBitsNorm;
  }

  // update normalizer with last samples of this round
  for (int j = 0; j < channels; j++)
  {
    i--;
    lnorm += (mixingPos[i] * mixingPos[i]) >> overlapDividerBitsNorm;
  }

  norm += (double)lnorm;

  // Normalize result by dividing by sqrt(norm) - this step is easiest 
  // done using floating point operation
  return (double)corr / sqrt((norm < 1e-9) ? 1.0 : norm);
}

#define calcCrossCorr calcCrossCorr_BASIC
#define calcCrossCorrAccumulate calcCrossCorrAccumulate_BASIC

/** calcCrossCorr code region end */


#ifdef USE_SSE
template <typename T>
void Resample_Tempo_Mix_LinearInterpolate(T* dst, T* src, size_t size, size_t channelcount, bool x)
{
  /** we don't check overflow here as linear interpolation should not occur overflow */
  for (size_t i = 1; i <= size; i++)
  {
    const double a = (x == 0) ? ((double)i / (size+1)) : (1.0 - (double)i / (size+1));
    for (size_t j = 0; j < channelcount; j++)
    {
      *dst += static_cast<T>(*src * a);
      dst++;
      src++;
    }
  }
}
#else
#endif

template <typename T>
void Resample_Tempo(Sound &dst, const Sound &src, double length)
{
  ASSERT(&src != &dst);
  size_t src_framecount = src.GetFrameCount();
  // requires at least : basic copy size. too small frame source might fail
  ASSERT(src_framecount > kSOLASegmentFrameCount);
  size_t new_framecount = static_cast<size_t>(src_framecount * length);
  size_t current_frame = 0;
  size_t channelcount = src.get_info().channels;
  size_t src_expected_pos = 0;  // mixing src frame which is expected by time position
  size_t src_opt_pos = 0;       // mixing src frame which is most desired, smiliar wave form.
  const size_t new_alloc_mem_size = sizeof(T) * new_framecount * channelcount;
  T* orgsrc = (T*)src.ptr();
  T* orgdst = (T*)malloc(new_alloc_mem_size);
  memset(orgdst, 0, new_alloc_mem_size);
  T *psrc, *pdst;

  while (current_frame < new_framecount)
  {
    /* calculate total copy frame size, interpolation, etc */
    size_t copytotalframesize = kSOLASegmentFrameCount;
    bool do_beginmix = true;
    bool do_endingmix = true;
    if (current_frame == 0)
    {
      // first copying. don't use starting interpolate
      do_beginmix = false;
    }
    if (current_frame + copytotalframesize >= new_framecount)
    {
      // last copying. don't use ending interpolate
      do_endingmix = false;
      copytotalframesize = new_framecount - current_frame;
    }
    const size_t copyframesize = copytotalframesize - do_beginmix * kSOLAOverlapFrameCount - do_endingmix * kSOLAOverlapFrameCount;

    /* start search optimized position of src */
    src_expected_pos = static_cast<size_t>((double)src_framecount * current_frame / new_framecount);   // set search start position
    if (src_expected_pos > src_framecount - copytotalframesize)
      src_expected_pos = src_framecount - copytotalframesize;
    // searching is only done since second copying
    if (do_beginmix)
    {
#ifdef SEARCH_SOLA
      psrc = orgsrc + (src_expected_pos - kSOLAOverlapFrameCount) * channelcount;
      if (psrc < orgsrc) psrc = orgsrc;
      pdst = orgdst + current_frame * channelcount;
      double norm;  // normalizer calculate for cross correlation
      double corr;  // current corr
      double bestCorr = calcCrossCorr(psrc, pdst, norm, channelcount);
      int bestOffs = 0;
      for (int i = 1; i < kSOLAOverlapFrameCount; ++i)
      {
        corr = calcCrossCorrAccumulate(psrc + i * channelcount, pdst + i * channelcount, norm, channelcount);
        if (corr > bestCorr)
        {
          bestCorr = corr;
          bestOffs = i;
        }
      }
      src_opt_pos = src_expected_pos + bestOffs;
#else
      // don't search optimal pos ...
      src_opt_pos = src_expected_pos;
#endif
    }
    else src_opt_pos = src_expected_pos;

    /* ready to start .. */
    psrc = orgsrc + src_opt_pos * channelcount;
    pdst = orgdst + current_frame * channelcount;

    /* interpolation of starting */
    if (do_beginmix)
    {
      Resample_Tempo_Mix_LinearInterpolate(pdst, psrc, kSOLAOverlapFrameCount, channelcount, 0);
      psrc += kSOLAOverlapFrameCount * channelcount;
      pdst += kSOLAOverlapFrameCount * channelcount;
    }

    /* no interpolation: just copy data */
    const size_t memcopysize = copyframesize * channelcount;
    memcpy(pdst, psrc, sizeof(T) * memcopysize);
    psrc += memcopysize;
    pdst += memcopysize;

    /* interpolation of ending */
    if (do_endingmix)
    {
      Resample_Tempo_Mix_LinearInterpolate(pdst, psrc, kSOLAOverlapFrameCount, channelcount, 1);
    }

    /* update next frame position (up to ending interpolate) */
    current_frame += copytotalframesize - kSOLAOverlapFrameCount * do_endingmix;
  }

  const SoundInfo& si = src.get_info();
  dst.SetBuffer(si.bitsize, si.channels, new_framecount, si.rate, orgdst);
}

bool Sampler::Resample(Sound &newsound)
{
  switch (info_.bitsize)
  {
  case 16:
    Resample_Internal<int16_t>(*source_, newsound, info_, pitch_);
    break;
  case 32:
    Resample_Internal<int32_t>(*source_, newsound, info_, pitch_);
    break;
  default:
    return false;
  }

  // tempo modification (SOLA method)
  if (tempo_ != 1.0)
  {
    Sound s;
    switch (info_.bitsize)
    {
    case 16:
      Resample_Tempo<int16_t>(s, newsound, tempo_);
      break;
    case 32:
      Resample_Tempo<int32_t>(s, newsound, tempo_);
      break;
    }
    newsound.Clear();
    newsound = std::move(s);
  }

  return true;
}


}