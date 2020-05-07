#ifndef RMIXER_MIXER_H_
#define RMIXER_MIXER_H_

#include "Sound.h"
#include "Midi.h"
#include <mutex>
#include <vector>
#include <map>

namespace rmixer
{

typedef unsigned int ChannelIndex;

/**
 * @brief
 * An object that contains playback information of current channel.
 */
class Channel
{
public:
  Channel(ChannelIndex chidx);
  ChannelIndex get_channel_index() const;
  void SetSound(Sound *sound);
  void SetChannelGroup(unsigned groupidx);
  
  void SetVolume(float volume);
  void Pause();
  void Play();
  void Play(int loop_count);
  void Stop();
  void SetFadePoint(unsigned milisecond);
  void LockChannel();
  void UnlockChannel();

  void Copy(char *out, size_t frame_len);
  void Mix(char *out, size_t frame_len);
  void UpdateBySample(size_t sample);
  void UpdateByByte(size_t byte);

  Sound *get_sound();
  const Sound *get_sound()const ;
  float volume() const;
  bool is_playing() const;
  bool is_occupied() const;
  bool is_virtual() const;
  bool is_loaded() const;
  float get_playtime_ms() const;

  friend class Mixer;

private:
  ChannelIndex chidx_;
  unsigned groupidx_;
  Sound *sound_;

  float volume_;
  int loop_;
  bool is_paused_;
  bool is_occupied_;
  size_t frame_pos_;        // uint64_t
  uint32_t effect_length_;
  uint32_t effect_remain_;

  /* sound effect related */
  float pitch_;
  float speed_;
  float reverb_;
  int key_;

  /* sound level and virtual sound related */
  bool is_virtual_;
  float sound_level_;
  int priority_;
};

const size_t kMaxAudibleChannelCount = 1024;

/**
 * @brief
 * Contains multiple sound data for mixing.
 * Multi-thread safe.
 *
 * @param info mixing pcm sound spec
 * @param max_buffer_byte_size max buffer size for midi mixing
 */
class Mixer
{
public:
  Mixer();
  Mixer(const SoundInfo& info, ChannelIndex max_channel_size);
  ~Mixer();

  /* for lazy-initialization (warn: costly procedure) */
  void SetSoundInfo(const SoundInfo& info);
  const SoundInfo& GetSoundInfo() const;

  void SetMaxChannelSize(ChannelIndex max_channel_size);
  ChannelIndex GetMaxChannelSize() const;
  void SetMaxAudioSize(int max_audio_size);
  int GetMaxAudioSize() const;

  void SetCacheSound(bool cache_sound);
  Sound* CreateSound(const char *filepath, bool loadasync = false);
  Sound* CreateSound(const char *p, size_t len, const char *filename = nullptr, bool loadasync = false);
  void DeleteSound(Sound *sound);
  Channel* PlaySound(Sound *sound, bool start = false);
  void StopSound(Sound *sound);
  
  void Play(ChannelIndex channel);
  void Stop(ChannelIndex channel);

  /* for midi */
  void InitializeMidi(const char* config_path);
  void InitializeMidi();
  void ClearMidi();
  void SendMidiCommand(unsigned channel, uint8_t command, uint8_t param1);
  void PressMidiKey(unsigned channel, unsigned key, unsigned volume);
  void ReleaseMidiKey(unsigned channel, unsigned key);

  /* @brief Update channels for next mixing */
  void Update();

  void Mix(char* out, size_t frame_len, ChannelIndex channel_index);
  void Copy(char* out, size_t frame_len, ChannelIndex channel_index);
  void MixAll(char* out, size_t frame_len);

  Midi* get_midi();
  Sound* get_midi_sound();
  Channel* get_midi_channel();

private:
  SoundInfo info_;

  std::mutex* channel_lock_;

  /* @brief Decide to cache sound by Mixer */
  bool cache_sound_;

  /* @brief Cached sound data which is loaded by Mixer */
  std::vector<Sound*> sounds_;

  /* @brief registered sound objects (only mix, not released) */
  std::vector<Channel*> channels_;

  /* @brief audible channels which is updated by Update() method. */
  Channel* audible_channels_[kMaxAudibleChannelCount];

  /* @brief Available channel count for mixing (maximum audio). */
  int maximum_audio_count_;

  /* @brief midi mixer object */
  Midi* midi_;
  const char* midi_config_path_;

  /* @brief midi PCM is stored in this sound object before output. */
  Sound* midi_sound_;
  Channel* midi_channel_;
};

}

#endif
