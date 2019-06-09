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
  ~Mixer();
  bool RegisterSound(uint32_t channel, Sound* s, bool is_freeable = true);
  void Clear();

  bool Play(uint32_t channel);

  /* mixing specified byte will automatically  */
  void Mix(char* out, size_t size);
  void Mix(PCMBuffer& out);

  void PlayRecord(uint32_t channel, uint32_t delay_ms);
  void MixRecord(PCMBuffer& out);
  size_t CalculateTotalRecordByteSize();

  Sound* GetSound(uint32_t channel);
  void SetChannelVolume(uint32_t channel, float v);
private:
  struct MixingRecord
  {
    uint32_t channel;
    uint32_t ms;
  };

  double time_ms_;
  SoundInfo info_;
  std::map<uint32_t, MixChannel> channels_;
  std::vector<MixingRecord> mixing_record_;
  size_t max_mixing_byte_size_;

  MixChannel* GetMixChannel(uint32_t channel);

  Midi midi_;
  char* midi_buf_;
};

}

#endif