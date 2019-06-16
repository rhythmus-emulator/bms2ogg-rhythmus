#ifndef RENCODER_MIXER_H_
#define RENCODER_MIXER_H_

#include "Sound.h"
#include "Midi.h"
#include "rutil.h"
#include <vector>
#include <map>

namespace rhythmus
{

struct MixChannel
{
  Sound *s;
  float volume;
  uint32_t total_byte;
  uint32_t remain_byte;
  bool loop;
  bool is_freeable;
};

/**
 * @description
 * Contains multiple sound data for mixing.
 */
class Mixer
{
public:
  Mixer(const SoundInfo& info, size_t max_buffer_byte_size = 1024*1024);
  Mixer(const SoundInfo& info, const char* midi_cfg_path, size_t max_buffer_byte_size = 1024*1024);
  ~Mixer();

  bool LoadSound(uint16_t channel, rutil::FileData &fd);
  bool LoadSound(uint16_t channel, const std::string& filepath);
  bool LoadSound(uint16_t channel, Sound* s, bool is_freeable = true);
  void FreeSoundGroup(uint16_t group);
  void FreeSound(uint16_t channel);
  void FreeAllSound();
  Sound* GetSound(uint16_t channel);
  void SetChannelVolume(uint16_t channel, float v);

  /**
   * Set source of mixing sound. currently available:
   * 0: PCM WAVE data
   * 1: MIDI data
   */
  void SetSoundSource(int sourcetype);
  /**
   * Set WAVE Sound group to Stop / Play.
   */
  void SetSoundGroup(uint16_t group);

  void Play(uint16_t channel, uint8_t key = 0, float volume = 1.0f);
  void Stop(uint16_t channel, uint8_t key = 0);
  void SendEvent(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b);
  void SendEvent(uint8_t c, uint8_t a, uint8_t b);

  /* mixing specified byte */
  void Mix(char* out, size_t size);
  void Mix(PCMBuffer& out);

  void SetRecordMode(uint32_t time_ms);
  void FinishRecordMode();
  void ClearRecord();
  void MixRecord(PCMBuffer& out);
  size_t CalculateTotalRecordByteSize();

  void Clear();

private:
  struct MixingRecord
  {
    uint32_t ms;
    uint32_t channel;
  };
  struct MidiMixingRecord
  {
    uint32_t ms;
    uint8_t channel, event_type;
    uint8_t a, b;
  };

  SoundInfo info_;
  std::map<uint32_t, MixChannel> channels_;
  std::vector<MixingRecord> mixing_record_;
  std::vector<MidiMixingRecord> midi_mixing_record_;
  size_t max_mixing_byte_size_;
  Midi midi_;
  char* midi_buf_;
  uint16_t current_group_;
  int current_source_;
  uint32_t record_time_ms_;
  bool is_record_mode_;

  void PlayRT(uint16_t channel, uint8_t key, float volume);
  void StopRT(uint16_t channel, uint8_t key);
  void EventRT(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b);
  void PlayRecord(uint16_t channel, uint8_t key, float volume);
  void StopRecord(uint16_t channel, uint8_t key);
  void EventRecord(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b);
  MixChannel* GetMixChannel(uint16_t channel);
};

}

#endif