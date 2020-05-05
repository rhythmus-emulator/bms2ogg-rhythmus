#ifndef RMIXER_SOUNDPOOL_H
#define RMIXER_SOUNDPOOL_H

#include "rparser.h"

namespace rmixer
{

class Mixer;
class Sound;
class Channel;
class MidiChannel;

constexpr size_t kMaxLaneCount = 256;

/* @brief contains sound and channels for playing soundfile */
class SoundPool
{
public:
  SoundPool(Mixer *mixer, size_t pool_size);
  virtual ~SoundPool();

  Sound* GetSound(size_t channel);
  bool LoadSound(size_t channel, const std::string& path);
  bool LoadSound(size_t channel, const char* p, size_t len, const char *name = nullptr);

  void Play(size_t lane);
  void Stop(size_t lane);
  void PlayMidi(uint8_t lane, uint8_t key);
  void StopMidi(uint8_t lane, uint8_t key);

  Mixer* get_mixer();
  Channel* get_channel(size_t ch);
  MidiChannel* get_midi_channel(uint8_t ch);
  const Mixer* get_mixer() const;
  const Channel* get_channel(size_t ch) const;
  const MidiChannel* get_midi_channel(uint8_t ch) const;

private:
  Channel** channels_;
  size_t pool_size_;
  Mixer* mixer_;
};

class KeySoundPoolWithTime : public SoundPool
{
public:
  KeySoundPoolWithTime(Mixer *mixer, size_t pool_size);

  /* @brief shortcut for load chart and whole sound files */
  void LoadFromChartAndSound(const rparser::Chart& c);

  /* @brief only loads chart and prepare to load sound files */
  void LoadFromChart(const rparser::Chart& c);

  /* @brief load a sound files (for async method) */
  void LoadRemainingSound();

  double get_load_progress() const;
  bool is_loading_finished() const;

  void SetAutoPlay(bool autoplay);
  void MoveTo(float ms);
  void Update(float delta_ms);
  void SetVolume(float volume);

  /* @brief Get last sound playing time. (not last object time!) */
  float GetLastSoundTime() const;

  /* @brief Create sound using mixer based on lane_time_mapping table.
   * @warn RegisterToMixer() should be called first. */
  void RecordToSound(Sound &s);

private:
  struct KeySoundProperty;
  void SetLaneChannel(unsigned lane, KeySoundProperty *prop);

  struct KeySoundProperty
  {
    unsigned channel;
    float time;
    int event_type;

    // is midi channel?
    bool is_midi_channel;

    // check for autoplay.
    // (always played sound e.g. BGM)
    int autoplay;

    // check for playable sound object
    // (actually played sound when PlayLane() triggered)
    int playable;

    // event arguments -- only for MIDI channel.
    uint8_t event_args[3];

    void Clear();
    bool operator<(const KeySoundProperty &other) const
    {
      return time < other.time;
    }
  };

  // mixing sequence objects of each lane
  std::vector<KeySoundProperty> lane_time_mapping_[kMaxLaneCount];

  // mixing sequence number of each lane
  size_t lane_idx_[kMaxLaneCount];

  // lane-to-channel(keysound) mapping object
  KeySoundProperty* lane_mapping_[kMaxLaneCount];

  float time_;
  bool is_autoplay_;
  size_t lane_count_;

  // loading related
  struct LoadFileDesc
  {
    std::string filename;
    size_t channel;
    void *dir;
  };
  std::vector<LoadFileDesc> files_to_load_;
  size_t file_load_idx_;
  double loading_progress_;
  bool loading_finished_;
  std::mutex loading_mutex_;

  // base volume of each channels
  float volume_base_;
};

}

#endif
