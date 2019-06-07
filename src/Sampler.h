#ifndef RENCODER_SAMPLER_H
#define RENCODER_SAMPLER_H

#include "Sound.h"

namespace rhythmus
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