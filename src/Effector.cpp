#include "Effector.h"
#include "Sound.h"
#include "Error.h"
#include <algorithm>

#ifndef _M_IX86_FP
#endif

#if defined(__SSE__) || defined(_M_IX86_FP) || defined(_M_X64)
#define USE_SSE
#else
#error "SSE instruction set not enabled"
#endif

#include <xmmintrin.h>

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
  return calcCrossCorr_SSE(pV1, pV2, norm, channels);
}

// just a constant to prevent integer overflow...
constexpr int overlapDividerBitsNorm = 2;

template <typename T> T overlapDivide(T v);
template <> int8_t overlapDivide(int8_t v) { return v; }
template <> int16_t overlapDivide(int16_t v) { return v; }
template <> int32_t overlapDivide(int32_t v) { return v >> overlapDividerBitsNorm; }
template <> uint8_t overlapDivide(uint8_t v) { return v; }
template <> uint16_t overlapDivide(uint16_t v) { return v; }
template <> uint32_t overlapDivide(uint32_t v) { return v >> overlapDividerBitsNorm; }
template <> float overlapDivide(float v) { return v * 1024; }
template <> double overlapDivide(double v) { return v * 1024; }

template <typename T>
double calcCrossCorr_BASIC(const T *mixingPos, const T *compare, double &norm, int channels)
{
  long corr;
  unsigned long lnorm;
  int i;

  corr = lnorm = 0;

  // only will support unrolling method
  RMIXER_ASSERT(channels * kSOLAOverlapFrameCount % 4 == 0);

  // Same routine for stereo and mono. For stereo, unroll loop for better
  // efficiency and gives slightly better resolution against rounding. 
  // For mono it same routine, just  unrolls loop by factor of 4
  for (i = 0; i < channels * kSOLAOverlapFrameCount; i += 4)
  {
    corr += (long)overlapDivide((mixingPos[i] * compare[i] +
      mixingPos[i + 1] * compare[i + 1]));    // notice: do intermediate division here to avoid integer overflow
    corr += (long)overlapDivide((mixingPos[i + 2] * compare[i + 2] +
      mixingPos[i + 3] * compare[i + 3]));
    lnorm += (unsigned long)overlapDivide((mixingPos[i] * mixingPos[i] +
      mixingPos[i + 1] * mixingPos[i + 1]));  // notice: do intermediate division here to avoid integer overflow
    lnorm += (unsigned long)overlapDivide((mixingPos[i + 2] * mixingPos[i + 2] +
      mixingPos[i + 3] * mixingPos[i + 3]));
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
    lnorm -= (unsigned long)overlapDivide((mixingPos[-i] * mixingPos[-i]));
  }

  corr = 0;
  // Same routine for stereo and mono. For stereo, unroll loop for better
  // efficiency and gives slightly better resolution against rounding. 
  // For mono it same routine, just  unrolls loop by factor of 4
  for (i = 0; i < channels * kSOLAOverlapFrameCount; i += 4)
  {
    corr += (long)overlapDivide((mixingPos[i] * compare[i] +
      mixingPos[i + 1] * compare[i + 1]));  // notice: do intermediate division here to avoid integer overflow
    corr += (long)overlapDivide((mixingPos[i + 2] * compare[i + 2] +
      mixingPos[i + 3] * compare[i + 3]));
  }

  // update normalizer with last samples of this round
  for (int j = 0; j < channels; j++)
  {
    i--;
    lnorm += (unsigned long)overlapDivide((mixingPos[i] * mixingPos[i]));
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
void Resample_Tempo_Mix_LinearInterpolate(T* dst, const T* src, size_t size, size_t channelcount, bool x)
{
  /** we don't check overflow here as linear interpolation should not occur overflow */
  for (size_t i = 1; i <= size; i++)
  {
    const double a = (x == 0) ? ((double)i / (size + 1)) : (1.0 - (double)i / (size + 1));
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
size_t Resample_Tempo(T **dst, const T *src, const SoundInfo &info, size_t src_framecount, double length)
{
  // There won't be much difference with tempo effect
  // when frame size is too small.
  // If it does, then just copy data repeatedly.
  // RMIXER_ASSERT(src_framecount > kSOLASegmentFrameCount);
  size_t new_framecount = static_cast<size_t>(src_framecount * length);
  size_t current_frame = 0;
  size_t channelcount = info.channels;
  size_t src_expected_pos = 0;  // mixing src frame which is expected by time position
  size_t src_opt_pos = 0;       // mixing src frame which is most desired, smiliar wave form.
  const size_t new_alloc_mem_size = sizeof(T) * new_framecount * channelcount;
  const T* orgsrc = src;
  T* orgdst = *dst = (T*)malloc(new_alloc_mem_size);
  const T *psrc;
  T *pdst;
  memset(orgdst, 0, new_alloc_mem_size);

  while (current_frame < new_framecount)
  {
    /* calculate total copy frame size, interpolation, etc */
    size_t copytotalframesize = kSOLASegmentFrameCount;
    bool do_beginmix = true;
    bool do_endingmix = true;
    if (current_frame == 0 || src_framecount <= kSOLASegmentFrameCount)
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
    const size_t copyframesize = std::min(
      copytotalframesize - do_beginmix * kSOLAOverlapFrameCount - do_endingmix * kSOLAOverlapFrameCount,
      src_framecount);

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

  return new_framecount;
}

template <typename T>
void Resample_Tempo(Sound &dst, const Sound &src, double length)
{
  T *p = nullptr;
  size_t framecount = Resample_Tempo(&p, (const T*)src.get_ptr(), src.get_soundinfo(),
                                     src.get_frame_count(), length);
  if (framecount == 0)
  {
    if (p) free(p);
    RMIXER_THROW("Error occured while Resample_Tempo");
  }
  dst.SetBuffer(src.get_soundinfo(), framecount, p);
}


/* @return  frame size of new buffer */
template <typename T>
size_t Resample_Pitch(T **dst, const T *src, const SoundInfo& info, size_t frame_size_src, double pitch)
{
  size_t new_framesize = (size_t)(frame_size_src / pitch);
  size_t id = 0;
  //const double ratio = 1.0 / pitch;
  const unsigned ch_cnt = info.channels;
  *dst = (T*)malloc(GetByteFromFrame(new_framesize, info));
  // may be a little coarse when downsampling.
  if (ch_cnt == 1)
  {
    // fast logic for single channel
    for (; id < new_framesize; ++id)
    {
      (*dst)[id] = src[(size_t)(id * pitch)];
    }
  }
  else
  {
    // general logic for multiple channel
    for (; id < new_framesize; ++id)
    {
      size_t dstfidx = (size_t)(id * pitch);
      for (size_t ch = 0; ch < ch_cnt; ++ch)
      {
        (*dst)[id * ch_cnt + ch] = src[dstfidx * ch_cnt + ch];
      }
    }
  }
  return new_framesize;
}

template
size_t Resample_Pitch(uint8_t **dst, const uint8_t *src, const SoundInfo& info, size_t frame_size_src, double pitch);
template
size_t Resample_Pitch(uint16_t **dst, const uint16_t *src, const SoundInfo& info, size_t frame_size_src, double pitch);
template
size_t Resample_Pitch(uint32_t **dst, const uint32_t *src, const SoundInfo& info, size_t frame_size_src, double pitch);

template
size_t Resample_Pitch(int8_t **dst, const int8_t *src, const SoundInfo& info, size_t frame_size_src, double pitch);
template
size_t Resample_Pitch(int16_t **dst, const int16_t *src, const SoundInfo& info, size_t frame_size_src, double pitch);
template
size_t Resample_Pitch(int32_t **dst, const int32_t *src, const SoundInfo& info, size_t frame_size_src, double pitch);

template
size_t Resample_Pitch(float **dst, const float *src, const SoundInfo& info, size_t frame_size_src, double pitch);
template
size_t Resample_Pitch(double **dst, const double *src, const SoundInfo& info, size_t frame_size_src, double pitch);


// shortcut function
template <typename T>
void Resample_Pitch(Sound &dst, const Sound &src, double pitch)
{
  const size_t dst_sample_len = (size_t)(src.get_sample_count() * pitch);

  if (pitch == 1.0)
  {
    dst.copy(src);
  }
  else
  {
    T *p;
    const SoundInfo &info = src.get_soundinfo();
    size_t new_framesize = Resample_Pitch(&p, (T*)src.get_ptr(), info,
                                          src.get_frame_count(), pitch);
    if (new_framesize == 0)
    {
      if (p) free(p);
      RMIXER_THROW("Error occured while Resample_Pitch");
    }
    dst.SetBuffer(info, new_framesize, p);
  }
}

/* @return  frame_size of dst buffer. */
template <typename T>
size_t Resample_Volume(T* src, const SoundInfo &info, size_t src_frame_size, double volume)
{
  for (size_t i = 0; i < src_frame_size * info.channels; ++i)
  {
    src[i] = (T)(src[i] * volume);
  }
  return src_frame_size;
}

// shortcut function
template <typename T>
size_t Resample_Volume(Sound &s, double volume)
{
  return Resample_Volume((T*)s.get_ptr(), s.get_soundinfo(), s.get_frame_count(), volume);
}

// ----------------------------- class Effector

Effector::Effector()
  : tempo_(1.0), pitch_(1.0), volume_(1.0) {}

void Effector::SetTempo(double tempo)
{
  tempo_ = tempo;
}

void Effector::SetPitch(double pitch)
{
  pitch_ = pitch;
}

void Effector::SetPitchConsistTempo(double length)
{
  tempo_ = length;
  pitch_ = length;
}

void Effector::SetVolume(double volume)
{
  volume_ = volume;
}

bool Effector::Resample(Sound &newsound)
{
  const SoundInfo &info = newsound.get_soundinfo();

  // volume
  if (volume_ != 1.0)
  {
    if (info.is_signed == 0)
    {
      switch (info.bitsize)
      {
      case 8:
        Resample_Volume<uint8_t>(newsound, volume_);
        break;
      case 16:
        Resample_Volume<uint16_t>(newsound, volume_);
        break;
      case 32:
        Resample_Volume<uint32_t>(newsound, volume_);
        break;
      default:
        return false;
      }
    }
    else if (info.is_signed == 1)
    {
      switch (info.bitsize)
      {
      case 8:
        Resample_Volume<int8_t>(newsound, volume_);
        break;
      case 16:
        Resample_Volume<int16_t>(newsound, volume_);
        break;
      case 32:
        Resample_Volume<int32_t>(newsound, volume_);
        break;
      default:
        return false;
      }
    }
    else if (info.is_signed == 2)
    {
      switch (info.bitsize)
      {
      case 32:
        Resample_Volume<float>(newsound, volume_);
        break;
      case 64:
        Resample_Volume<double>(newsound, volume_);
        break;
      default:
        return false;
      }
    }
    else return false;
  }

  // pitch modification
  if (pitch_ != 1.0)
  {
    Sound s;
    if (info.is_signed == 0)
    {
      switch (info.bitsize)
      {
      case 8:
        Resample_Pitch<uint8_t>(s, newsound, pitch_);
        break;
      case 16:
        Resample_Pitch<uint16_t>(s, newsound, pitch_);
        break;
      case 32:
        Resample_Pitch<uint32_t>(s, newsound, pitch_);
        break;
      default:
        return false;
      }
    }
    else if (info.is_signed == 1)
    {
      switch (info.bitsize)
      {
      case 8:
        Resample_Pitch<int8_t>(s, newsound, pitch_);
        break;
      case 16:
        Resample_Pitch<int16_t>(s, newsound, pitch_);
        break;
      case 32:
        Resample_Pitch<int32_t>(s, newsound, pitch_);
        break;
      default:
        return false;
      }
    }
    else if (info.is_signed == 2)
    {
      switch (info.bitsize)
      {
      case 32:
        Resample_Pitch<float>(s, newsound, pitch_);
        break;
      case 64:
        Resample_Pitch<double>(s, newsound, pitch_);
        break;
      default:
        return false;
      }
    }
    else return false;
    newsound.swap(s);
  }

  // tempo modification (SOLA method)
  if (tempo_ != 1.0)
  {
    Sound s;
    if (info.is_signed == 0)
    {
      switch (info.bitsize)
      {
      case 8:
        Resample_Pitch<uint8_t>(s, newsound, pitch_);
        break;
      case 16:
        Resample_Tempo<uint16_t>(s, newsound, tempo_);
        break;
      case 32:
        Resample_Tempo<uint32_t>(s, newsound, tempo_);
        break;
      default:
        return false;
      }
    }
    else if (info.is_signed == 1)
    {
      switch (info.bitsize)
      {
      case 8:
        Resample_Pitch<int8_t>(s, newsound, pitch_);
        break;
      case 16:
        Resample_Tempo<int16_t>(s, newsound, tempo_);
        break;
      case 32:
        Resample_Tempo<int32_t>(s, newsound, tempo_);
        break;
      default:
        return false;
      }
    }
    else if (info.is_signed == 2)
    {
      switch (info.bitsize)
      {
      case 32:
        Resample_Tempo<float>(s, newsound, tempo_);
        break;
      case 64:
        Resample_Tempo<double>(s, newsound, tempo_);
        break;
      default:
        return false;
      }
    }
    else return false;
    newsound.swap(s);
  }

  return true;
}

}