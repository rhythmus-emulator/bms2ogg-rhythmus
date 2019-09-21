#ifndef RMIXER_SAMPLER_H
#define RMIXER_SAMPLER_H

#include "Sound.h"

namespace rmixer
{

class Sampler
{
public:
  Sampler();
  Sampler(const PCMBuffer& source);
  Sampler(const PCMBuffer& source, const SoundInfo& target_quality);
  ~Sampler();

  void SetSource(const PCMBuffer& source);
  void SetTargetQuality(const SoundInfo& target_info);

  void SetTempo(double tempo);
  void SetPitch(double pitch);
  void SetPitchConsistTempo(double pitch);
  void SetVolume(double volume);

  bool Resample(PCMBuffer &newsound);
private:
  double tempo_;
  double pitch_;
  double volume_;
  const PCMBuffer *source_;
  SoundInfo target_info_;
};

}

#endif