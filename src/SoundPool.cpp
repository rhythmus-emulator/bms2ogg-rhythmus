#include "SoundPool.h"

#include "playmidi.h" // midi event related

namespace rmixer
{

// ---------------------------- class SoundPool

SoundPool::SoundPool() : channels_(0), pool_size_(0), mixer_(0)
{
}

SoundPool::~SoundPool()
{
  Clear();
}

void SoundPool::Initalize(size_t pool_size)
{
  if (channels_)
    return;
  channels_ = (BaseSound**)calloc(1, pool_size * sizeof(Sound*));
  pool_size_ = pool_size;
}

void SoundPool::Clear()
{
  if (!channels_)
    return;

  UnregisterAll();

  for (size_t i = 0; i < pool_size_; ++i)
    delete channels_[i];
  free(channels_);
  channels_ = 0;
  pool_size_ = 0;
}

BaseSound* SoundPool::GetSound(size_t channel)
{
  if (!channels_ || channel >= pool_size_)
    return nullptr;
  else return channels_[channel];
}

Sound* SoundPool::CreateEmptySound(size_t channel)
{
  if (!channels_ || channel >= pool_size_)
    return nullptr;
  delete channels_[channel];
  return (Sound*)(channels_[channel] = new Sound());
}

bool SoundPool::LoadSound(size_t channel, const std::string& path)
{
  Sound* s = CreateEmptySound(channel);
  ASSERT(s);
  return s->Load(path);
}

bool SoundPool::LoadSound(size_t channel, const char* p, size_t len)
{
  Sound* s = CreateEmptySound(channel);
  ASSERT(s);
  return s->Load(p, len);
}

void SoundPool::LoadMidiSound(size_t channel)
{
  if (!channels_ || channel >= pool_size_)
    return;
  delete channels_[channel];
  SoundMidi *s = new SoundMidi();
  s->SetMidiChannel(channel);
  channels_[channel] = s;
}

void SoundPool::RegisterToMixer(Mixer& mixer)
{
  if (!channels_ || mixer_)
    return;

  for (size_t i = 0; i < pool_size_; ++i) if (channels_[i])
  {
    mixer.RegisterSound(channels_[i]);
  }

  mixer_ = &mixer;
}

void SoundPool::UnregisterAll()
{
  if (channels_ && mixer_)
  {
    for (size_t i = 0; i < pool_size_; ++i) if (channels_[i])
    {
      mixer_->UnregisterSound(channels_[i]);
    }
  }
  mixer_ = 0;
}


// ------------------------- class KeySoundPool

KeySoundPool::KeySoundPool() : lane_count_(36)
{
  memset(channel_mapping_, 0, sizeof(channel_mapping_));
}

KeySoundPool::~KeySoundPool()
{}

void KeySoundPool::SetLaneChannel(size_t lane, size_t ch)
{
  if (lane > lane_count_) return;
  channel_mapping_[lane] = ch;
}

void KeySoundPool::SetLaneChannel(size_t lane, size_t ch, int duration, int defkey, float volume)
{
  if (lane > lane_count_) return;
  SetLaneChannel(lane, ch);
  BaseSound* s = channels_[ch];
  s->SetDuration(duration);
  s->SetDefaultKey(defkey);
  s->SetVolume(volume);
}

bool KeySoundPool::SetLaneCount(size_t lane_count)
{
  if (lane_count > kMaxLaneCount)
    return false;
  lane_count_ = lane_count;
  return true;
}

void KeySoundPool::PlayLane(size_t lane)
{
  if (lane > lane_count_) return;
  BaseSound *s = GetSound(channel_mapping_[lane]);
  if (!s) return;
  s->Play();
}

void KeySoundPool::StopLane(size_t lane)
{
  if (lane > lane_count_) return;
  BaseSound *s = GetSound(channel_mapping_[lane]);
  if (!s) return;
  s->Stop();
}

void KeySoundPool::PlayLane(size_t lane, int key)
{
  if (lane > lane_count_) return;
  BaseSound *s = GetSound(channel_mapping_[lane]);
  if (!s) return;
  s->Play(key);
}

void KeySoundPool::StopLane(size_t lane, int key)
{
  if (lane > lane_count_) return;
  BaseSound *s = GetSound(channel_mapping_[lane]);
  if (!s) return;
  s->Stop(key);
}


// ----------------- class KeySoundPoolWithTime

KeySoundPoolWithTime::KeySoundPoolWithTime()
  : time_(0), is_autoplay_(false),
    loading_progress_(0), loading_finished_(true),
    volume_base_(1.0f)
{
  memset(lane_idx_, 0, sizeof(lane_idx_));
}

void KeySoundPoolWithTime::LoadFromChartAndSound(const rparser::Chart& c)
{
  LoadFromChart(c);
  while (!is_loading_finished())
  {
    LoadRemainingSound();
  }
}

void KeySoundPoolWithTime::LoadFromChart(const rparser::Chart& c)
{
  using namespace rparser;

  // clear loading context
  loading_finished_ = false;
  loading_progress_ = 0.;
  file_load_idx_ = 0;
  files_to_load_.clear();

  // prepare desc for loading
  Directory *dir = c.GetParent()->GetDirectory();
  if (dir)
    dir->SetAlternativeSearch(true);
  const auto &md = c.GetMetaData();
  for (auto &ii : md.GetSoundChannel()->fn)
  {
    const char* p;
    size_t len;
    const std::string filename = ii.second;
    if (!dir)
    {
      if (filename == "midi")
      {
        // reserved filename for midi channel
        LoadMidiSound(ii.first);
      }
    }
    else
    {
      files_to_load_.push_back({
        filename, ii.first, dir
        });
    }
  }

  // set lane count here?
  SetLaneCount(1000);

  KeySoundProperty ksoundprop;

  // -- add MIDI command as event property in BGM channel.
  {
    auto midieventtrack =
      c.GetEffectData().get_track(rparser::EffectObjectTypes::kMIDI);
    for (auto *obj : midieventtrack)
    {
      auto &e = static_cast<EffectObject&>(*obj);
      ksoundprop.Clear();
      e.GetMidiCommand(ksoundprop.event_args[0],
        ksoundprop.event_args[1],
        ksoundprop.event_args[2]);
      ksoundprop.time = static_cast<float>(e.GetTimePos());
      ksoundprop.autoplay = 1;
      ksoundprop.playable = 0;
      lane_time_mapping_[0].push_back(ksoundprop);
    }
  }

  // -- Bgm event (lane 0)
  {
    auto &bgmtrack = c.GetBgmData();
    auto iter = bgmtrack.GetAllTrackIterator();
    while (!iter.is_end())
    {
      auto &n = static_cast<BgmObject&>(*iter);
      ksoundprop.Clear();
      ksoundprop.time = (float)n.time_msec;
      ksoundprop.autoplay = 1;
      ksoundprop.playable = 0;
      ksoundprop.channel = n.channel();

      // generally MIDI properties
      ksoundprop.event_args[0] = ME_NOTEON;
      ksoundprop.event_args[1] = n.key();
      ksoundprop.event_args[2] = static_cast<uint8_t>(n.volume() * 0x7F);
      // NOTEOFF will called automatically at the end of duration
      ksoundprop.duration = n.length();

      lane_time_mapping_[0].push_back(ksoundprop);
      ++iter;
    }
  }

  // -- Chart notes
  {
    auto &notedata = c.GetNoteData();
    auto iter = notedata.GetAllTrackIterator();
    while (!iter.is_end())
    {
      auto &n = static_cast<Note&>(*iter);
      ksoundprop.Clear();
      ksoundprop.time = (float)n.time_msec;
      ksoundprop.autoplay = 0;
      ksoundprop.playable = 1;
      ksoundprop.channel = n.channel();

      // generally MIDI properties
      ksoundprop.event_args[0] = ME_NOTEON;
      ksoundprop.event_args[1] = n.key();
      ksoundprop.event_args[2] = static_cast<uint8_t>(n.volume() * 0x7F);
      // NOTEOFF will called automatically at the end of duration
      ksoundprop.duration = n.length();

      lane_time_mapping_[n.get_track() + 1].push_back(ksoundprop);
      ++iter;
    }
  }

  lane_count_ = c.GetNoteData().get_track_count();

  // sort keyevents by time
  for (size_t i = 0; i <= lane_count_; ++i)
    std::sort(lane_time_mapping_[i].begin(), lane_time_mapping_[i].end());

  // whole load finished
  loading_progress_ = 1.0;
}

void KeySoundPoolWithTime::LoadRemainingSound()
{
  std::string filename;
  size_t channel;
  rparser::Directory *dir;
  const char* p;
  size_t len;

  loading_mutex_.lock();
  if (file_load_idx_ >= files_to_load_.size())
  {
    // cannot read more ...
    loading_mutex_.unlock();
    return;
  }
  auto &ld = files_to_load_[file_load_idx_];
  dir = (rparser::Directory*)ld.dir;
  filename = ld.filename;
  channel = ld.channel;
  file_load_idx_++;
  loading_mutex_.unlock();

  if (!dir->GetFile(filename, &p, len))
  {
    std::cerr << "Missing sound file: " << filename
      << " (" << channel << ")" << std::endl;
    return;
  }
  if (!LoadSound(channel, p, len))
  {
    std::cerr << "Failed loading sound file: " << filename
      << " (" << channel << ")" << std::endl;
    return;
  }

  // XXX: this shouldn't need to be thread-safe ..?
  loading_finished_ = (file_load_idx_ >= files_to_load_.size());
  loading_progress_ = (double)file_load_idx_ / files_to_load_.size();
}

double KeySoundPoolWithTime::get_load_progress() const
{
  return loading_progress_;
}

bool KeySoundPoolWithTime::is_loading_finished() const
{
  return loading_finished_;
}

void KeySoundPoolWithTime::SetAutoPlay(bool autoplay)
{
  is_autoplay_ = autoplay;
}

void KeySoundPoolWithTime::SetVolume(float volume)
{
  volume_base_ = volume;
}

void KeySoundPoolWithTime::TapLane(size_t lane)
{
  BaseSound *s = channels_[channel_mapping_tap_[lane]];
  if (!s) return;
  s->Play();
}

void KeySoundPoolWithTime::UnTapLane(size_t lane)
{
  BaseSound *s = channels_[channel_mapping_tap_[lane]];
  if (!s) return;
  s->Stop();
}

void KeySoundPoolWithTime::MoveTo(float ms)
{
  // do skipping
  for (size_t i = 0; i <= lane_count_; ++i)
  {
    lane_idx_[i] = 0;
    while (lane_idx_[i] < lane_time_mapping_[i].size() &&
           lane_time_mapping_[i][lane_idx_[i]].time <= ms)
      lane_idx_[i]++;
    // internally reduce index for force-update ...
    if (lane_idx_[i] > 0) lane_idx_[i]--;
  }

  time_ = ms;

  // now do force-update
  Update(0);
}

float KeySoundPoolWithTime::GetLastSoundTime() const
{
  float last_play_time = 0;
  for (size_t i = 0; i <= lane_count_; ++i)
  {
    for (auto& keyevt : lane_time_mapping_[i])
    {
      BaseSound *s = channels_[keyevt.channel];
      if (!s) continue;
      float key_end_time = keyevt.time + s->GetDuration();
      last_play_time = std::max(last_play_time, key_end_time);
    }
  }
  return last_play_time;
}

void KeySoundPoolWithTime::Update(float delta_ms)
{
  time_ += delta_ms;

  // update lane-channel table
  for (size_t i = 0; i <= lane_count_; ++i)
  {
    bool is_lane_updated = false;
    while (lane_idx_[i] < lane_time_mapping_[i].size() &&
      lane_time_mapping_[i][lane_idx_[i]].time <= time_)
    {
      auto& currlanecmd = lane_time_mapping_[i][lane_idx_[i]];
      lane_idx_[i]++;

      // set channel to lane
      SetLaneChannel(i, currlanecmd.channel);
      // get sound object of i-th lane.
      BaseSound *s = GetSound(channel_mapping_[i]);
      if (!s) continue;

      // check for any special effect command.
      // if not, then just play lane.
      if (currlanecmd.event_args[0] > 1)
      {
        s->SetCommand(currlanecmd.event_args);
      }
      else
      {
        // set sound-playing property of i-th lane.
        SetLaneChannel(i,
          currlanecmd.channel,
          currlanecmd.duration,
          currlanecmd.event_args[1],
          (currlanecmd.event_args[2] / (double)0x7F) * volume_base_
        );

        // check for autoplay flag
        if (is_autoplay_ || currlanecmd.autoplay)
        {
          s->Play();
        }
      }
      is_lane_updated = true;
    }

    // if lane updated, then also update next tapping object
    if (is_lane_updated && i > 0 /* not bgm lane */)
    {
      size_t idx = lane_idx_[i] + 1;
      while (idx < lane_time_mapping_[i].size())
      {
        auto &e = lane_time_mapping_[i][idx];
        BaseSound *s = channels_[e.channel];
        channel_mapping_tap_[i] = e.channel;
        if (s)
        {
          s->SetDuration(e.duration);
          s->SetDefaultKey(e.event_args[1]);
          s->SetVolume((e.event_args[2] / (double)0x7F) * volume_base_);
        }
        // XXX: is there any situation to skip lane to search next sound object?
        break;
      }
    }
  }
}

void KeySoundPoolWithTime::RecordToSound(Sound &s)
{
  // we reuse loading progress here again ...
  loading_finished_ = false;
  loading_progress_ = 0.;

  // stack mixing timepoint
  std::vector<float> mixing_timepoint;
  for (size_t i = 0; i <= lane_count_; ++i)
  {
    for (size_t j = 0; j < lane_time_mapping_[i].size(); ++j)
      mixing_timepoint.push_back(lane_time_mapping_[i][j].time);
  }
  std::sort(mixing_timepoint.begin(), mixing_timepoint.end());

  // reduce mixing timepoint for optimization
  std::vector<float> mixing_timepoint_opt;
  for (float timepoint : mixing_timepoint)
  {
    if (mixing_timepoint_opt.empty() || timepoint - mixing_timepoint_opt.back() > 10)
      mixing_timepoint_opt.push_back(timepoint);
    else
      mixing_timepoint_opt.back() = timepoint;
  }

  if (mixing_timepoint_opt.empty())
    return;

  // get last timepoint(byte offset) of the mixing ...
  // Give 3 sec of spare time
  ASSERT(mixer_);
  SoundInfo info = mixer_->GetSoundInfo();
  float last_play_time = GetLastSoundTime() + 3000;
  size_t last_offset = GetByteFromMilisecond(last_play_time, info);

  // allocate new sound and start mixing
  s.get_buffer()->SetEmptyBuffer(info, GetFrameFromMilisecond(last_play_time, info));
  size_t p_offset = 0;
  size_t p_offset_delta = 0;
  float prev_timepoint = 0;
  for (float timepoint : mixing_timepoint_opt)
  {
    p_offset_delta = GetByteFromMilisecond(timepoint, info) - p_offset;
    mixer_->Mix((char*)s.get_buffer()->get_ptr() + p_offset, p_offset_delta);
    Update(timepoint - prev_timepoint);
    prev_timepoint = timepoint;
    p_offset += p_offset_delta;
  }

  // mix remaining byte to end
  ASSERT(last_offset >= p_offset);
  mixer_->Mix((char*)s.get_buffer()->get_ptr() + p_offset, last_offset - p_offset);
}

}
