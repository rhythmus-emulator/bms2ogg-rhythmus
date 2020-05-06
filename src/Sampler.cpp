#include "Sampler.h"
#include "Error.h"

// internally calls pitch effector while sample rate conversion.
#include "Effector.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"


#include <math.h>

#if 0
// script for byte-to-floats
private static float[] bytesToFloats(byte[] bytes) {
  float[] floats = new float[bytes.length / 2];
  for (int i = 0; i < bytes.length; i += 2) {
    floats[i / 2] = bytes[i] | (bytes[i + 1] << 8);
  }
  return floats;
}
#endif

namespace rmixer
{

Sampler::Sampler() : source_(nullptr) {}

Sampler::Sampler(const Sound& source) : source_(&source) {}

Sampler::Sampler(const Sound& source, const SoundInfo& target_quality)
  : source_(&source), target_info_(target_quality) {}

Sampler::~Sampler() {}

void Sampler::SetSource(const Sound& source)
{
  source_ = &source;
}

void Sampler::SetTargetQuality(const SoundInfo& target_info)
{
  target_info_ = target_info;
}

template <typename T>
void Resample_from_u4_2_int(const Sound &source, T* p)
{
  const size_t samplecnt = source.get_sample_count();
  const int8_t* p_source = source.get_ptr();
  for (size_t i = 0; i < samplecnt; i++)
  {
    *p = (*p_source & 0b00001111) << (sizeof(T) * 8 - 4);
    // if not last frame ...
    if (samplecnt - i > 1) *(p + 1) = (*p_source & 0b11110000) << (sizeof(T) * 8 - 8);
    p += 2;
  }
}

template <typename T>
void Resample_from_u4_2_float(const Sound &source, T* p)
{
  const size_t samplecnt = source.get_sample_count();
  const int8_t* p_source = source.get_ptr();
  for (size_t i = 0; i < samplecnt; i++)
  {
    *p = (*p_source & 0b00001111) - 8 / 16.0f;
    // if not last frame ...
    if (samplecnt - i > 1) *(p + 1) = (*p_source & 0b11110000) - 128 / 256.0f;
    p += 2;
  }
}

template <typename T> void Resample_from_u4(const Sound &source, T* p);

template <> void Resample_from_u4(const Sound &source, uint8_t* p)
{
  Resample_from_u4_2_int(source, p);
}

template <> void Resample_from_u4(const Sound &source, uint16_t* p)
{
  Resample_from_u4_2_int(source, p);
}

template <> void Resample_from_u4(const Sound &source, uint32_t* p)
{
  Resample_from_u4_2_int(source, p);
}

template <> void Resample_from_u4(const Sound &source, int8_t* p)
{
  Resample_from_u4_2_int(source, p);
}

template <> void Resample_from_u4(const Sound &source, int16_t* p)
{
  Resample_from_u4_2_int(source, p);
}

template <> void Resample_from_u4(const Sound &source, int32_t* p)
{
  Resample_from_u4_2_int(source, p);
}

template <> void Resample_from_u4(const Sound &source, float* p)
{
  Resample_from_u4_2_float(source, p);
}

template <> void Resample_from_u4(const Sound &source, double* p)
{
  Resample_from_u4_2_float(source, p);
}

static void u8_to_s8(uint8_t *src, size_t sample_size)
{
  for (size_t i = 0; i < sample_size; ++i)
    ((int8_t*)src)[i] = src[i] - 0x80;
}

static void u16_to_s16(uint16_t *src, size_t sample_size)
{
  for (size_t i = 0; i < sample_size; ++i)
    ((int16_t*)src)[i] = src[i] - 0x8000;
}

static void u32_to_s32(uint32_t *src, size_t sample_size)
{
  for (size_t i = 0; i < sample_size; ++i)
    ((int32_t*)src)[i] = src[i] - 0x80000000;
}

static void s8_to_u8(int8_t *src, size_t sample_size)
{
  for (size_t i = 0; i < sample_size; ++i)
    ((uint8_t*)src)[i] = src[i] + 0x80;
}

static void s16_to_u16(int16_t *src, size_t sample_size)
{
  for (size_t i = 0; i < sample_size; ++i)
    ((uint16_t*)src)[i] = src[i] + 0x8000;
}

static void s32_to_u32(int32_t *src, size_t sample_size)
{
  for (size_t i = 0; i < sample_size; ++i)
    ((uint32_t*)src)[i] = src[i] + 0x80000000;
}


// XXX:
// Performance problem when signed/unsigned is changed.
// Need to create new sign/unsigned conversion API
// independent from drwav library.
template <typename T_TO>
void Resample_Internal(const Sound &source, Sound &newsound, const SoundInfo& newinfo)
{
  RMIXER_ASSERT(sizeof(T_TO) == 2 || sizeof(T_TO) == 4);
  RMIXER_ASSERT(&source != &newsound);

  T_TO *prev_ptr = 0, *new_ptr = 0;
  const SoundInfo &sinfo = source.get_soundinfo();
  size_t framecount = source.get_frame_count();
  new_ptr = const_cast<T_TO*>(reinterpret_cast<const T_TO*>(source.get_ptr()));

  // 1. do byte conversion(PCM conversion), if necessary.
  bool is_bitsize_diff = (sinfo.bitsize != newinfo.bitsize) || (sinfo.is_signed != newinfo.is_signed);
  bool is_channelsize_diff = sinfo.channels != newinfo.channels;
  if (is_bitsize_diff)
  {
    T_TO *p = (T_TO*)malloc(sizeof(T_TO) * framecount * sinfo.channels);
    if (sinfo.bitsize == 4)
    {
      if (sinfo.is_signed == 1)
        RMIXER_THROW("Unsupported bit size");

      // check for 4 bit PCM (XXX: is this really exists?)
      Resample_from_u4(source, p);
    }
    else
    {
      unsigned is_signed = sinfo.is_signed;
      // signed 8bit is not supported currently!
      if (sinfo.bitsize == 8)
      {
        if (is_signed == 1)
        {
          RMIXER_THROW("Unsupported bit size");
        }
        else
        {
          /* Trick: actually source is unsigned, but output is signed,
           * so change sign flag to match property below. */
          is_signed = 1;
        }
      }

      if (sinfo.is_signed == 0 || sinfo.is_signed == 1)
      {
        if (newinfo.is_signed <= 2 && sizeof(T_TO) == 2 /* S16 */)
        {
          drwav__pcm_to_s16((int16_t*)p, (uint8_t*)source.get_ptr(), framecount * sinfo.channels, sinfo.bitsize / 8);
          /* unsigned-to-unsigned is safe */
          if (is_signed == 0 && newinfo.is_signed == 1)
            u16_to_s16((uint16_t*)p, framecount * sinfo.channels);
          if (is_signed == 1 && newinfo.is_signed == 0)
            s16_to_u16((int16_t*)p, framecount * sinfo.channels);
        }
        else if (newinfo.is_signed <= 2 && sizeof(T_TO) == 4 /* S32 */)
        {
          drwav__pcm_to_s32((int32_t*)p, (uint8_t*)source.get_ptr(), framecount * sinfo.channels, sinfo.bitsize / 8);
          /* unsigned-to-unsigned is safe */
          if (is_signed == 0 && newinfo.is_signed == 1)
            u32_to_s32((uint32_t*)p, framecount * sinfo.channels);
          if (is_signed == 1 && newinfo.is_signed == 0)
            s32_to_u32((int32_t*)p, framecount * sinfo.channels);
        }
        else if (newinfo.is_signed == 2 && sizeof(T_TO) == 4 /* F32 */)
        {
          drwav__pcm_to_f32((float*)p, (uint8_t*)source.get_ptr(), framecount * sinfo.channels, sinfo.bitsize / 8);
          if (is_signed == 0) /* in case of source is unsigned */
          {
            float *_p = (float*)p;
            for (size_t i = 0; i < framecount * sinfo.channels; ++i)
              _p[i] -= 1.0f;
          }
        }
        else
        {
          RMIXER_THROW("Unsupported bit size");
        }
      }
      else if (sinfo.is_signed == 2)
      {
        if (newinfo.is_signed <= 2 && sizeof(T_TO) == 2 /* S16 */)
        {
          drwav__ieee_to_s16((int16_t*)p, (uint8_t*)source.get_ptr(), framecount * sinfo.channels, sinfo.bitsize / 8);
          if (newinfo.is_signed == 0)
            s16_to_u16((int16_t*)p, framecount * sinfo.channels);
        }
        else if (newinfo.is_signed <= 2 && sizeof(T_TO) == 4 /* S32 */)
        {
          drwav__ieee_to_s32((int32_t*)p, (uint8_t*)source.get_ptr(), framecount * sinfo.channels, sinfo.bitsize / 8);
          if (newinfo.is_signed == 0)
            s32_to_u32((int32_t*)p, framecount * sinfo.channels);
        }
        else if (newinfo.is_signed == 2 && sizeof(T_TO) == 4 /* F32 */)
        {
          RMIXER_ASSERT(0); /* should not happen */
        }
        else
        {
          RMIXER_THROW("Unsupported bit size");
        }
      }
      else
      {
        RMIXER_THROW("Unsupported PCM type.");
      }
    }
    // replace previous cache
    new_ptr = p;
    prev_ptr = new_ptr;
  }

  // 2. channel conversion
  // downsample into single channel and averaging method
  if (is_channelsize_diff)
  {
    T_TO downsample;
    T_TO *p = (T_TO*)malloc(sizeof(T_TO) * framecount * newinfo.channels);
    T_TO *p_org = p;
    for (size_t i = 0; i < framecount; i++)
    {
      downsample = 0;
      for (size_t ch = 0; ch < sinfo.channels; ch++)
        downsample += *(new_ptr++) / sinfo.channels;
      for (size_t ch = 0; ch < newinfo.channels; ch++)
        *(p++) = downsample;
    }
    // replace previous cache
    if (prev_ptr) { free(prev_ptr); }
    new_ptr = p_org;
    prev_ptr = new_ptr;
  }

  double sample_rate = (double)newinfo.rate / sinfo.rate;

  // 3. sample rate conversion
  if (sample_rate != 1.0)
  {
    const size_t new_framecount = static_cast<size_t>(framecount * sample_rate);
    T_TO *p = (T_TO*)malloc(sizeof(T_TO) * new_framecount * newinfo.channels);
    if (newinfo.is_signed == 0)
    {
      switch (newinfo.bitsize)
      {
      case 8:
        PitchEffectorU8((uint8_t*)p, (uint8_t*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      case 16:
        PitchEffectorU16((uint16_t*)p, (uint16_t*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      case 32:
        PitchEffectorU32((uint32_t*)p, (uint32_t*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      default:
        RMIXER_THROW("Unsupported PCM bitsize.");
      }
    }
    else if (newinfo.is_signed == 1)
    {
      switch (newinfo.bitsize)
      {
      case 8:
        PitchEffectorS8((int8_t*)p, (int8_t*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      case 16:
        PitchEffectorS16((int16_t*)p, (int16_t*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      case 32:
        PitchEffectorS32((int32_t*)p, (int32_t*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      default:
        RMIXER_THROW("Unsupported PCM bitsize.");
      }
    }
    else if (newinfo.is_signed == 2)
    {
      switch (newinfo.bitsize)
      {
      case 32:
        PitchEffectorF32((float*)p, (float*)new_ptr, framecount, new_framecount, newinfo.channels);
        break;
      default:
        RMIXER_THROW("Unsupported PCM bitsize.");
      }
    }
    else
    {
      RMIXER_THROW("Unsupported PCM type.");
    }
    // replace previous cache
    if (prev_ptr) { free(prev_ptr); }
    new_ptr = p;
    prev_ptr = new_ptr;
    framecount = new_framecount;
  }

  // if no conversion is done, then just clone memory.
  if (!prev_ptr)
  {
    size_t s = sizeof(T_TO) * source.get_frame_count() * sinfo.channels;
    new_ptr = (T_TO*)malloc(s);
    memcpy(new_ptr, source.get_ptr(), s);
  }

  // set new PCM data to output sound.
  newsound.SetBuffer(newinfo, framecount, new_ptr);
}


bool Sampler::Resample(Sound &newsound)
{
  if (!source_)
    return false;

  if (!newsound.is_empty() && newsound.get_soundinfo() == target_info_)
    return true;

  switch (target_info_.is_signed)
  {
  case 0:
    switch (target_info_.bitsize)
    {
    case 8:
      Resample_Internal<int8_t>(*source_, newsound, target_info_);
      break;
    case 16:
      Resample_Internal<int16_t>(*source_, newsound, target_info_);
      break;
    case 32:
      Resample_Internal<int32_t>(*source_, newsound, target_info_);
      break;
    default:
      return false;
    }
    break;
  case 1:
    switch (target_info_.bitsize)
    {
    case 8:
      Resample_Internal<uint8_t>(*source_, newsound, target_info_);
      break;
    case 16:
      Resample_Internal<uint16_t>(*source_, newsound, target_info_);
      break;
    case 32:
      Resample_Internal<uint32_t>(*source_, newsound, target_info_);
      break;
    default:
      return false;
    }
    break;
  case 2:
    switch (target_info_.bitsize)
    {
    case 32:
      Resample_Internal<float>(*source_, newsound, target_info_);
      break;
    case 64:
      Resample_Internal<double>(*source_, newsound, target_info_);
      break;
    default:
      return false;
    }
    break;
  }

  return true;
}

bool Resample(Sound &dst, const Sound &src, const SoundInfo &target_info)
{
  Sampler sampler(src, target_info);
  return sampler.Resample(dst);
}

}
