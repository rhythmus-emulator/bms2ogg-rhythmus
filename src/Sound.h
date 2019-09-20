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
  PCMBuffer(const SoundInfo& info, size_t buffer_size, int8_t *p);
  virtual ~PCMBuffer();

  void AllocateSize(const SoundInfo& info, size_t buffer_size);
  void AllocateDuration(const SoundInfo& info, uint32_t duration_ms);
  void SetBuffer(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate, void* );
  void SetBuffer(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate);
  void Clear();

  size_t GetFrameCount() const;
  uint32_t GetDurationInMilisecond() const;
  const SoundInfo& get_info() const;
  size_t get_total_byte() const;
  int8_t* get_ptr();
  const int8_t* get_ptr() const;
  bool IsEmpty() const;

protected:
  SoundInfo info_;
  size_t buffer_size_;  /* buffer size in byte */
  int8_t* buffer_;
};

/**
 * @brief
 * Base sound attributes
 */
class BaseSound
{
public:
  BaseSound();

  void SetVolume(float v);
  void SetLoop(bool loop);
  void SetId(const std::string& id);
  const std::string& GetId();

  void Play() { Play(0); };
  void Stop() { Stop(0); };
  virtual void Play(int key) = 0;
  virtual void Stop(int key) = 0;

  virtual size_t MixDataTo(int8_t* copy_to, size_t byte_len) const;
  virtual size_t MixDataFrom(int8_t* copy_from, size_t src_offset, size_t byte_len) const;

protected:
  // sound id
  std::string id_;

  float volume_;
  bool loop_;
};

/**
 * @brief
 * PCM data with single buffer
 */
class Sound : public BaseSound, public PCMBuffer
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

  int8_t* ptr();
  const int8_t* ptr() const;

  /* @brief procedure which is called by mixer */
  virtual size_t MixDataTo(int8_t* copy_to, size_t byte_len) const;
  virtual size_t MixDataFrom(int8_t* copy_from, size_t src_offset, size_t byte_len) const;
  //virtual size_t CopyDataTo(int8_t* copy_to, size_t src_offset, size_t desired_byte) const;

  virtual void Play(int key);
  virtual void Stop(int key);

private:
  size_t buffer_remain_;
};

class Midi;

class SoundMidi : public BaseSound
{
public:
  SoundMidi();
  void SetMidi(Midi* midi);
  void SetMidiChannel(int midi_channel);

  virtual void Play(int key);
  virtual void Stop(int key);
  void SendEvent(uint8_t arg1, uint8_t arg2, uint8_t arg3);

  /* We don't make MixDataTo here, as midi context will mix all midi at once.
   * This method is only for mixing-per-channel, which is not suitable. */

private:
  Midi* midi_;
  int midi_channel_;
};

#if 0
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

void SoundVariableBufferToSoundBuffer(SoundVariableBuffer &in, Sound &out);
#endif

void memmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample, float src_volume);
void memmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample);

}

#endif