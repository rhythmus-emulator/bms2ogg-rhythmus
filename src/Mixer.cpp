#include "Mixer.h"
#include "Error.h"
#include <algorithm>

// for sending timidity event
#include "playmidi.h"

namespace rmixer
{

// -------------------------------- class Mixer

Mixer::Mixer()
  : max_mixing_byte_size_(kMidiDefMaxBufferByteSize)
{
}

Mixer::Mixer(const SoundInfo& info, size_t s)
  : info_(info), max_mixing_byte_size_(s), midi_(info, s)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::Mixer(const SoundInfo& info, const char* midi_cfg_path, size_t s)
  : info_(info), max_mixing_byte_size_(s), midi_(info, s, midi_cfg_path)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::~Mixer()
{
  Clear();
}

void Mixer::SetSoundInfo(const SoundInfo& info)
{
  info_ = info;
  midi_.Close();
  midi_.Init(info, 0);
}

const SoundInfo& Mixer::GetSoundInfo() const
{
  return info_;
}

Channel Mixer::LoadSound(const rutil::FileData &fd)
{
  return LoadSound(fd.GetFilename(), (char*)fd.p, fd.len);
}

Channel Mixer::LoadSound(const std::string& filename,
  const char* p, size_t len)
{
  Channel ch = AllocNewChannel();
  Sound *s = GetSound(ch);
  bool r = s->Load(filename);
  if (!r)
  {
    FreeSound(ch);
    return -1;
  }
  return ch;
}

Channel Mixer::LoadSound(const std::string& filepath)
{
  rutil::FileData fd;
  rutil::ReadFileData(filepath, fd);
  if (fd.IsEmpty())
    return false;
  return LoadSound(fd);
}

Channel Mixer::LoadSound(Sound* s)
{
  if (!s) return false;
  if (s->get_info() != info_) return false;
  return AllocNewChannel(s);
}

Sound* Mixer::GetSound(Channel channel)
{
  if (channel < 0 || channel >= channels_.size())
    return nullptr;
  return channels_[channel];
}

void Mixer::FreeSound(Channel channel)
{
  if (channel < 0 || channel >= channels_.size())
    return;

  channel_lock_.lock();
  delete channels_[channel];
  channels_[channel] = 0;
  channel_lock_.unlock();
}

void Mixer::FreeAllSound()
{
  channel_lock_.lock();
  for (auto *p : channels_)
    delete p;
  channels_.clear();
  channel_lock_.unlock();
}

void Mixer::RegisterSound(Sound* s)
{
  channel_lock_.lock();
  auto i = std::find(channels_reg_.begin(), channels_reg_.end(), s);
  if (i == channels_reg_.end())
    channels_reg_.push_back(s);
  channel_lock_.unlock();
}

void Mixer::UnregisterSound(const Sound *s)
{
  channel_lock_.lock();
  auto i = std::find(channels_reg_.begin(), channels_reg_.end(), s);
  if (i != channels_reg_.end())
    channels_reg_.erase(i);
  channel_lock_.unlock();
}

void Mixer::UnregisterAllSound()
{
  channel_lock_.lock();
  channels_reg_.clear();
  channel_lock_.unlock();
}

Midi* Mixer::get_midi()
{
  return &midi_;
}

void Mixer::Play(uint16_t channel, int key)
{
  channels_[channel]->Play(key);
}

void Mixer::Stop(uint16_t channel, int key)
{
  channels_[channel]->Stop(key);
}

int Mixer::AllocNewChannel()
{
  return AllocNewChannel(new Sound());
}

int Mixer::AllocNewChannel(Sound *s)
{
  channel_lock_.lock();
  // attempt to reuse empty channel
  for (int i = 0; i < channels_.size(); ++i)
  {
    if (!channels_[i])
    {
      channels_[i] = s;
      channel_lock_.unlock();
      return i;
    }
  }

  // If empty channel not found, then create new channel
  channels_.push_back(s);
  channel_lock_.unlock();
  return (int)channels_.size() - 1;
}

void Mixer::Mix(char* out, size_t size_)
{
  if (size_ > max_mixing_byte_size_)
    size_ = max_mixing_byte_size_;

  // iterate all channels and mix
  channel_lock_.lock();
  for (Sound *s : channels_)
  {
    if (s->IsEmpty()) continue;
    s->MixDataTo((int8_t*)out, size_);
  }
  for (Sound *s : channels_reg_)
  {
    if (s->IsEmpty()) continue;
    s->MixDataTo((int8_t*)out, size_);
  }
  channel_lock_.lock();

  // mix Midi PCM output
  midi_.GetMixedPCMData(midi_buf_, size_);
  memmix((int8_t*)out, (int8_t*)midi_buf_, size_, info_.bitsize / 8);
}

void Mixer::Clear()
{
  FreeAllSound();
  UnregisterAllSound();
}

}