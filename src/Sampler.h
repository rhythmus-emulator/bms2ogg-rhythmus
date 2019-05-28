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
  void SetLength(double length);
  void SetVolume(double volume);

  bool Resample(Sound &newsound);
private:
  void SampleRateConversion(int8_t* source, int8_t* target, const SoundInfo& from, const SoundInfo& to);

  double tempo_;
  double pitch_;
  double volume_;
  const Sound *source_;
  SoundInfo info_;
};

}

#endif