#ifndef RMIXER_SAMPLER_H
#define RMIXER_SAMPLER_H

#include "Sound.h"

namespace rmixer
{

class Sampler
{
public:
  Sampler(const Sound& source);
  Sampler(const Sound& source, const SoundInfo& target_quality);
  ~Sampler();
  void SetTempo(double tempo);
  void SetPitch(double pitch);
  void SetPitchConsistTempo(double pitch);
  void SetVolume(double volume);

  bool Resample(Sound &newsound);
private:
  double tempo_;
  double pitch_;
  double volume_;
  const Sound *source_;
  SoundInfo info_;
};

}

#endif