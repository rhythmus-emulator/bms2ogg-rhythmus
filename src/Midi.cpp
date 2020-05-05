#include "Midi.h"
#include "Error.h"
#include "rparser.h"  /* for rutil::fopen_utf8 */
#include <iostream>
#include <memory.h>

#define TIMIDITY_STATIC
#define TIMIDITY_BUILD
#include "timidity.h"
#include "timidity_internal.h"
#include "timidity_realtime.h"
#include "playmidi.h"

namespace rmixer
{

// -------------------------- class MidiChannel

MidiChannel::MidiChannel()
  : midi_(nullptr), channel_(0), default_key_(0), volume_(0x7F) {}

void MidiChannel::SetVelocity(float v)
{
  volume_ = static_cast<uint8_t>(v * 0x7F);
}

void MidiChannel::SetVelocityRaw(uint8_t v)
{
  volume_ = v;
}

void MidiChannel::SetDefaultKey(uint8_t key)
{
  default_key_ = key;
}

void MidiChannel::Play(uint8_t key)
{
  midi_->SendEvent(channel_, ME_NOTEON, key, volume_);
}

void MidiChannel::Stop(uint8_t key)
{
  midi_->SendEvent(channel_, ME_NOTEOFF, key, 0);
}

void MidiChannel::Play()
{
  midi_->SendEvent(channel_, ME_NOTEON, default_key_, volume_);
}

void MidiChannel::Stop()
{
  midi_->SendEvent(channel_, ME_NOTEOFF, default_key_, 0);
}

void MidiChannel::SendEvent(uint8_t *e)
{
  midi_->SendEvent(channel_, e[0], e[1], e[2]);
}


// --------------------------------- class Midi

int Midi::midi_count = 0;

Midi::Midi(size_t buffer_size_in_byte, const char* midi_cfg_path)
  : song_(0), buffer_size_(buffer_size_in_byte), nrpn_(0)
{
  if (midi_count++ == 0)
  {
    if (!midi_cfg_path || mid_init(midi_cfg_path) != 0)
    {
      std::cerr << "[Midi] No Soundfont, midi sound may be muted." << std::endl;
      mid_init_no_config();
    }
  }

  for (unsigned i = 0; i < kMidiMaxChannel; ++i)
  {
    ch_[i].midi_ = this;
    ch_[i].channel_ = (uint8_t)i;
  }
  memset(rpn_lsb_, 0, sizeof(rpn_lsb_));
  memset(rpn_msb_, 0, sizeof(rpn_msb_));
}

Midi::Midi(const SoundInfo& info, size_t buffer_size_in_byte, const char* midi_cfg_path)
  : song_(0), info_(info), buffer_size_(buffer_size_in_byte), nrpn_(0)
{
  if (midi_count++ == 0)
  {
    if (!midi_cfg_path || mid_init(midi_cfg_path) != 0)
    {
      std::cerr << "[Midi] No Soundfont, midi sound may be muted." << std::endl;
      mid_init_no_config();
    }
  }

  for (unsigned i = 0; i < kMidiMaxChannel; ++i)
  {
    ch_[i].midi_ = this;
    ch_[i].channel_ = (uint8_t)i;
  }
  memset(rpn_lsb_, 0, sizeof(rpn_lsb_));
  memset(rpn_msb_, 0, sizeof(rpn_msb_));
}

Midi::~Midi()
{
  Close();
  if (--midi_count == 0)
    mid_exit();
}

bool Midi::LoadFile(const char* filename)
{
  Close();
  FILE *fp = rutil::fopen_utf8(filename, "r");
  if (!fp) return false;
  MidIStream *stream = mid_istream_open_fp(stdin, 0);
  Init(stream);
  mid_istream_close(stream);
  return true;
}

bool Midi::LoadFromStream()
{
  Close();
  return Init(0);
}

void Midi::Close()
{
  if (song_)
  {
    mid_song_free(song_);
    song_ = 0;
  }
}

void Midi::Restart()
{
  if (!song_)
    return;

  // set current sample to 0 and enable playing flag
  mid_song_start(song_);
}

/* refere : read_midi_event in readmidi.c */

void Midi::Play(uint8_t channel, uint8_t key)
{
  ch_[channel].Play(key);
}

void Midi::Stop(uint8_t channel, uint8_t key)
{
  ch_[channel].Stop(key);
}

MidiChannel *Midi::GetChannel(uint8_t channel)
{
  return &ch_[channel];
}

void Midi::SendEvent(uint8_t channel, uint8_t type, uint8_t a, uint8_t b)
{
  if (!song_) return;

  // won't support channel over 16
  RMIXER_ASSERT_M(channel < 16, "Midi isn't supporting channel over 16");

  // create new midi event
  // song->current_sample : offset from playing sample
  // song->at : offset from file loading
  MidEvent event;
  event.time = song_->current_sample;
  event.type = type;
  event.channel = channel;
  event.a = a;
  event.b = b;

  send_event(song_, &event);
}

void Midi::SetVolume(float v)
{
  mid_song_set_volume(song_, (int)(v * 800));
}

bool Midi::Init(const SoundInfo& info, MidIStream *stream)
{
  info_ = info;
  return Init(stream);
}

bool Midi::Init(MidIStream *stream)
{
  MidSongOptions options;
  options.rate = info_.rate;
  /* XXX: should use MSB format in case of BE? */
  if (info_.is_signed)
  {
    switch (info_.bitsize)
    {
    case 8:
      options.format = MID_AUDIO_U8;
      break;
    case 16:
      options.format = MID_AUDIO_U16;
      break;
    default:
      RMIXER_THROW("Unsupported Midi bitsize.");
    }
  }
  else
  {
    switch (info_.bitsize)
    {
    case 8:
      options.format = MID_AUDIO_S8;
      break;
    case 16:
      options.format = MID_AUDIO_S16;
      break;
    default:
      RMIXER_THROW("Unsupported Midi bitsize.");
    }
  }
  options.channels = info_.channels;
  // frame size
  options.buffer_size = buffer_size_ / (info_.bitsize * info_.channels / 8);

  if (stream)
    song_ = mid_song_load(stream, &options);
  else
    song_ = mid_song_for_stream(&options);  // "empty midi stream" for real-time playing

  mid_song_start(song_);

  return true;
}

void Midi::ClearEvent()
{
  // clear all midi event. (TODO)
}

bool Midi::IsMixFinish()
{
  if (!song_) return true;
  return song_->playing == 0;
}

size_t Midi::GetMixedPCMData(char* outbuf, size_t size)
{
  if (!song_) return 0;
  return mid_song_read_wave(song_, (sint8*)outbuf, size);
}

/* refer: readmidi.c : read_midi_event */
uint8_t Midi::GetEventTypeFromStatus(uint8_t status, uint8_t &a, uint8_t &b)
{
  uint8_t channel;
  uint8_t stat_group;

  // ignore SysEx & Meta event
  if (status == 0xF0 || status == 0xF7 || status == 0xFF)
    return 0;

  // check if status byte
  if (status & 0x80)
  {
    channel = status & 0x0F;
    stat_group = (status >> 4) & 0x07;
    a &= 0x7F;
  }
  else return 0;

  switch (stat_group)
  {
  case 0: return ME_NOTEOFF;
  case 1: return ME_NOTEON;
  case 2: return ME_KEYPRESSURE;
  case 3: /* 0xB0 Channel Mode Messages */
  {
    switch (a)
    {
    case 7: return ME_MAINVOLUME;
    case 10: return ME_PAN;
    case 11: return ME_EXPRESSION;
    case 64: return ME_SUSTAIN;
    case 120: return ME_ALL_SOUNDS_OFF;
    case 121: return ME_RESET_CONTROLLERS;
    case 123: return ME_ALL_NOTES_OFF;
    case 0: return ME_TONE_BANK;
    case 32: /* ignored */
      /* TODO: I didn't implemented PITCH SENSITIVITY related
         as I don't know it's neither its spec nor its implementation
         is proper or not... */
      break;
    case 100: nrpn_ = 0; rpn_msb_[channel] = b; break;
    case 101: nrpn_ = 0; rpn_lsb_[channel] = b; break;
    case 99: nrpn_ = 1; rpn_msb_[channel] = b; break;
    case 98: nrpn_ = 1; rpn_lsb_[channel] = b; break;
    case 6:
      if (nrpn_) break; /* ignored? */
      switch ((rpn_msb_[channel] << 8) | rpn_lsb_[channel])
      {
      case 0x0000:
        return ME_PITCH_SENS;
      case 0x7F7F:
        a = 2;
        b = 0;
        return ME_PITCH_SENS;
      }
      break;
    default:
      // REMed midi status warning ... some file gives too much error.
      //std::cerr << "[Midi] unrecognized midi status command : " << (int)status << "," << (int)a << std::endl;
      return 0;
    }
    break;
  }
  case 4: return ME_PROGRAM;
  case 5: std::cerr << "[Midi] Channel pressure is not implemented." << std::endl; break;
  case 6: return ME_PITCHWHEEL;
  }

  return 0;
}

const SoundInfo& Midi::get_soundinfo() const
{
  return info_;
}


// ---------------------------- class MidiSound

MidiSound::MidiSound(const SoundInfo& info, Midi *midi) : Sound(info, 65536), midi_(midi) {}

size_t MidiSound::Mix(int8_t *copy_to, size_t *offset, size_t frame_len) const
{
  const_cast<MidiSound*>(this)->CreateMidiData(frame_len);
  return Sound::Mix(copy_to, offset, frame_len);
}

size_t MidiSound::MixWithVolume(int8_t *copy_to, size_t *offset, size_t frame_len, float volume) const
{
  const_cast<MidiSound*>(this)->CreateMidiData(frame_len);
  return Sound::MixWithVolume(copy_to, offset, frame_len, volume);
}

size_t MidiSound::Copy(int8_t *p, size_t *offset, size_t frame_len) const
{
  const_cast<MidiSound*>(this)->CreateMidiData(frame_len);
  return Sound::Copy(p, offset, frame_len);
}

size_t MidiSound::CopyWithVolume(int8_t *p, size_t *offset, size_t frame_len, float volume) const
{
  const_cast<MidiSound*>(this)->CreateMidiData(frame_len);
  return Sound::CopyWithVolume(p, offset, frame_len, volume);
}

void MidiSound::CreateMidiData(size_t frame_len)
{
  // check if resampling is necessary
  if (midi_->get_soundinfo() == get_soundinfo())
  {
    // resize buffer if previously allocated buffer is not enough
    size_t req_buffer_size = GetByteFromSample(frame_len);
    if (get_total_byte() < req_buffer_size)
    {
      size_t newsize = get_total_byte() << 1;
      while (newsize < req_buffer_size) newsize <<= 1;
      AllocateSize(get_soundinfo(), newsize);
    }
    midi_->GetMixedPCMData((char*)get_ptr(), req_buffer_size);
  }
  else
  {
    size_t req_buffer_size = rmixer::GetByteFromFrame(frame_len, midi_->get_soundinfo());
    Sound s(midi_->get_soundinfo(), req_buffer_size);
    midi_->GetMixedPCMData((char*)s.get_ptr(), req_buffer_size);
    if (s.Resample(get_soundinfo()))
      swap(s);
    else
      memset(get_ptr(), 0, GetByteFromFrame(frame_len));
  }
  frame_size_ = frame_len;
  buffer_size_ = GetByteFromFrame(frame_len);
}

}
