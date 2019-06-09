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

Midi::Midi(const SoundInfo& info, size_t buffer_size_in_byte)
  : song_(0), info_(info), buffer_size_(buffer_size_in_byte)
{
  if (midi_count++ == 0)
    mid_init(0);
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
  ApplyNewEvent(channel, ME_NOTEON, key, 0);
}

void Midi::Stop(uint8_t channel, uint8_t key)
{
  ApplyNewEvent(channel, ME_NOTEOFF, key, 0);
}

void Midi::ApplyNewEvent(uint8_t channel, uint8_t type, uint8_t a, uint8_t b)
{
  ASSERT(song_);
  /** replace current_event temporarily. TODO: MidSong should be locked. */
  MidEvent *backup = song_->current_event;
  MidEvent event;
  song_->current_event = &event;

  // create new midi event
  // song->current_sample : offset from playing sample
  // song->at : offset from file loading
  event.time = song_->current_sample;
  event.type = type;
  event.channel = channel;
  event.a = a;
  event.b = b;

  switch (type)
  {
  case ME_NOTEON:
    note_on_export(song_);
  case ME_NOTEOFF:
    note_off_export(song_);
  default:
    std::cerr << "[rhythmus::Midi] Unexpected Midi Event detected." << std::endl;
  }

  song_->current_event = backup;
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


}