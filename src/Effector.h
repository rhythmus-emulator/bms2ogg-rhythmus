#ifndef RMIXER_EFFECTOR_H
#define RMIXER_EFFECTOR_H

#include <stdint.h>

namespace rmixer
{

class Sound;

class Effector
{
public:
  Effector(const Sound *s);

  void SetSource(const Sound *s);
  void SetTempo(double tempo);
  void SetPitch(double pitch);
  void SetPitchConsistTempo(double pitch);
  void SetVolume(double volume);

  bool Resample(Sound &newsound);

private:
  const Sound *sound_;
  double tempo_;
  double pitch_;
  double volume_;
};

void PitchEffectorS8(int8_t *dst, const int8_t *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);
void PitchEffectorS16(int16_t *dst, const int16_t *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);
void PitchEffectorS32(int32_t *dst, const int32_t *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);
void PitchEffectorU8(uint8_t *dst, const uint8_t *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);
void PitchEffectorU16(uint16_t *dst, const uint16_t *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);
void PitchEffectorU32(uint32_t *dst, const uint32_t *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);
void PitchEffectorF32(float *dst, const float *src, size_t frame_size_src, size_t frame_size_dst, size_t ch_cnt);


}

#endif