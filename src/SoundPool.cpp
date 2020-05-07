#include "SoundPool.h"
#include "Error.h"
#include "Mixer.h"
#include "Midi.h"

#include <algorithm>
#include <iostream>
#include <memory.h>

namespace rmixer
{

enum InternalMidiEvents
{
  kNoteOn,
  kNoteOff,
  kEffect,
};

// ---------------------------- class SoundPool

SoundPool::SoundPool(Mixer *mixer, size_t pool_size)
  : channels_(0), pool_size_(pool_size), mixer_(mixer)
{
  // must enable sound caching by mixer, or memory will leak.
  mixer->SetCacheSound(true);
  channels_ = (Channel**)calloc(1, pool_size * sizeof(void*));
}

SoundPool::~SoundPool()
{
  free(channels_);
  channels_ = 0;
  pool_size_ = 0;
}

Sound* SoundPool::GetSound(size_t channel)
{
  if (!channels_ || channel >= pool_size_)
    return nullptr;
  else return channels_[channel]->get_sound();
}

bool SoundPool::LoadSound(size_t channel, const std::string& path)
{
  Sound* s = mixer_->CreateSound(path.c_str());
  if (!s) return false;
  channels_[channel] = mixer_->PlaySound(s, false);
  return true;
}

bool SoundPool::LoadSound(size_t channel, const char* p, size_t len, const char *name)
{
  Sound* s = mixer_->CreateSound(p, len, name, false);
  if (!s) return false;
  channels_[channel] = mixer_->PlaySound(s, false);
  return true;
}

void SoundPool::Play(size_t lane)
{
  if (channels_[lane])
    channels_[lane]->Play();
}

void SoundPool::Stop(size_t lane)
{
  if (channels_[lane])
    channels_[lane]->Stop();
}

void SoundPool::PlayMidi(uint8_t lane, uint8_t key)
{
  if (mixer_->get_midi())
    mixer_->get_midi()->GetChannel(lane)->Play(key);
}

void SoundPool::StopMidi(uint8_t lane, uint8_t key)
{
  if (mixer_->get_midi())
    mixer_->get_midi()->GetChannel(lane)->Stop(key);
}

Mixer *SoundPool::get_mixer() { return mixer_; }
Channel* SoundPool::get_channel(size_t ch) { return channels_[ch]; }
const Mixer *SoundPool::get_mixer() const { return mixer_; }
const Channel* SoundPool::get_channel(size_t ch) const { return channels_[ch]; }

MidiChannel* SoundPool::get_midi_channel(uint8_t ch)
{
  return mixer_->get_midi() ? mixer_->get_midi()->GetChannel(ch) : nullptr;
}

const MidiChannel* SoundPool::get_midi_channel(uint8_t ch) const
{
  return const_cast<SoundPool*>(this)->get_midi_channel(ch);
}

// ----------------- class KeySoundPoolWithTime

KeySoundPoolWithTime::KeySoundPoolWithTime(Mixer *mixer, size_t pool_size)
  : SoundPool(mixer, pool_size), time_(0), is_autoplay_(false), lane_count_(0),
    loading_progress_(0), loading_finished_(true),
    volume_base_(1.0f)
{
  memset(lane_mapping_, 0, sizeof(lane_mapping_));
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
  // if no directory, then it must be Midi sequenced file (e.g. VOS)
  // so don't prepare loading list.
  bool is_midi = true;
  Directory *dir = c.GetParent()->GetDirectory();
  if (dir)
  {
    dir->SetAlternativeSearch(true);
    const auto &md = c.GetMetaData();
    for (auto &ii : md.GetSoundChannel()->fn)
    {
      const std::string filename = ii.second;
      if (dir)
      {
        files_to_load_.push_back({
          filename, ii.first, dir
          });
      }
    }
    is_midi = false;
  }

  KeySoundProperty ksoundprop;

  // -- add MIDI command as event property in BGM channel.
  {
    auto midieventtrack =
      c.GetCommandData().get_track(rparser::CommandTrackTypes::kMidi);
    for (auto &obj : midieventtrack)
    {
      ksoundprop.Clear();
      int a, b, c;
      obj.get_point(a, b, c);
      ksoundprop.is_midi_channel = true;
      ksoundprop.event_type = InternalMidiEvents::kEffect;
      ksoundprop.event_args[0] = (uint8_t)a;
      ksoundprop.event_args[1] = (uint8_t)b;
      ksoundprop.event_args[2] = (uint8_t)c;
      ksoundprop.time = static_cast<float>(obj.time());
      ksoundprop.autoplay = 1;
      ksoundprop.playable = 0;
      lane_time_mapping_[0].push_back(ksoundprop);
    }
  }

  // -- Bgm event (lane 0)
  {
    auto &bgmtrack = const_cast<rparser::Chart&>(c).GetBgmData();
    auto iter = bgmtrack.GetAllTrackIterator();
    while (!iter.is_end())
    {
      auto &n = *iter;
      ksoundprop.Clear();
      ksoundprop.time = (float)n.time();
      ksoundprop.autoplay = 1;
      ksoundprop.playable = 0;

      auto &sprop = n.get_value_sprop();
      ksoundprop.channel = sprop.channel;
      ksoundprop.is_midi_channel = is_midi;
      ksoundprop.event_type = InternalMidiEvents::kNoteOn;

      // general MIDI properties
      ksoundprop.event_args[0] = 0;
      ksoundprop.event_args[1] = sprop.key;
      ksoundprop.event_args[2] = static_cast<uint8_t>(sprop.volume * 0x7F);

      lane_time_mapping_[0].push_back(ksoundprop);

      // in case of duration -- add NoteOff event
      if (sprop.length > 0)
      {
        ksoundprop.time += sprop.length;
        ksoundprop.event_type = InternalMidiEvents::kNoteOff;
        lane_time_mapping_[0].push_back(ksoundprop);
      }

      lane_time_mapping_[0].push_back(ksoundprop);
      ++iter;
    }
  }

  // -- Chart notes
  {
    auto &notedata = const_cast<rparser::Chart&>(c).GetNoteData();
    auto iter = notedata.GetAllTrackIterator();
    while (!iter.is_end())
    {
      auto &n = *iter;
      auto track = iter.get_track() + 1;
      ksoundprop.Clear();
      ksoundprop.time = (float)n.time();
      ksoundprop.autoplay = 0;
      ksoundprop.playable = 1;

      auto &sprop = n.get_value_sprop();
      ksoundprop.channel = sprop.channel;
      ksoundprop.event_type = InternalMidiEvents::kNoteOn;

      // general MIDI properties
      ksoundprop.event_args[0] = 0;
      ksoundprop.event_args[1] = sprop.key;
      ksoundprop.event_args[2] = static_cast<uint8_t>(sprop.volume * 0x7F);

      lane_time_mapping_[track].push_back(ksoundprop);

      // in case of duration -- add NoteOff event
      if (sprop.length > 0)
      {
        ksoundprop.time += sprop.length;
        ksoundprop.event_type = InternalMidiEvents::kNoteOff;
        lane_time_mapping_[track].push_back(ksoundprop);
      }

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
    loading_finished_ = true;
    loading_progress_ = 1.0;
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
      float key_end_time = keyevt.time;
      const Channel *ch = get_channel(keyevt.channel);
      const Sound *s = ch ? ch->get_sound() : nullptr;
      if (s) key_end_time += s->get_duration();
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
      SetLaneChannel(i, &currlanecmd);

      if (!currlanecmd.is_midi_channel)
      {
        switch (currlanecmd.event_type)
        {
        case InternalMidiEvents::kNoteOn:
          if (is_autoplay_ || currlanecmd.autoplay)
            Play(currlanecmd.channel);
          break;
        case InternalMidiEvents::kNoteOff:  // XXX: may not reachable
          if (is_autoplay_ || currlanecmd.autoplay)
            Stop(currlanecmd.channel);
          break;
        default:
          break;
        }
      }
      else
      {
        auto *c = get_midi_channel(currlanecmd.channel);
        switch (currlanecmd.event_type)
        {
        case InternalMidiEvents::kNoteOn:
          c->SetVelocityRaw(currlanecmd.event_args[2]);
          if (is_autoplay_ || currlanecmd.autoplay)
            c->Play(currlanecmd.event_args[1] /* key */);
          break;
        case InternalMidiEvents::kNoteOff:  // XXX: may not reachable
          c->SetVelocityRaw(0);
          if (is_autoplay_ || currlanecmd.autoplay)
            c->Stop(currlanecmd.event_args[1] /* key */);
          break;
        case InternalMidiEvents::kEffect:
          get_midi_channel(currlanecmd.channel)->SendEvent(currlanecmd.event_args);
          break;
        default:
          break;
        }
      }

      is_lane_updated = true;
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
  const SoundInfo &info = get_mixer()->GetSoundInfo();
  uint32_t last_play_time = (uint32_t)GetLastSoundTime() + 3000;
  size_t last_frame_offset = GetFrameFromMilisecond(last_play_time, info);

  // allocate new sound and start mixing
  s.AllocateFrame(info, last_frame_offset);
  size_t frame_offset = 0;
  size_t byte_offset = 0;
  float prev_timepoint = 0;
  for (float timepoint : mixing_timepoint_opt)
  {
    size_t new_offset = GetFrameFromMilisecond((uint32_t)timepoint, info);
    byte_offset = GetByteFromFrame((uint32_t)frame_offset, info);
    get_mixer()->MixAll((char*)s.get_ptr() + byte_offset, new_offset - frame_offset);
    Update(timepoint - prev_timepoint);
    prev_timepoint = timepoint;
    frame_offset = new_offset;
  }

  // mix remaining byte to end
  RMIXER_ASSERT(last_frame_offset >= frame_offset);
  byte_offset = GetByteFromFrame((uint32_t)frame_offset, info);
  get_mixer()->MixAll((char*)s.get_ptr() + byte_offset, last_frame_offset - frame_offset);
}

void KeySoundPoolWithTime::KeySoundProperty::Clear()
{
  memset(this, 0, sizeof(KeySoundProperty));
}

void KeySoundPoolWithTime::SetLaneChannel(unsigned lane, KeySoundProperty *prop)
{
  lane_mapping_[lane] = prop;
}

}
