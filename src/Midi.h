#ifndef RENCODER_MIDI_H
#define RENCODER_MIDI_H

#include "Sound.h"

struct _MidSong;
typedef struct _MidSong MidSong;
struct _MidIStream;
typedef struct _MidIStream MidIStream;

namespace rhythmus
{

class Midi
{
public:
  /**
   * SoundInfo : output PCM format.
   * buffer_size_in_byte : maximum PCM output buffer size.
   */
  Midi(const SoundInfo& info, size_t buffer_size_in_byte);
  ~Midi();

  bool LoadFile(const char* filename);
  void Play(uint8_t channel, uint8_t key);
  void Stop(uint8_t channel, uint8_t key);
  void SendEvent(uint8_t channel, uint8_t type, uint8_t a, uint8_t b);
  void SetVolume(float v);

  void ClearEvent();
  void MixRestart();
  bool IsMixFinish();
  size_t GetMixedPCMData(char* outbuf, size_t size);

private:
  static int midi_count;
  MidSong *song_;
  SoundInfo info_;
  size_t buffer_size_;

  bool Init(MidIStream *stream);
  void Close();
};

}

#endif