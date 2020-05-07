#ifndef RMIXER_SOUND_H
#define RMIXER_SOUND_H

#include <stdint.h>
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace rmixer
{

class Mixer;
class Sound;

/**
 * @brief describing PCM sound info
 */
struct SoundInfo
{
  uint8_t is_signed;  /* is bit signed */
  uint8_t bitsize;    /* bits per sample (value is set as unsigned by default) */
  uint8_t channels;   /* 1 : mono, 2 : stereo. */
  uint32_t rate;      /* sample rate (Hz) */

  SoundInfo();
  SoundInfo(uint8_t signed_, uint8_t bitsize_, uint8_t channels_, uint32_t rate_);
  SoundInfo(const SoundInfo&) = default;
  SoundInfo(SoundInfo&&) = default;
  SoundInfo& operator=(const SoundInfo&) = default;

  static void SetDefaultSoundInfo(const SoundInfo& info);
  static const SoundInfo& GetDefaultSoundInfo();
};

uint32_t GetByteFromFrame(uint32_t frame, const SoundInfo& info);
uint32_t GetByteFromSample(uint32_t sample, const SoundInfo& info);
uint32_t GetByteFromMilisecond(uint32_t ms, const SoundInfo& sinfo);
uint32_t GetFrameFromMilisecond(uint32_t ms, const SoundInfo& sinfo);
uint32_t GetFrameFromByte(uint32_t byte, const SoundInfo& sinfo);
uint32_t GetSampleFromByte(uint32_t byte, const SoundInfo& sinfo);
uint32_t GetMilisecondFromByte(uint32_t byte, const SoundInfo& sinfo);
float GetMilisecondFromByteF(uint32_t byte, const SoundInfo& sinfo);
uint32_t GetMilisecondFromFrame(uint32_t frame, const SoundInfo& sinfo);

bool operator==(const SoundInfo& a, const SoundInfo& b);
bool operator!=(const SoundInfo& a, const SoundInfo& b);

/**
 * @brief
 * PCM sound data.
 */
class Sound
{
public:
  Sound();
  Sound(const SoundInfo& info, size_t buffer_size);
  Sound(const SoundInfo& info, size_t buffer_size, int8_t *p);
  ~Sound();

  void set_name(const std::string& name);
  const std::string& name();

  bool Load(const std::string& path);
  bool Load(const std::string& path, const SoundInfo& info);
  bool Load(const char* p, size_t len);
  bool Load(const char* p, size_t len, const SoundInfo &info);
  bool Save(const std::string& path);
  bool Save(const std::string& path, const SoundInfo &info);
  bool Save(const std::string& path,
    const std::map<std::string, std::string> &metadata,
    const SoundInfo *info,
    double quality);
  void Clear();

  void AllocateSize(const SoundInfo& info, size_t buffer_size);
  void AllocateSample(const SoundInfo& info, size_t sample_size);
  void AllocateFrame(const SoundInfo& info, size_t frame_size);
  void AllocateDuration(const SoundInfo& info, uint32_t duration_ms);
  void SetBuffer(const SoundInfo& info, size_t framecount, void*);
  void SetEmptyBuffer(const SoundInfo& info, size_t framecount);
  bool Effect(double pitch, double tempo, double volume);
  bool Resample(const SoundInfo& new_info);
  bool SetSoundFormat(const SoundInfo& info); /* alias to Resample */
  float GetSoundLevel(size_t frame_offset, size_t sample_count) const;

  size_t get_frame_count() const;
  size_t get_sample_count() const;
  float get_duration() const;   /* in milisecond */
  const SoundInfo& get_soundinfo() const;
  size_t get_total_byte() const;
  int8_t* get_ptr();
  const int8_t* get_ptr() const;
  bool is_empty() const;
  bool is_loading() const;
  bool is_loaded() const;
  size_t GetByteFromSample(size_t sample_len) const;
  size_t GetByteFromFrame(size_t frame_len) const;

  /**
   * @brief   Mix or Copy(faster method) PCM data from Sound object.
   * @param   output      buffer to be filled.
   * @param   offset      source buffer offset (in frame)
   * @param   sample_len  frame count to mix/copy
   * @param   volume      mixing/copying volume (optional; default 1.0)
   * @return  filled buffer size in frame count
   */
  virtual size_t Mix(int8_t *copy_to, size_t *offset, size_t sample_len) const;
  virtual size_t MixWithVolume(int8_t *copy_to, size_t *offset, size_t sample_len, float volume) const;
  virtual size_t Copy(int8_t *p, size_t *offset, size_t sample_len) const;
  virtual size_t CopyWithVolume(int8_t *p, size_t *offset, size_t sample_len, float volume) const;

  void swap(Sound &s);
  void copy(const Sound &src);
  Sound* clone() const;
  std::string toString() const;

  static void EnableDetailedLog(bool enable_detailed_log);
  friend class Mixer;

private:
  // sound name
  std::string name_;

  SoundInfo info_;
  int8_t* buffer_;
  float duration_;      /* in milisecond */
  bool is_loading_;     /* if loading in async mode. */

protected:
  size_t buffer_size_;  /* buffer size in byte */
  size_t frame_size_;
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

void pcmcpy(int8_t* dst, const int8_t* src, size_t sample_count);
void pcmcpy(int16_t* dst, const int16_t* src, size_t sample_count);
void pcmcpy24(int8_t* dst, const int8_t* src, size_t sample_count);
void pcmcpy(int32_t* dst, const int32_t* src, size_t sample_count);
void pcmcpy(uint8_t* dst, const uint8_t* src, size_t sample_count);
void pcmcpy(uint16_t* dst, const uint16_t* src, size_t sample_count);
void pcmcpy(uint32_t* dst, const uint32_t* src, size_t sample_count);
void pcmcpy(float* dst, const float* src, size_t sample_count);

void pcmmix(int8_t* dst, const int8_t* src, size_t sample_count);
void pcmmix(int16_t* dst, const int16_t* src, size_t sample_count);
void pcmmix24(int8_t* dst, const int8_t* src, size_t sample_count);
void pcmmix(int32_t* dst, const int32_t* src, size_t sample_count);
void pcmmix(uint16_t* dst, const uint16_t* src, size_t sample_count);
void pcmmix(uint32_t* dst, const uint32_t* src, size_t sample_count);
void pcmmix(float* dst, const float* src, size_t sample_count);

void pcmcpy(int8_t* dst, const int8_t* src, size_t sample_count, float volume);
void pcmcpy(int16_t* dst, const int16_t* src, size_t sample_count, float volume);
void pcmcpy24(int8_t* dst, const int8_t* src, size_t sample_count, float volume);
void pcmcpy(int32_t* dst, const int32_t* src, size_t sample_count, float volume);
void pcmcpy(uint8_t* dst, const uint8_t* src, size_t sample_count, float volume);
void pcmcpy(uint16_t* dst, const uint16_t* src, size_t sample_count, float volume);
void pcmcpy(uint32_t* dst, const uint32_t* src, size_t sample_count, float volume);
void pcmcpy(float* dst, const float* src, size_t sample_count, float volume);

void pcmmix(int8_t* dst, const int8_t* src, size_t sample_count, float volume);
void pcmmix(int16_t* dst, const int16_t* src, size_t sample_count, float volume);
void pcmmix24(int8_t* dst, const int8_t* src, size_t sample_count, float volume);
void pcmmix(int32_t* dst, const int32_t* src, size_t sample_count, float volume);
void pcmmix(uint8_t* dst, const uint8_t* src, size_t sample_count, float volume);
void pcmmix(uint16_t* dst, const uint16_t* src, size_t sample_count, float volume);
void pcmmix(uint32_t* dst, const uint32_t* src, size_t sample_count, float volume);
void pcmmix(float* dst, const float* src, size_t sample_count, float volume);

void pcmmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample, float src_volume);
void pcmmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample);

}

#endif
