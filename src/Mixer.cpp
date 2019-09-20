#include "Mixer.h"
#include "Error.h"
#include "Decoder.h"
#include "Sampler.h"
#include <algorithm>

// for sending timidity event
#include "playmidi.h"

namespace rmixer
{

void MixRecordEvent::Play(int key)
{
  is_play = 1;
  args[0] = key;
}

void MixRecordEvent::Stop(int key)
{
  is_play = 0;
  args[0] = key;
}

void MixRecordEvent::Event(int arg1, int arg2, int arg3)
{
  is_play = 2;
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
}

bool operator<(const MixRecordEvent &e1, const MixRecordEvent &e2)
{
  return e1.time < e2.time;
}

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

Channel Mixer::LoadSound(const rutil::FileData &fd)
{
  return LoadSound(fd.GetFilename(), (char*)fd.p, fd.len);
}

Channel Mixer::LoadSound(const std::string& filename,
  const char* p, size_t len)
{
  const std::string ext = rutil::upper(rutil::GetExtension(filename));
  Decoder* d = 0;
  bool r = false;
  Sound *s = nullptr;

  if (ext == "WAV") d = new Decoder_WAV(*s);
  else if (ext == "OGG") d = new Decoder_OGG(*s);
  else if (ext == "FLAC") d = new Decoder_FLAC(*s);
  else if (ext == "MP3") d = new Decoder_LAME(*s);

  // read using decoder
  r = (d && d->open(p, len) && d->read() != 0);
  delete d;
  if (!r) return -1;

  // check is resampling necessary
  if (s->get_info() != info_)
  {
    Sound *new_s = new Sound();
    Sampler sampler(*s, info_);
    sampler.Resample(*new_s);
    delete s;
    s = new_s;
  }

  // now allocate new channel
  return AllocNewChannel(s);
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

void Mixer::MixRecord(const std::vector<MixRecordEvent> &events, char **out, size_t &size)
{
  if (events.empty())
    return;

  // sort events and create buffer
  //std::sort(mixing_events_.begin(), mixing_events_.end());
  *out = (char*)calloc(1, GetByteFromMilisecond(events.back().time, info_));

  // mix iteratively - but faster method
  // --> mix interval varies from record events
  int last_time = 0;
  for (auto& e : events)
  {
    if (last_time - e.time > 10) /* milisecond */
    {
      Mix(*out, GetByteFromMilisecond(last_time - e.time, info_));
      last_time = e.time;
    }

    if (e.is_play == 0)
      e.s->Stop(e.args[0]);
    else if (e.is_play == 1)
      e.s->Play(e.args[0]);
    else if (e.is_play == 2)
    {
      SoundMidi* smidi = dynamic_cast<SoundMidi*>(e.s);
      if (smidi)
        smidi->SendEvent(e.args[0], e.args[1], e.args[2]);
    }
  }
}

void Mixer::Clear()
{
  FreeAllSound();
  UnregisterAllSound();
}

}