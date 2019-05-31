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
  void Set(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate, void* p = 0);
  size_t GetFrameCount() const;
  void Clear();
  int8_t* ptr();
  const int8_t* ptr() const;
  const SoundInfo& get_info() const;
  const size_t buffer_byte_size() const;

private:
  int8_t *buffer_;
  size_t buffer_size_;  /* buffer size in bit */
  SoundInfo info_;
};

/**
 * @description
 * A Sound buffer of which size is not fixed (mainly for mixing/writing)
 */
class SoundMixer
{
public:
  SoundMixer(size_t chunk_size = 1024*1024);
  ~SoundMixer();
  void Mix(Sound& s, uint32_t ms);
  void SetInfo(const SoundInfo& info);
  const SoundInfo& get_info() const;
  size_t get_chunk_size() const;
  size_t get_chunk_count() const;
  int8_t* get_chunk(size_t idx);
  const int8_t* get_chunk(size_t idx) const;
  size_t get_total_size() const;
  void Clear();

private:
  uint32_t Time2Byteoffset(double ms);
  void PrepareByteoffset(uint32_t offset);
  void MixToChunk(int8_t *source, size_t source_size, int8_t *dest);

  std::vector<int8_t*> buffers_;
  size_t total_size_;
  size_t chunk_size_;
  size_t sample_count_in_chunk_;
  SoundInfo info_;
};

}

#endif