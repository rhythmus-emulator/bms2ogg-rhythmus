#ifndef RMIXER_SOUNDPOOL_H
#define RMIXER_SOUNDPOOL_H

#include "Mixer.h"
#include "rparser.h"

namespace rmixer
{

class SoundPool
{
public:
  SoundPool();
  virtual ~SoundPool();

  /* @brief general channel initializing method
            (sound object isn't initialized) */
  void Initalize(size_t pool_size);

  void Clear();
  BaseSound* GetSound(size_t channel);
  Sound* CreateEmptySound(size_t channel);
  bool LoadSound(size_t channel, const std::string& path);
  bool LoadSound(size_t channel, const std::string& path, const char* p, size_t len);
  void LoadMidiSound(size_t channel);
  void RegisterToMixer(Mixer& mixer);
  void UnregisterAll();

protected:
  BaseSound** channels_;
  size_t pool_size_;
  Mixer* mixer_;
};

constexpr size_t kMaxLaneCount = 256;

class KeySoundPool : public SoundPool
{
public:
  KeySoundPool();
  virtual ~KeySoundPool();

  void SetLaneChannel(size_t lane_idx, size_t ch);
  void SetLaneChannel(size_t lane_idx, size_t ch, int duration, int defkey, float volume);
  bool SetLaneCount(size_t lane_count);
  void PlayLane(size_t lane);
  void StopLane(size_t lane);
  void PlayLane(size_t lane, int key);
  void StopLane(size_t lane, int key);

protected:
  size_t channel_mapping_[kMaxLaneCount];
  size_t lane_count_;
};

class KeySoundPoolWithTime : public KeySoundPool
{
public:
  KeySoundPoolWithTime();
  void LoadFromChart(rparser::Song& s, const rparser::Chart& c);
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
  struct KeySoundProperty
  {
    size_t channel;
    float time;

    // check for autoplay.
    // (always played sound e.g. BGM)
    int autoplay;

    // check for playable sound object
    // (actually played sound when PlayLane() triggered)
    int playable;

    // key duration for a single stroke
    // if duration end, then key sound stops whatever it is.
    int duration;

    // event arguments -- only for MIDI channel.
    uint8_t event_args[3];

    void Clear() { memset(this, 0, sizeof(KeySoundProperty)); }
    bool operator<(const KeySoundProperty &other)
    {
      return time < other.time;
    }
  };

  std::vector<KeySoundProperty> lane_time_mapping_[kMaxLaneCount];
  size_t lane_idx_[kMaxLaneCount];
  float time_;
  bool is_autoplay_;
  bool loading_progress_;
  bool loading_finished_;

  // base volume of each channels
  float volume_base_;
};

}

#endif