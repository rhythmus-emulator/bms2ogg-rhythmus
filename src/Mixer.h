#ifndef RMIXER_MIXER_H_
#define RMIXER_MIXER_H_

#include "Sound.h"
#include "Midi.h"
#include "rutil.h"
#include <mutex>
#include <vector>
#include <map>

namespace rmixer
{

/**
 * @brief
 * Contains multiple sound data for mixing.
 * Multi-thread safe.
 *
 * @param info mixing pcm sound spec
 * @param max_buffer_byte_size max buffer size for midi mixing
 */
class Mixer
{
public:
  Mixer();
  Mixer(const SoundInfo& info,
    size_t midi_buffer_byte_size = kMidiDefMaxBufferByteSize);
  Mixer(const SoundInfo& info, const char* midi_cfg_path,
    size_t midi_buffer_byte_size = kMidiDefMaxBufferByteSize);
  ~Mixer();

  /* for lazy-initialization */
  void SetSoundInfo(const SoundInfo& info);

  /* for midi lazy-initialization */
  void SetMidiBufferSize(size_t midi_buffer_byte_size);

  const SoundInfo& GetSoundInfo() const;

  /* @brief Add sound to mixer object (no ownership) */
  void RegisterSound(BaseSound* s);

  /* @brief Detach sound from mixer object */
  void UnregisterSound(BaseSound *s);

  void UnregisterAllSound();

  /* @brief mix to specified byte */
  void Mix(char* out, size_t size);

  Midi* get_midi();

private:
  SoundInfo info_;

  std::mutex* channel_lock_;

  /* @brief registered sound objects (only mix, not released) */
  std::vector<BaseSound*> channels_;

  size_t midi_buffer_byte_size_;
  Midi midi_;
  char* midi_buf_;
};

}

#endif