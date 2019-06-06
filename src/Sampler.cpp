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
bool Resample_Internal(const Sound &source, Sound &newsound, const SoundInfo& newinfo, double pitch)
{
  ASSERT(sizeof(T_TO) == 2 || sizeof(T_TO) == 4);

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
  const size_t new_framecount = framecount * sample_rate;

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
  newsound.Set(newinfo.bitsize, newinfo.channels, new_framecount, newinfo.rate, new_sound_mod_ptr);

  return true;
}

bool Sampler::Resample(Sound &newsound)
{
  switch (info_.bitsize)
  {
  case 16:
    return Resample_Internal<int16_t>(*source_, newsound, info_, pitch_);
  case 32:
    return Resample_Internal<int32_t>(*source_, newsound, info_, pitch_);
  }

  // tempo modification (SOLA method)
  if (tempo_ != 1.0)
  {
    //new_sound_ptr = (int8_t*)malloc(info_.bitsize * info_.channels * source->GetFrameCount());
    // TODO
    ASSERT(0);
  }

  return false;
}


}