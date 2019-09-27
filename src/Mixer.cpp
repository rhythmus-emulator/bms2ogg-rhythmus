#include "Mixer.h"
#include "Error.h"
#include <algorithm>

// for sending timidity event
#include "playmidi.h"

namespace rmixer
{

// -------------------------------- class Mixer

Mixer::Mixer()
  : midi_buf_(nullptr), channel_lock_(new std::mutex()),
  midi_buffer_byte_size_(kMidiDefMaxBufferByteSize)
{
}

Mixer::Mixer(const SoundInfo& info, size_t s)
  : channel_lock_(new std::mutex()), info_(info),
    midi_buffer_byte_size_(s), midi_(info, s)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::Mixer(const SoundInfo& info, const char* midi_cfg_path, size_t s)
  : channel_lock_(new std::mutex()), info_(info),
    midi_buffer_byte_size_(s), midi_(info, s, midi_cfg_path)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::~Mixer()
{
  // better to call unregister (Mix() may be called from other thread ..?)
  UnregisterAllSound();
  delete channel_lock_;
}

void Mixer::SetSoundInfo(const SoundInfo& info)
{
  info_ = info;
  midi_.Close();
  midi_.Init(info, 0);
}

void Mixer::SetMidiBufferSize(size_t midi_buffer_byte_size)
{
  if (midi_buf_)
    delete midi_buf_;
  midi_buffer_byte_size_ = midi_buffer_byte_size;
  midi_buf_ = (char*)malloc(midi_buffer_byte_size);
}

const SoundInfo& Mixer::GetSoundInfo() const
{
  return info_;
}

void Mixer::RegisterSound(BaseSound* s)
{
  if (!s) return;
  /* As setting sound format may take long time,
   * it must be done before channel locking. */
  s->SetSoundFormat(info_);
  channel_lock_->lock();
  auto i = std::find(channels_.begin(), channels_.end(), s);
  if (i == channels_.end())
  {
    s->mixer_ = this;
    channels_.push_back(s);
  }
  channel_lock_->unlock();
}

void Mixer::UnregisterSound(BaseSound *s)
{
  if (!s || !s->mixer_) return;
  channel_lock_->lock();
  auto i = std::find(channels_.begin(), channels_.end(), s);
  if (i != channels_.end())
  {
    channels_.erase(i);
    s->mixer_ = nullptr;
  }
  channel_lock_->unlock();
}

void Mixer::UnregisterAllSound()
{
  channel_lock_->lock();
  for (auto *s : channels_)
    s->mixer_ = nullptr;
  channels_.clear();
  channel_lock_->unlock();
}

Midi* Mixer::get_midi()
{
  return &midi_;
}

void Mixer::Mix(char* out, size_t size_)
{
  // check time duration of mixing
  float time = (float)GetMilisecondFromByte(size_, info_);

  // iterate all channels and mix & update
  channel_lock_->lock();
  for (BaseSound *s : channels_)
  {
    s->MixDataTo((int8_t*)out, size_);
    s->Update(time);
  }
  channel_lock_->unlock();

  // mix Midi PCM output
  if (midi_buf_)
  {
    const size_t midi_mix_size = std::min(midi_buffer_byte_size_, size_);
    midi_.GetMixedPCMData(midi_buf_, midi_mix_size);
    memmix((int8_t*)out, (int8_t*)midi_buf_, midi_mix_size, info_.bitsize / 8);
  }
}

}