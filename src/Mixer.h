#ifndef RENCODER_MIXER_H_
#define RENCODER_MIXER_H_

#include "Sound.h"
#include "Midi.h"
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
 * Contains multiple sound data for mixing
 */
class Mixer
{
public:
  Mixer(const SoundInfo& info, size_t max_buffer_byte_size = 1024*1024);
  Mixer(const SoundInfo& info, const char* midi_cfg_path, size_t max_buffer_byte_size = 1024*1024);
  ~Mixer();
  bool RegisterSound(uint32_t channel, Sound* s, bool is_freeable = true);
  void Clear();

  bool Play(uint32_t channel);
  bool PlayMidi(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b);
  bool PlayMidi(uint8_t c, uint8_t a, uint8_t b);
  bool PlayMidi_NoteOn(uint8_t channel, uint8_t key);
  bool PlayMidi_NoteOff(uint8_t channel, uint8_t key);

  /* mixing specified byte will automatically  */
  void Mix(char* out, size_t size);
  void Mix(PCMBuffer& out);

  void PlayRecord(uint32_t delay_ms, uint32_t channel);
  void PlayMidiRecord(uint32_t ms, uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b);
  void PlayMidiRecord(uint32_t ms, uint8_t c, uint8_t a, uint8_t b);
  void PlayMidiRecord_NoteOn(uint32_t ms, uint8_t channel, uint8_t key, uint8_t volume = 0x7F);
  void PlayMidiRecord_NoteOff(uint32_t ms, uint8_t channel, uint8_t key, uint8_t volume = 0);

  void MixRecord(PCMBuffer& out);
  size_t CalculateTotalRecordByteSize();

  Sound* GetSound(uint32_t channel);
  void SetChannelVolume(uint32_t channel, float v);
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

  double time_ms_;
  SoundInfo info_;
  std::map<uint32_t, MixChannel> channels_;
  std::vector<MixingRecord> mixing_record_;
  std::vector<MidiMixingRecord> midi_mixing_record_;
  size_t max_mixing_byte_size_;

  MixChannel* GetMixChannel(uint32_t channel);

  Midi midi_;
  char* midi_buf_;
};

}

#endif