#include "Mixer.h"
#include "Error.h"

namespace rhythmus
{

Mixer::Mixer(const SoundInfo& info)
  : time_ms_(0), info_(info)
{
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
  for (auto &ii : channels_)
    if (ii.second.is_freeable)
      delete ii.second.s;
  channels_.clear();
}


bool Mixer::Play(uint32_t channel)
{
  return channels_[channel].remain_byte = channels_[channel].total_byte;
}

void Mixer::Mix(PCMBuffer& out)
{
  Mix((char*)out.AccessData(0), out.get_total_byte());
}

void Mixer::Mix(char* out, size_t size_)
{
  memset(out, 0, size_);

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
}


void Mixer::PlayRecord(uint32_t channel, uint32_t delay_ms)
{
  mixing_record_.emplace_back(MixingRecord{ channel, delay_ms });
}

void Mixer::MixRecord(PCMBuffer& out)
{
  for (auto &ii : mixing_record_)
  {
    Sound* s = GetSound(ii.channel);
    if (s)
      out.Mix(ii.ms, *s); // TODO: volume parameter to Mix function
  }
}

size_t Mixer::GetRecordByteSize()
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