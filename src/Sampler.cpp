#include "Sampler.h"
#include "Error.h"

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

bool Sampler::Resample(Sound &newsound)
{
  return false;
}

#if 0
template<typename T_FROM, typename T_TO>
bool Sampler::Resample(Sound &newsound)
{
  bool is_PCM_conversion_necessary = memcmp(&info_, &source_->get_info(), sizeof(SoundInfo)) == 0;

  const T_TO* new_sound_ref_ptr = 0;
  T_TO* new_sound_mod_ptr = 0, *mod_ptr = 0;

  // 1. do byte conversion(PCM conversion), if necessary.
  bool is_bitsize_diff = source_->get_info().bitsize != info_.bitsize;
  bool is_channelsize_diff = source_->get_info().channels != info_.channels;
  if (is_bitsize_diff || is_channelsize_diff)
  {
    if (source_->get_info().channels > 2)  // won't going to support converison from multiple channel
      return false; // TODO: more elegant, not memory loss way

    mod_ptr = static_cast<T_TO*>(malloc(info_.bitsize * info_.channels * source_->GetFrameCount()));
    size_t new_framecount = static_cast<size_t>(source_->GetFrameCount() * info_.channels);
    size_t i_new = 0, i_source = 0;
    while (i_new < new_framecount)
    {
      if (is_bitsize_diff)
      {
        mod_ptr[i_new] = static_cast<T_TO>(
          (double)static_cast<T_FROM*>(source_->ptr())[i_source] * std::numeric_limits(T_FROM)::max() / std::numeric_limits(T_TO)::max()
          );
      } else
      {
        mod_ptr[i_new] = static_cast<T_FROM*>(source_->ptr()[i_source]);
      }
      if (is_channelsize_diff)
      {
        for (int k = 1; k < info_.channels; k++)
          mod_ptr[i_new+1] = mod_ptr[i_new++];
      }
      i_new++;
    }
    new_sound_ref_ptr = new_sound_mod_ptr = mod_ptr;
  } else
  {
    // should decltype(T_TO) == decltype(T_FROM)
    new_sound_ref_ptr = static_cast<T_FROM*>(source_->ptr())
  }

  // 2. get new sample from previous source
  if (tempo_ != 1.0 || pitch_ != 1.0)
  {
    new_sound_ptr = (int8_t*)malloc(info_.bitsize * info_.channels * source_->GetFrameCount());
    // TODO
    ASSERT(0);
  }
  // TODO

  // 3. do Sampling conversion
  // (may be a little coarse, bad quality as it only refers two samples at most)
  if (source_->get_info().rate != info_.rate)
  {
    mod_ptr = static_cast<T_TO*>(const_cast<int8_t*>(newsound.ptr()));
    double sample_rate = (double)info_.rate / source_->get_info().rate;
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
  newsound.Set(info_.bitsize, info_.channels, new_framecount, info_.rate);
  // TODO

  return false;
}
#endif

}