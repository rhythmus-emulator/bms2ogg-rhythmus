#ifndef RENCODER_MIDI_H
#define RENCODER_MIDI_H

#include "Sound.h"

struct _MidSong;
typedef struct _MidSong MidSong;
struct _MidIStream;
typedef struct _MidIStream MidIStream;

namespace rmixer
{

constexpr size_t kMidiDefMaxBufferByteSize = 1024 * 1024;
constexpr size_t kMidiMaxChannel = 16;
class Midi;

class MidiChannel
{
public:
  MidiChannel();
  void SetVelocity(float v);
  void SetVelocityRaw(uint8_t v);
  void SetDefaultKey(uint8_t key);
  void Play(uint8_t key);
  void Stop(uint8_t key);
  void SendEvent(uint8_t *e);
  void Play();
  void Stop();
  friend class Midi;
private:
  Midi *midi_;
  uint8_t channel_;
  uint8_t default_key_;
  uint8_t volume_;
};

class Midi
{
public:
  /**
   * SoundInfo : output PCM format.
   * buffer_size_in_byte : maximum PCM output buffer size.
   */
  Midi(size_t buffer_size_in_byte = kMidiDefMaxBufferByteSize,
    const char* midi_cfg_path = 0);
  Midi(const SoundInfo& info,
    size_t buffer_size_in_byte = kMidiDefMaxBufferByteSize,
    const char* midi_cfg_path = 0);
  ~Midi();

  bool LoadFile(const char* filename);
  bool LoadFromStream();
  void Restart();
  void Close();

  void Play(uint8_t channel, uint8_t key);
  void Stop(uint8_t channel, uint8_t key);
  MidiChannel *GetChannel(uint8_t channel);
  void SendEvent(uint8_t channel, uint8_t type, uint8_t a, uint8_t b);
  void SetVolume(float v);

  void ClearEvent();
  bool IsMixFinish();
  size_t GetMixedPCMData(char* outbuf, size_t size);

  uint8_t GetEventTypeFromStatus(uint8_t status, uint8_t &a, uint8_t &b);
  const SoundInfo& get_soundinfo() const;

private:
  static int midi_count;
  MidSong *song_;
  SoundInfo info_;
  size_t buffer_size_;
  uint8_t rpn_msb_[kMidiMaxChannel];
  uint8_t rpn_lsb_[kMidiMaxChannel];
  uint8_t nrpn_;
  MidiChannel ch_[kMidiMaxChannel];

  bool Init(const SoundInfo& info, MidIStream *stream);
  bool Init(MidIStream *stream);
};

/**
 * @brief Adaptor for converting midi audio to PCM sound data.
 * @warn  offset parameter of Mix/Copy method is always ignored and will be set to 0.
 */
class MidiSound : public Sound
{
public:
  MidiSound(const SoundInfo& info, Midi *midi);
  virtual size_t Mix(int8_t *copy_to, size_t *offset, size_t frame_len) const;
  virtual size_t MixWithVolume(int8_t *copy_to, size_t *offset, size_t frame_len, float volume) const;
  virtual size_t Copy(int8_t *p, size_t *offset, size_t frame_len) const;
  virtual size_t CopyWithVolume(int8_t *p, size_t *offset, size_t frame_len, float volume) const;
  void CreateMidiData(size_t frame_len);

private:
  void ReallocateBufferSize(size_t req_buffer_size);

  Midi *midi_;
  size_t actual_buffer_size_;
};

}

#endif