#ifndef RMIXER_SOUND_H
#define RMIXER_SOUND_H

#include <stdint.h>
#include <vector>

namespace rmixer
{

/**
 * @brief describing PCM sound info
 */
typedef struct
{
  uint16_t bitsize;   /* bits per sample (value is set as unsigned by default) */
  uint8_t channels;   /* 1 : mono, 2 : stereo. */
  uint32_t rate;      /* sample rate (Hz) */
} SoundInfo;

uint32_t GetByteFromFrame(uint32_t frame, const SoundInfo& info);
uint32_t GetByteFromMilisecond(uint32_t ms, const SoundInfo& sinfo);
uint32_t GetFrameFromMilisecond(uint32_t ms, const SoundInfo& sinfo);
uint32_t GetFrameFromByte(uint32_t byte, const SoundInfo& sinfo);
uint32_t GetMilisecondFromByte(uint32_t byte, const SoundInfo& sinfo);

bool operator==(const SoundInfo& a, const SoundInfo& b);
bool operator!=(const SoundInfo& a, const SoundInfo& b);


/**
 * @brief interface class containing PCM data
 */
class PCMBuffer
{
public:
  PCMBuffer();
  PCMBuffer(const SoundInfo& info, size_t buffer_size);
  virtual ~PCMBuffer();
  virtual size_t Mix(size_t ms, const PCMBuffer& s, float volume = 1.0f) = 0;
  virtual size_t MixData(int8_t* copy_to, size_t src_offset, size_t desired_byte, bool copy = false, float volume = 1.0f) const = 0;
  virtual void Clear() = 0;

  size_t GetFrameCount() const;
  const SoundInfo& get_info() const;
  size_t get_total_byte() const;
  bool IsEmpty() const;
  virtual int8_t* AccessData(size_t byte_offset, size_t* remaining_byte = 0);
  const int8_t* AccessData(size_t byte_offset, size_t* remaining_byte = 0) const;
protected:
  SoundInfo info_;
  size_t buffer_size_;  /* buffer size in byte */
};

/**
 * @description
 * PCM data with single buffer
 */
class Sound : public PCMBuffer
{
public:
  Sound();
  Sound(const SoundInfo& info, size_t framecount);
  Sound(const SoundInfo& info, size_t framecount, void *p);
  Sound(const Sound &s) = delete;
  Sound(Sound &&s);
  Sound& operator=(const Sound &s) = delete;
  Sound& operator=(Sound &&s);
  virtual ~Sound();

  using PCMBuffer::AccessData;

  virtual size_t Mix(size_t ms, const PCMBuffer& s, float volume = 1.0f);
  virtual size_t MixData(int8_t* copy_to, size_t src_offset, size_t desired_byte, bool copy = false, float volume = 1.0f) const;
  virtual int8_t* AccessData(size_t byte_offset, size_t* remaining_byte = 0);
  virtual void Clear();

  void Set(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate, void* p = 0);
  int8_t* ptr();
  const int8_t* ptr() const;

private:
  int8_t *buffer_;
};

/**
 * @description
 * PCM data with multiple buffer, adapted with varying size
 */
class SoundVariableBuffer : public PCMBuffer
{
public:
  SoundVariableBuffer(const SoundInfo& info, size_t chunk_byte_size = 1024*1024);
  SoundVariableBuffer(const SoundVariableBuffer &s) = delete;
  SoundVariableBuffer(SoundVariableBuffer &&s) = delete;
  SoundVariableBuffer& operator=(const SoundVariableBuffer &s) = delete;
  SoundVariableBuffer& operator=(SoundVariableBuffer &&s);
  virtual ~SoundVariableBuffer();

  using PCMBuffer::AccessData;

  virtual size_t Mix(size_t ms, const PCMBuffer& s, float volume = 1.0f);
  virtual size_t MixData(int8_t* copy_to, size_t src_offset, size_t desired_byte, bool copy = false, float volume = 1.0f) const;
  virtual int8_t* AccessData(size_t byte_offset, size_t* remaining_byte = 0);
  virtual void Clear();

  int8_t* get_chunk(int idx);
  const int8_t* get_chunk(int idx) const;
private:
  void PrepareByteoffset(uint32_t offset);

  std::vector<int8_t*> buffers_;
  size_t chunk_byte_size_;
  size_t frame_count_in_chunk_;
};

void memmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample, float src_volume);
void memmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample);
void SoundVariableBufferToSoundBuffer(SoundVariableBuffer &in, Sound &out);

}

#endif