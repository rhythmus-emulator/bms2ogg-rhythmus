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

bool SoundPool::LoadSound(size_t channel, const std::string& path, const char* p, size_t len)
{
  Sound* s = CreateEmptySound(channel);
  ASSERT(s);
  return s->Load(p, len, rutil::lower(rutil::GetExtension(path)));
}

void SoundPool::LoadMidiSound(size_t channel)
{
  if (!channels_ || channel >= pool_size_)
    return;
  delete channels_[channel];
  channels_[channel] = new SoundMidi();
}

void SoundPool::RegisterToMixer(Mixer& mixer)
{
  if (!channels_ || mixer_)
    return;

  for (size_t i = 0; i < pool_size_; ++i) if (channels_[i])
  {
    channels_[i]->SetSoundFormat(mixer.GetSoundInfo());
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
  : time_(0), is_autoplay_(false), loading_progress_(0), loading_finished_(true)
{
  memset(lane_idx_, 0, sizeof(lane_idx_));
}

void KeySoundPoolWithTime::LoadFromChart(rparser::Song& s, const rparser::Chart& c)
{
  using namespace rparser;
  loading_finished_ = false;
  loading_progress_ = 0.;

  // fetch song directory
  // if the song is not directory-based, nothing to read, exit.
  Directory *dir = s.GetDirectory();
  if (!dir)
    return;
  dir->SetAlternativeSearch(true);

  // load sound resources first.
  // most time consumed here, to loading_progress_ is indicates here.
  SetLaneCount(1000);
  const auto &md = c.GetMetaData();
  size_t total_count = md.GetSoundChannel()->fn.size();
  size_t curr_idx = 0;
  for (auto &ii : md.GetSoundChannel()->fn)
  {
    const char* p;
    size_t len;
    const std::string filename = ii.second;
    loading_progress_ = (double)curr_idx / total_count;
    curr_idx++;

    if (filename == "midi")
    {
      // reserved filename for midi channel
      LoadMidiSound(ii.first);
      continue;
    }

    if (!dir->GetFile(filename, &p, len))
    {
      std::cerr << "Missing sound file: " << ii.second << " (" << ii.first << ")" << std::endl;
      continue;
    }
    if (!LoadSound(ii.first, filename, p, len))
    {
      std::cerr << "Failed loading sound file: " << ii.second << " (" << ii.first << ")" << std::endl;
      continue;
    }
  }

  //
  // TODO: BGM-only lane is necessary...
  //
  KeySoundProperty ksoundprop;
  for (auto &e : c.GetEventNoteData())
  {
    if (e.subtype() != rparser::NoteEventTypes::kMIDI)
      continue;
    ksoundprop.Clear();
    e.GetMidiCommand(ksoundprop.event_args[0],
      ksoundprop.event_args[1],
      ksoundprop.event_args[2]);
    ksoundprop.time = static_cast<float>(e.GetTimePos());
    ksoundprop.autoplay = 1;
    ksoundprop.playable = 0;
    lane_time_mapping_[0].push_back(ksoundprop);
  }

  // -- Sound event
  lane_count_ = 0;
  for (auto &n : c.GetNoteData())
  {
    // XXX: lane 0 is BGM only.
    size_t lane = n.GetLane();
    if (n.type() == NoteTypes::kBGM)
      lane = 0;
    ksoundprop.Clear();
    ksoundprop.time = (float)n.time_msec;
    ksoundprop.autoplay = (n.type() == NoteTypes::kBGM) ? 1 : 0;
    ksoundprop.playable = (n.type() == NoteTypes::kTap) ? 1 : 0;
    ksoundprop.channel = n.value;
    ksoundprop.event_args[2] = 0x7F; /* volume of pcm wave as max value (p.s. ugly a little ...) */

    // in case of MIDI object
    if (n.channel_type == 1)
    {
      ksoundprop.event_args[0] = ME_NOTEON;
      ksoundprop.event_args[1] = n.effect.key;
      ksoundprop.event_args[2] = static_cast<uint8_t>(n.effect.volume * 0x7F);
      // NOTEOFF will called automatically at the end of duration
      ksoundprop.duration = n.effect.duration_ms;
    }

    lane_time_mapping_[lane].push_back(ksoundprop);
    if (lane_count_ < lane) lane_count_ = lane;
  }

  // sort keyevents by time
  for (size_t i = 0; i < lane_count_; ++i)
    std::sort(lane_time_mapping_[i].begin(), lane_time_mapping_[i].end());

  // whole load finished
  loading_progress_ = 1.0;
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

void KeySoundPoolWithTime::MoveTo(float ms)
{
  // do skipping
  for (size_t i = 0; i < lane_count_; ++i)
  {
    lane_idx_[i] = 0;
    while (lane_idx_[i] < lane_time_mapping_[i].size() &&
           lane_time_mapping_[i][lane_idx_[i]].time <= ms)
      lane_idx_[i]++;
  }
  time_ = ms;
}

void KeySoundPoolWithTime::Update(float delta_ms)
{
  time_ += delta_ms;

  // update lane-channel table
  for (size_t i = 0; i < lane_count_; ++i)
  {
    while (lane_idx_[i] < lane_time_mapping_[i].size() &&
      lane_time_mapping_[i][lane_idx_[i]].time <= time_)
    {
      SetLaneChannel(i,
        lane_time_mapping_[i][lane_idx_[i]].channel,
        lane_time_mapping_[i][lane_idx_[i]].duration,
        lane_time_mapping_[i][lane_idx_[i]].event_args[1],
        lane_time_mapping_[i][lane_idx_[i]].event_args[2] / (double)0x7F
      );
      // check for autoplay flag
      if (is_autoplay_ || lane_time_mapping_[i][lane_idx_[i]].autoplay)
        PlayLane(i);
      lane_idx_[i]++;
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
  for (size_t i = 0; i < lane_count_; ++i)
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

  // allocate new sound and start mixing
  // TODO: 3000 is ambiguous time. need to calculate real last time
  ASSERT(mixer_);
  SoundInfo info = mixer_->GetSoundInfo();
  s.SetEmptyBuffer(info, GetFrameFromMilisecond(
    (uint32_t)(mixing_timepoint_opt.back() + 10000),
    info
  ));
  size_t p_offset = 0;
  size_t p_offset_delta = 0;
  float prev_timepoint = 0;
  for (float timepoint : mixing_timepoint_opt)
  {
    p_offset_delta = GetByteFromMilisecond(timepoint, info) - p_offset;
    mixer_->Mix((char*)s.get_ptr() + p_offset, p_offset_delta);
    Update(timepoint - prev_timepoint);
    prev_timepoint = timepoint;
    p_offset += p_offset_delta;
  }
}

}