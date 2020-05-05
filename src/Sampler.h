#ifndef RMIXER_SAMPLER_H
#define RMIXER_SAMPLER_H

#include "Sound.h"

namespace rmixer
{

class Sampler
{
public:
  Sampler();
  Sampler(const Sound& source);
  Sampler(const Sound& source, const SoundInfo& target_quality);
  ~Sampler();

  void SetSource(const Sound& source);
  void SetTargetQuality(const SoundInfo& target_info);

  bool Resample(Sound &newsound);
private:
  const Sound *source_;
  SoundInfo target_info_;
};

// Utility resampler function
bool Resample(Sound &dst, const Sound &src, const SoundInfo &target_info);

}

#endif