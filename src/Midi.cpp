#include "Midi.h"
#include "Error.h"
#include "rutil.h"
#include <iostream>

#define TIMIDITY_STATIC
#define TIMIDITY_BUILD
#include "timidity.h"
#include "timidity_internal.h"
#include "timidity_realtime.h"
#include "playmidi.h"

namespace rhythmus
{

int Midi::midi_count = 0;

Midi::Midi(const SoundInfo& info, size_t buffer_size_in_byte, const char* midi_cfg_path)
  : song_(0), info_(info), buffer_size_(buffer_size_in_byte)
{
  if (midi_count++ == 0)
  {
    if (!midi_cfg_path || !mid_init(midi_cfg_path))
      mid_init_no_config();
  }
  ASSERT(Init(0));
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

/* refere : read_midi_event in readmidi.c */

void Midi::Play(uint8_t channel, uint8_t key)
{
  SendEvent(channel, ME_NOTEON, key, 0);
}

void Midi::Stop(uint8_t channel, uint8_t key)
{
  SendEvent(channel, ME_NOTEOFF, key, 0);
}

void Midi::SendEvent(uint8_t channel, uint8_t type, uint8_t a, uint8_t b)
{
  ASSERT(song_);

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
  mid_song_set_volume(song_, v);
}

bool Midi::Init(MidIStream *stream)
{
  MidSongOptions options;
  options.rate = info_.rate;
  options.format = (info_.bitsize == 16) ? MID_AUDIO_S16LSB : MID_AUDIO_U8;
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

void Midi::MixRestart()
{
  // set current sample to 0 and enable playing flag
  mid_song_start(song_);
}

void Midi::Close()
{
  if (song_)
  {
    mid_song_free(song_);
    song_ = 0;
  }
}

void Midi::ClearEvent()
{
  // clear all midi event.
}

bool Midi::IsMixFinish()
{
  return song_->playing == 0;
}

size_t Midi::GetMixedPCMData(char* outbuf, size_t size)
{
  return mid_song_read_wave(song_, (sint8*)outbuf, size);
}

/* refer: readmidi.c : read_midi_event */
uint8_t Midi::GetEventTypeFromStatus(uint8_t status, uint8_t a)
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
    case 32: break; /* ignored */
      /* TODO: I didn't implemented PITCH SENSITIVITY related
         as I don't know it's neither its spec nor its implementation
         is proper or not... */
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


}