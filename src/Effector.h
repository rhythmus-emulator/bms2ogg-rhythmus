#ifndef RMIXER_EFFECTOR_H
#define RMIXER_EFFECTOR_H

#include <stdint.h>
#include <stddef.h>

namespace rmixer
{

class Sound;
struct SoundInfo;

class Effector
{
public:
  Effector();

  void SetTempo(double tempo);
  void SetPitch(double pitch);
  void SetPitchConsistTempo(double pitch);
  void SetVolume(double volume);

  bool Resample(Sound &s);

private:
  double tempo_;
  double pitch_;
  double volume_;
};

template <typename T>
size_t Resample_Pitch(T **dst, const T *src, const SoundInfo& info, size_t frame_size_src, double pitch);

template <typename T>
size_t Resample_Volume(T* src, const SoundInfo &info, size_t src_frame_size, double volume);

template <typename T>
size_t Resample_Tempo(T **dst, const T *src, const SoundInfo &info, size_t src_framecount, double length);

}

#endif
