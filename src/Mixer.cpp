#include "Mixer.h"
#include "Error.h"
#include <algorithm>
#include <memory.h>
#include <string.h>

// for sending timidity event
#include "playmidi.h"

namespace rmixer
{

// ------------------------------ class Channel

Channel::Channel(ChannelIndex chidx)
  : chidx_(chidx), groupidx_(0), sound_(nullptr),
    volume_(1.0f), loop_(0), is_paused_(false), is_occupied_(false),
    frame_pos_(0), effect_length_(0), effect_remain_(0),
    pitch_(1.0f), speed_(1.0f), reverb_(0.0f),
    is_virtual_(false), sound_level_(0.0f), priority_(0) {}

ChannelIndex Channel::get_channel_index() const { return chidx_; }

void Channel::SetSound(Sound *sound)
{
  // reset playback information and set sound
  Stop();
  sound_ = sound;
}

void Channel::SetChannelGroup(unsigned groupidx)
{
  groupidx_ = groupidx;
}

void Channel::SetVolume(float volume)
{
  volume_ = volume;
}

void Channel::Pause()
{
  is_paused_ = true;
}

void Channel::Play()
{
  /* if paused, then play from paused position. */
  if (is_paused_)
  {
    is_paused_ = false;
    return;
  }
  Play(1);
}

void Channel::Play(int loop_count)
{
  if (loop_count < 0)
    loop_count = 0;
  /* restart sample even if it had been paused. */
  loop_ = loop_count;
  is_paused_ = false;
  frame_pos_ = 0;
  effect_length_ = effect_remain_ = 0;
}

void Channel::Stop()
{
  Play(0);
}

void Channel::SetFadePoint(unsigned milisecond)
{
  effect_length_ = effect_remain_ = milisecond;
}

/* @brief lock channel to prevent channel is occupied by other sound. */
void Channel::LockChannel()
{
  is_occupied_ = true;
}

void Channel::UnlockChannel()
{
  is_occupied_ = false;
}

/**
 * @param out buffer to write output PCM data
 * @param len PCM byte size
 */
void Channel::Copy(char *out, size_t frame_len)
{
  float volume_final = volume_;
  size_t mixsize = 0;
  if (!sound_) return;
  if (!sound_->is_loaded() || !is_playing() || volume_final < .0f)
  {
    size_t len = sound_->GetByteFromFrame(frame_len);
    memset(out, 0, len);
    return;
  }
  if (effect_length_ > 0)
  {
    volume_final *= 1.0f - (float)effect_remain_ / effect_length_;
  }
  if (volume_final >= 1.0f) volume_final = 1.0f;
  while (mixsize < frame_len && loop_ > 0)
  {
    if (volume_final == 1.0f)
      mixsize += sound_->Copy((int8_t*)out, &frame_pos_, frame_len);
    else
      mixsize += sound_->CopyWithVolume((int8_t*)out, &frame_pos_, frame_len, volume_final);
    frame_pos_ += mixsize;
    if (frame_pos_ >= sound_->get_frame_count())
    {
      loop_--;
      frame_pos_ = 0;
    }
  }
  if (mixsize < frame_len)
  {
    // memset remaining memory
    memset((int8_t*)out + sound_->GetByteFromFrame(frame_pos_), 0,
           sound_->GetByteFromFrame(frame_len - mixsize));
  }
}

/**
 * @param out buffer to write output PCM data
 * @param len PCM byte size
 */
void Channel::Mix(char *out, size_t frame_len)
{
  float volume_final = volume_;
  size_t mixsize = 0;
  if (!sound_) return;
  if (!sound_->is_loaded() || !is_playing() || volume_final < .0f)
    return;
  if (effect_length_ > 0)
  {
    volume_final *= 1.0f - (float)effect_remain_ / effect_length_;
  }
  if (volume_final >= 1.0f) volume_final = 1.0f;
  while (mixsize < frame_len && loop_ > 0)
  {
    if (volume_final == 1.0f)
      mixsize += sound_->Mix((int8_t*)out, &frame_pos_, frame_len);
    else
      mixsize += sound_->MixWithVolume((int8_t*)out, &frame_pos_, frame_len, volume_final);
    // already updated by Sound::Mix(..) method
    //frame_pos_ += mixsize;
    if (!sound_->is_streaming() /* streaming sound must be played continously */ &&
        frame_pos_ >= sound_->get_frame_count())
    {
      loop_--;
      frame_pos_ = 0;
    }
  }
}

void Channel::UpdateBySample(size_t sample)
{
  float sound_level = sound_->GetSoundLevel(frame_pos_, sample);
}

void Channel::UpdateByByte(size_t byte)
{
  // not implemented
  RMIXER_ASSERT(0);
}

Sound *Channel::get_sound() { return sound_; }
const Sound *Channel::get_sound() const { return sound_; }
float Channel::volume() const { return volume_; }
bool Channel::is_playing() const { return loop_ > 0; }
bool Channel::is_occupied() const { return is_occupied_; }
bool Channel::is_virtual() const { return is_virtual_; }
bool Channel::is_loaded() const { return !sound_ || sound_->is_loaded(); }

float Channel::get_playtime_ms() const
{
  return (float)GetMilisecondFromFrame(frame_pos_, sound_->get_soundinfo());
}

// -------------------------------- class Mixer

Mixer::Mixer()
  : channel_lock_(new std::mutex()), cache_sound_(true), maximum_audio_count_(-1),
    midi_(nullptr), midi_config_path_(nullptr), midi_sound_(nullptr), midi_channel_(nullptr)
{
  memset(audible_channels_, 0, sizeof(audible_channels_));
}

Mixer::Mixer(const SoundInfo& info, ChannelIndex max_channel_size)
  : info_(info), channel_lock_(new std::mutex()), cache_sound_(true), maximum_audio_count_(-1),
    midi_(nullptr), midi_config_path_(nullptr), midi_sound_(nullptr), midi_channel_(nullptr)
{
  memset(audible_channels_, 0, sizeof(audible_channels_));
  SetMaxChannelSize(max_channel_size);
}

Mixer::~Mixer()
{
  ClearMidi();
  channel_lock_->lock();
  for (auto *c : channels_)
    delete c;
  for (auto *s : sounds_)
    delete s;
  channel_lock_->unlock();
  delete channel_lock_;
}

void Mixer::SetSoundInfo(const SoundInfo& info)
{
  info_ = info;
  // update previous audio data
  for (auto *s : sounds_)
    s->SetSoundFormat(info);
  InitializeMidi();
}

const SoundInfo& Mixer::GetSoundInfo() const
{
  return info_;
}

void Mixer::SetMaxChannelSize(ChannelIndex max_channel_size)
{
  channel_lock_->lock();
  if (max_channel_size < channels_.size())
  {
    for (size_t i = max_channel_size; i < channels_.size(); ++i)
      delete channels_[i];
    channels_.resize(max_channel_size);
  }
  else if (max_channel_size > channels_.size())
  {
    size_t old_size = channels_.size();
    channels_.resize(max_channel_size);
    for (size_t i = old_size; i < max_channel_size; ++i)
      channels_[i] = new Channel(i);
  }
  channel_lock_->unlock();
}

ChannelIndex Mixer::GetMaxChannelSize() const
{
  return channels_.size();
}

void Mixer::SetMaxAudioSize(int max_audio_size)
{
  maximum_audio_count_ = max_audio_size;
  if (maximum_audio_count_ > kMaxAudibleChannelCount)
    maximum_audio_count_ = kMaxAudibleChannelCount;
}

int Mixer::GetMaxAudioSize() const
{
  return maximum_audio_count_;
}

void Mixer::SetCacheSound(bool cache_sound)
{
  cache_sound_ = cache_sound;
}

Sound* Mixer::CreateSound(const char *filepath, bool loadasync)
{
  Sound *s;
  if (!filepath)
    return nullptr;
  // search for pre-registered sound
  if (cache_sound_)
  {
    channel_lock_->lock();
    for (auto *ss : sounds_)
      if (strcmp(ss->name().c_str(), filepath) == 0)
        return ss;
    channel_lock_->unlock();
  }
  s = new Sound();
  if (!s->Load(filepath))
  {
    delete s;
    return nullptr;
  }
  s->set_name(filepath);
  s->SetSoundFormat(info_);
  if (cache_sound_)
  {
    channel_lock_->lock();
    sounds_.push_back(s);
    channel_lock_->unlock();
  }
  return s;
}

Sound* Mixer::CreateSound(const char *p, size_t len, const char *filename, bool loadasync)
{
  Sound *s;
  // search for pre-registered sound
  if (cache_sound_ && filename != nullptr && *filename)
  {
    channel_lock_->lock();
    for (auto *ss : sounds_)
      if (strcmp(ss->name().c_str(), filename) == 0)
        return ss;
    channel_lock_->unlock();
  }
  s = new Sound();
  const char *ext = nullptr;
  if (filename)
  {
    for (const char *p = filename; *p; ++p)
      if (*p == '.') ext = p + 1;
  }
  if (!s->Load(p, len, ext))
  {
    delete s;
    return nullptr;
  }
  if (filename)
    s->set_name(filename);
  s->SetSoundFormat(info_);
  if (cache_sound_)
  {
    channel_lock_->lock();
    sounds_.push_back(s);
    channel_lock_->unlock();
  }
  return s;
}

void Mixer::DeleteSound(Sound *sound)
{
  auto i = std::find(sounds_.begin(), sounds_.end(), sound);
  if (i != sounds_.end())
  {
    sounds_.erase(i);
  }
  StopSound(sound);
  delete sound;
}

Channel* Mixer::PlaySound(Sound *sound, bool start)
{
  // search for empty channel
  // if empty, then allocate sound to that channel.
  for (auto *c : channels_)
  {
    if (!c->is_playing() && !c->is_occupied())
    {
      c->SetSound(sound);
      if (start)
        c->Play();
      return c;
    }
  }
  // sound may failed to play, no empty channel.
  return nullptr;
}

void Mixer::StopSound(Sound *sound)
{
  for (auto *c : channels_)
    if (c->get_sound() == sound) c->Stop();
}

void Mixer::Play(ChannelIndex channel) { if (channel < channels_.size()) channels_[channel]->Play(); }
void Mixer::Stop(ChannelIndex channel) { if (channel < channels_.size()) channels_[channel]->Stop(); }

void Mixer::InitializeMidi(const char* midi_config_path)
{
  midi_config_path_ = midi_config_path;
  InitializeMidi();
}

void Mixer::InitializeMidi()
{
  if (midi_)
  {
    ClearMidi();
  }
  midi_ = new Midi(0x00010000, midi_config_path_);
  if (!midi_->LoadFromStream())
  {
    ClearMidi();
    RMIXER_THROW("Failed to initialize midi object in streaming mode");
  }
  /* from here, MIDI audio MUST be created */
  midi_sound_ = new MidiSound(GetSoundInfo(), midi_);
  if (!midi_channel_)
    midi_channel_ = PlaySound(midi_sound_, true);
  else
    midi_channel_->SetSound(midi_sound_);
  if (!midi_sound_ || !midi_channel_)
    RMIXER_FATAL("Midi Initialization failed and it's unrecoverable.");
}

void Mixer::ClearMidi()
{
  if (!midi_) return;
  if (midi_channel_)
  {
    midi_channel_->SetSound(nullptr);
    midi_channel_ = nullptr;  /* should destroy at channel destruction */
  }
  delete midi_sound_;
  delete midi_;
  midi_ = nullptr;
  midi_sound_ = nullptr;
}

void Mixer::SendMidiCommand(unsigned channel, uint8_t command, uint8_t param1)
{
  if (!midi_) return;
}

void Mixer::PressMidiKey(unsigned channel, unsigned key, unsigned volume)
{
  if (!midi_) return;
}

void Mixer::ReleaseMidiKey(unsigned channel, unsigned key)
{
  if (!midi_) return;
}

void Mixer::Update()
{
  // don't update virtual level is audio count is not set
  // (only update priority if it is playing)
  if (maximum_audio_count_ == -1)
  {
    size_t i = 0;
    for (auto *c : channels_)
    {
      /*
      c->priority_ = 0;
      c->is_virtual_ = false;
      */
      if (c->is_playing())
        audible_channels_[i++] = c;
      if (i >= kMaxAudibleChannelCount)
        break;
    }
    for (; i < kMaxAudibleChannelCount; ++i)
      audible_channels_[i] = 0;
    return;
  }
  else
  {
    // update sound level for all channels (warn: costly)
    for (auto *c : channels_)
      c->UpdateBySample(128);

    // set virtual flag to all audio
    // we use previously sampled audio volume while calculated during mixing,
    // supposing not much time difference between sampling.
    // priority is set by these guide:
    // 1. audible & playing
    // 2. only playing, not audible
    // 3. not playing
    size_t priority = 0;
    auto sortchannelarray = channels_;
    auto sortfunc = [](const Channel *c1, const Channel *c2) {
      return c1->is_playing() * c1->sound_level_ < c2->is_playing() * c2->sound_level_;
    };
    sortchannelarray.push_back(midi_channel_);
    std::sort(sortchannelarray.begin(), sortchannelarray.end(), sortfunc);
    for (auto *c : sortchannelarray)
    {
      if (c->priority_ != -1)
      {
        c->priority_ = (int)priority;
        c->is_virtual_ = (c->priority_ > maximum_audio_count_);
        ++priority;
      }
    }

    // swap out audible channels.
    unsigned i = 0, j = 0;
    for (; i < (unsigned)maximum_audio_count_; ++i)
    {
      for (; j < channels_.size(); ++j)
      {
        if (!sortchannelarray[j]->is_virtual())
        {
          audible_channels_[i] = sortchannelarray[j++];
          break;
        }
      }
    }
    for (; i < kMaxAudibleChannelCount; ++i)
      audible_channels_[i] = 0;
  }

}

void Mixer::Mix(char* out, size_t frame_len, ChannelIndex channel_index)
{
  // XXX: need channel lock?
  if (channel_index >= channels_.size()) return;
  channels_[channel_index]->Mix(out, frame_len);
}

void Mixer::Copy(char* out, size_t frame_len, ChannelIndex channel_index)
{
  // XXX: need channel lock?
  if (channel_index >= channels_.size()) return;
  channels_[channel_index]->Copy(out, frame_len);
}

void Mixer::MixAll(char* out, size_t frame_len)
{
  channel_lock_->lock();
  if (maximum_audio_count_ == -1)
  {
    for (auto *c : channels_)
      c->Mix(out, frame_len);
  }
  else
  {
    for (int i = 0; i < maximum_audio_count_; ++i)
      audible_channels_[i]->Mix(out, frame_len);
  }
  channel_lock_->unlock();
}

Midi* Mixer::get_midi()
{
  return midi_;
}

Sound* Mixer::get_midi_sound()
{
  return midi_sound_;
}

Channel* Mixer::get_midi_channel()
{
  return midi_channel_;
}

}
