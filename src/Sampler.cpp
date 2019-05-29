#include "Sampler.h"
#include "Error.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

namespace rhythmus
{

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

void Sampler::SetLength(double length)
{
  tempo_ = 1.0 / length;
  pitch_ = length;
}

void Sampler::SetVolume(double volume)
{
  volume_ = volume;
}

void Sampler::SampleRateConversion(int8_t* source, int8_t* target, const SoundInfo& from, const SoundInfo& to)
{

}

inline void Resample_u4_to_s16(const Sound &source, int16_t* p)
{
  const size_t framecnt = source.GetFrameCount();
  const int8_t* p_source = source.ptr();
  for (size_t i = 0; i < framecnt; i++)
  {
    *p = (*p_source & 0b00001111) << 12;
    // if not last frame ...
    if (framecnt - i > 1) *(p+1) = (*p_source & 0b11110000) << 8;
    p += 2;
    p_source++;
  }
}

inline void Resample_u4_to_s32(const Sound &source, int32_t* p)
{
  const size_t framecnt = source.GetFrameCount();
  const int8_t* p_source = source.ptr();
  for (size_t i = 0; i < framecnt; i++)
  {
    *p = (*p_source & 0b00001111) << 28;
    // if not last frame ...
    if (framecnt - i > 1) *(p + 1) = (*p_source & 0b11110000) << 24;
    p += 2;
  }
}

template<typename T_TO>
bool Resample_Internal(const Sound &source, Sound &newsound, const SoundInfo& newinfo)
{
  ASSERT(sizeof(T_TO) == 16 || sizeof(T_TO) == 32);
  bool is_PCM_conversion_necessary = memcmp(&newinfo, &source.get_info(), sizeof(SoundInfo)) == 0;

  const T_TO* new_sound_ref_ptr = 0;
  T_TO* new_sound_mod_ptr = 0, *mod_ptr = 0;

  // 1. do byte conversion(PCM conversion), if necessary.
  bool is_bitsize_diff = source.get_info().bitsize != newinfo.bitsize;
  bool is_channelsize_diff = source.get_info().channels != newinfo.channels;
  if (is_bitsize_diff)
  {
    const size_t framecnt = source.GetFrameCount();
    mod_ptr = (T_TO*)malloc(sizeof(T_TO) * framecnt * source.get_info().channels);
    // check for 4 bit
    if (source.get_info().bitsize == 4)
    {
      if (sizeof(T_TO) == 16)
        Resample_u4_to_s16(source, (int16_t*)mod_ptr);
      else
        Resample_u4_to_s32(source, (int32_t*)mod_ptr);
    } else
    {
      if (sizeof(T_TO) == 16)
        drwav__pcm_to_s16((int16_t*)mod_ptr, (uint8_t*)new_sound_ref_ptr, source.GetFrameCount(), source.get_info().bitsize / 8);
      else
        drwav__pcm_to_s32((int32_t*)mod_ptr, (uint8_t*)new_sound_ref_ptr, source.GetFrameCount(), source.get_info().bitsize / 8);
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
    p = mod_ptr = (T_TO*)malloc(sizeof(T_TO) * source.GetFrameCount() * source.get_info().channels);
    for (size_t i = 0; i < source.GetFrameCount(); i++)
    {
      downsample = 0;
      for (size_t ch = 0; ch < source.get_info().channels; ch++)
        downsample += *(new_sound_ref_ptr++) / source.get_info().channels;
      for (size_t ch = 0; ch < newinfo.channels; ch++)
        (*p++) = downsample;
    }
    // replace previous cache
    if (new_sound_mod_ptr) free(new_sound_mod_ptr);
    new_sound_ref_ptr = new_sound_mod_ptr = mod_ptr;
  }

  // 3. do Sampling conversion
  // (may be a little coarse when downsampling it only refers two samples at most)
  const size_t new_framecount = newinfo.channels * newinfo.rate;
  if (source.get_info().rate != newinfo.rate)
  {
    mod_ptr = reinterpret_cast<T_TO*>(const_cast<int8_t*>(newsound.ptr()));
    double sample_rate = (double)newinfo.rate / source.get_info().rate;
    double a, b, original_sample_idx = 0;
    long sample_idx_int = 0;
    for (size_t i = 0; i < new_framecount; ++i)
    {
      original_sample_idx += sample_rate;
      sample_idx_int = static_cast<long>(original_sample_idx);
      a = original_sample_idx - sample_idx_int;
      b = 1.0 - a;
      mod_ptr[i] = static_cast<T_TO>(new_sound_ref_ptr[sample_idx_int] * a + new_sound_ref_ptr[sample_idx_int + 1] * b);
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
  newsound.Set(newinfo.bitsize, newinfo.channels, new_framecount, newinfo.rate, new_sound_mod_ptr);

  return false;
}

bool Sampler::Resample(Sound &newsound)
{
  switch (info_.bitsize)
  {
  case 16:
    return Resample_Internal<int16_t>(*source_, newsound, info_);
  case 32:
    return Resample_Internal<int32_t>(*source_, newsound, info_);
  }

  // tempo / pitch modification
  if (tempo_ != 1.0 || pitch_ != 1.0)
  {
    //new_sound_ptr = (int8_t*)malloc(info_.bitsize * info_.channels * source->GetFrameCount());
    // TODO
    ASSERT(0);
  }

  return false;
}


}