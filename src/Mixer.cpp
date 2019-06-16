#include "Mixer.h"
#include "Error.h"
#include <algorithm>

// for sending timidity event
#include "playmidi.h"

namespace rhythmus
{

Mixer::Mixer(const SoundInfo& info, size_t s)
  : time_ms_(0), info_(info), max_mixing_byte_size_(s), midi_(info, s)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::Mixer(const SoundInfo& info, const char* midi_cfg_path, size_t s)
  : time_ms_(0), info_(info), max_mixing_byte_size_(s), midi_(info, s, midi_cfg_path)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::~Mixer()
{
  Clear();
}

bool Mixer::RegisterSound(uint32_t channel, Sound* s, bool is_freeable)
{
  if (!s) return false;
  if (s->get_info() != info_) return false;

  // TODO: if channel is already exist, then clear that channel first
  channels_[channel] = { s, 1.0f, s->get_total_byte(), 0, false, is_freeable };
  return true;
}

void Mixer::Clear()
{
  // don't need to touch soundinfo, maybe
  time_ms_ = 0;
  mixing_record_.clear();
  midi_mixing_record_.clear();
  for (auto &ii : channels_)
    if (ii.second.is_freeable)
      delete ii.second.s;
  channels_.clear();
}


bool Mixer::Play(uint32_t channel)
{
  return channels_[channel].remain_byte = channels_[channel].total_byte;
}

// for midi command data of timidity
bool Mixer::PlayMidi(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b)
{
  // ME_NONE ignore.
  if (event_type == 0) return false;
  midi_.SendEvent(channel, event_type, a, b);
  return true;
}

// for raw midi play data
bool Mixer::PlayMidi(uint8_t c, uint8_t a, uint8_t b)
{
  const uint8_t type = midi_.GetEventTypeFromStatus(c, a, b);
  return PlayMidi(type, c & 0xf, a, b);
}

bool Mixer::PlayMidi_NoteOn(uint8_t channel, uint8_t key)
{
  return PlayMidi(ME_NOTEON, channel, key, 0 /* TODO: Velo */);
}

bool Mixer::PlayMidi_NoteOff(uint8_t channel, uint8_t key)
{
  return PlayMidi(ME_NOTEOFF, channel, key, 0 /* TODO: Velo */);
}

void Mixer::Mix(PCMBuffer& out)
{
  Mix((char*)out.AccessData(0), out.get_total_byte());
}

void Mixer::Mix(char* out, size_t size_)
{
  if (size_ > max_mixing_byte_size_)
    size_ = max_mixing_byte_size_;

  // iterate all channels and mix
  for (auto &ii : channels_)
  {
    // not playing
    if (ii.second.remain_byte == 0 && !ii.second.loop)
      continue;

    uint32_t size = size_;
    uint32_t src_byte_offset = ii.second.total_byte - ii.second.remain_byte;
    uint32_t mixed_byte;
    do {
      // check loop
      if (ii.second.loop && ii.second.remain_byte == 0)
      {
        ii.second.remain_byte = ii.second.total_byte;
        src_byte_offset = 0;
      }
      mixed_byte = ii.second.s->MixData((int8_t*)out, src_byte_offset, size, false, ii.second.volume);
      if (mixed_byte == 0)
        break;  // nothing to mix
      // update pos
      src_byte_offset += mixed_byte;
      size -= mixed_byte;
      ii.second.remain_byte -= mixed_byte;
    } while (size > 0);
  }

  // mix Midi PCM output
  midi_.GetMixedPCMData(midi_buf_, size_);
  memmix((int8_t*)out, (int8_t*)midi_buf_, size_, info_.bitsize / 8);
}


void Mixer::PlayRecord(uint32_t delay_ms, uint32_t channel)
{
  mixing_record_.emplace_back(MixingRecord{ delay_ms, channel });
}

void Mixer::PlayMidiRecord(uint32_t ms, uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b)
{
  if (event_type == 0) return;
  midi_mixing_record_.emplace_back(MidiMixingRecord{ ms, channel, event_type, a, b });
}

void Mixer::PlayMidiRecord(uint32_t ms, uint8_t c, uint8_t a, uint8_t b)
{
  const uint8_t type = midi_.GetEventTypeFromStatus(c, a, b);
  PlayMidiRecord(ms, type, c & 0xf, a, b);
}

void Mixer::PlayMidiRecord_NoteOn(uint32_t ms, uint8_t channel, uint8_t key, uint8_t volume)
{
  PlayMidiRecord(ms, ME_NOTEON, channel, key, volume /* velo */);
}

void Mixer::PlayMidiRecord_NoteOff(uint32_t ms, uint8_t channel, uint8_t key, uint8_t volume)
{
  PlayMidiRecord(ms, ME_NOTEOFF, channel, key, volume /* velo */);
}

void Mixer::MixRecord(PCMBuffer& out)
{
  for (auto &ii : mixing_record_)
  {
    Sound* s = GetSound(ii.channel);
    if (s)
      out.Mix(ii.ms, *s); // TODO: volume parameter to Mix function
  }

  if (midi_mixing_record_.size() > 0)
  {
    // sort midi events
    std::sort(midi_mixing_record_.begin(), midi_mixing_record_.end(),
      [](const MidiMixingRecord &a, const MidiMixingRecord &b) {
      return a.ms < b.ms;
    });

    constexpr size_t kEventInterval = 1024; /* less size means exact-quality, more time necessary. */
    ASSERT(max_mixing_byte_size_ > kEventInterval);
    auto midi_event_ii = midi_mixing_record_.begin();

    // mix midi PCM to pre-allocated memory (or midi file end).
    size_t midi_mix_offset = 0;
    size_t cur_time = 0;
    while (!midi_.IsMixFinish())
    {
      cur_time = GetMilisecondFromByte(midi_mix_offset, info_);
      while (midi_event_ii != midi_mixing_record_.end() && midi_event_ii->ms < cur_time)
      {
        PlayMidi(midi_event_ii->event_type, midi_event_ii->channel, midi_event_ii->a, midi_event_ii->b);
        midi_event_ii++;
      }
      size_t outsize = midi_.GetMixedPCMData(midi_buf_, kEventInterval);
      size_t mixsize = 0;
      int8_t* dst;
      while (outsize > 0 && (dst = out.AccessData(midi_mix_offset, &mixsize)))
      {
        // exit if there's no more available buffer.
        if (mixsize == 0) break;
        if (mixsize > outsize) mixsize = outsize;
        memmix(dst, (int8_t*)midi_buf_, mixsize, info_.bitsize / 8);
        midi_mix_offset += mixsize;
        outsize -= mixsize;
      }
      if (mixsize == 0) break;
    }
  }
}

/* calculate total pcm byte size by MixRecord() result. */
size_t Mixer::CalculateTotalRecordByteSize()
{
  size_t lastoffset = 0;
  for (auto &ii : mixing_record_)
  {
    Sound* s = GetSound(ii.channel);
    if (s)
    {
      size_t curlastoffset =
        GetByteFromMilisecond(ii.ms, info_) + s->get_total_byte();
      if (curlastoffset > lastoffset)
        lastoffset = curlastoffset;
    }
  }
  return lastoffset;
}

Sound* Mixer::GetSound(uint32_t channel)
{
  auto *c = GetMixChannel(channel);
  if (c)
    return c->s;
  else return 0;
}

void Mixer::SetChannelVolume(uint32_t channel, float v)
{
  auto *c = GetMixChannel(channel);
  if (c)
    c->volume = v;
}

MixChannel* Mixer::GetMixChannel(uint32_t channel)
{
  auto ii = channels_.find(channel);
  if (ii == channels_.end())
    return 0;
  else
    return &ii->second;
}

}