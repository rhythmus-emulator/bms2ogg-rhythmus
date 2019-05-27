#include "Sampler.h"

namespace rhythmus
{

Sampler::Sampler(const Sound& source)
: source_(&source), tempo_(1.0), pitch_(1.0), volume_(1.0) {}

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

bool Sampler::Resample(Sound &newsound)
{
  // TODO
  return false;
}

}