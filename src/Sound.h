#ifndef RENCODER_SOUND_H
#define RENCODER_SOUND_H

#include <stdint.h>
#include <vector>

namespace rhythmus
{

typedef struct
{
  uint16_t bitsize; /* bits per sample (value is set as unsigned by default) */
  uint8_t channels; /* 1 : mono, 2 : stereo. */
  uint32_t rate;    /* sample rate (Hz) */
} SoundInfo;

/**
 * @description
 * PCM data which is read-only (not reallocable)
 */
class Sound
{
public:
  Sound();
  ~Sound();
  void Set(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate);
  size_t GetFrameCount() const;
  void Clear();
  int8_t* ptr();
private:
  int8_t *buffer_;
  size_t buffer_size_;
  SoundInfo info_;
};

/**
 * @description
 * A Sound buffer of which size is not fixed (mainly for mixing/writing)
 */
class SoundMixer
{
public:
  SoundMixer();
  ~SoundMixer();
private:
  std::vector<uint8_t*> buffers_;
  size_t total_size_;
  size_t chunk_size_;
  SoundInfo info_;
};

}

#endif