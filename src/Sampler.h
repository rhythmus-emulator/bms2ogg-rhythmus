#ifndef RENCODER_SAMPLER_H
#define RENCODER_SAMPLER_H

#include "Sound.h"

namespace rhythmus
{

class Sampler
{
public:
  Sampler(const Sound& source);
  ~Sampler();
  void SetTempo(double tempo);
  void SetPitch(double pitch);
  void SetLength(double length);
  void SetVolume(double volume);
  bool Resample(Sound &newsound);
private:
  double tempo_;
  double pitch_;
  double volume_;
  const Sound *source_;
};

}

#endif