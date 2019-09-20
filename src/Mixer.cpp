#include "Mixer.h"
#include "Error.h"
#include "Decoder.h"
#include "Sampler.h"
#include <algorithm>

// for sending timidity event
#include "playmidi.h"

namespace rmixer
{

Mixer::Mixer(const SoundInfo& info, size_t s)
  : info_(info), max_mixing_byte_size_(s), midi_(info, s),
  current_group_(0), current_source_(0), record_time_ms_(0), is_record_mode_(false)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::Mixer(const SoundInfo& info, const char* midi_cfg_path, size_t s)
  : info_(info), max_mixing_byte_size_(s), midi_(info, s, midi_cfg_path),
  current_group_(0), current_source_(0), record_time_ms_(0), is_record_mode_(false)
{
  midi_buf_ = (char*)malloc(s);
}

Mixer::~Mixer()
{
  Clear();
}

bool Mixer::LoadSound(uint16_t channel, const rutil::FileData &fd)
{
  const std::string ext = rutil::upper(rutil::GetExtension(fd.GetFilename()));
  Decoder* d = 0;
  Sound *s = new Sound();

  if (ext == "WAV") d = new Decoder_WAV(*s);
  else if (ext == "OGG") d = new Decoder_OGG(*s);
  else if (ext == "FLAC") d = new Decoder_FLAC(*s);
  else if (ext == "MP3") d = new Decoder_LAME(*s);

  /* WARN: const_cast is used */
  bool r = d && d->open(const_cast<rutil::FileData&>(fd)) && d->read() != 0;
  delete d;

  if (r)
  {
    // check for resampling
    if (s->get_info() != info_)
    {
      Sound *new_s = new Sound();
      Sampler sampler(*s, info_);
      sampler.Resample(*new_s);
      delete s;
      s = new_s;
    }

    channels_[(current_group_ << 16) | channel] =
    { s, 1.0f, s->get_total_byte(), 0, false, true };
  }

  return r;
}

bool Mixer::LoadSound(uint16_t channel, const std::string& filename,
  const char* p, size_t len)
{
  // TODO
  return false;
}

bool Mixer::LoadSound(uint16_t channel, const std::string& filepath)
{
  rutil::FileData fd;
  rutil::ReadFileData(filepath, fd);
  if (fd.IsEmpty())
    return false;
  return LoadSound(channel, fd);
}

bool Mixer::LoadSound(uint16_t channel, Sound* s, bool is_freeable)
{
  if (!s) return false;
  if (s->get_info() != info_) return false;
  const uint32_t idx = (current_group_ << 16) | channel;

  // if channel is already exist, then clear that channel first
  auto ii = channels_.find(idx);
  if (ii != channels_.end())
  {
    if (ii->second.is_freeable)
      delete ii->second.s;
  }
  channels_[idx] =
    { s, 1.0f, s->get_total_byte(), 0, false, is_freeable };
  return true;
}

void Mixer::FreeSoundGroup(uint16_t group)
{
  // TODO
  ASSERT(0);
}

void Mixer::FreeSound(uint16_t channel)
{
  auto ii = channels_.find((current_group_ << 16) | channel);
  if (ii != channels_.end())
  {
    if (ii->second.is_freeable)
      delete ii->second.s;
  }
  channels_.erase(ii);
}

void Mixer::FreeAllSound()
{
  for (auto &ii : channels_)
    if (ii.second.is_freeable)
      delete ii.second.s;
  channels_.clear();
}

void Mixer::SetSoundSource(int sourcetype)
{
  current_source_ = sourcetype;
}

void Mixer::SetSoundGroup(uint16_t group)
{
  current_group_ = group;
}

void Mixer::Play(uint16_t channel, uint8_t key, float volume)
{
  if (!is_record_mode_) PlayRT(channel, key, volume);
  else PlayRecord(channel, key, volume);
}

void Mixer::Stop(uint16_t channel, uint8_t key)
{
  if (!is_record_mode_) StopRT(channel, key);
  else StopRecord(channel, key);
}

// for midi command data of timidity
void Mixer::SendEvent(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b)
{
  // ME_NONE ignore.
  if (event_type == 0) return;
  if (!is_record_mode_) EventRT(event_type, channel, a, b);
  else EventRecord(event_type, channel, a, b);
}

// for raw midi play data
void Mixer::SendEvent(uint8_t c, uint8_t a, uint8_t b)
{
  const uint8_t type = midi_.GetEventTypeFromStatus(c, a, b);
  SendEvent(type, c & 0xf, a, b);
}

void Mixer::PlayRT(uint16_t channel, uint8_t key, float volume)
{
  /** Real-time mode */
  if (current_source_ == 0)
  {
    /** WAVE */
    auto *c = GetMixChannel(channel);
    if (!c) return;
    c->remain_byte = c->total_byte;
  }
  else if (current_source_ == 1)
  {
    /** MIDI */
    SendEvent(ME_NOTEON, channel, key, (uint8_t)(0x7F * volume) /* Velo */);
  }
}

void Mixer::StopRT(uint16_t channel, uint8_t key)
{
  /** Real-time mode */
  if (current_source_ == 0)
  {
    /** WAVE */
    auto *c = GetMixChannel(channel);
    if (!c) return;
    c->remain_byte = 0;
  }
  else if (current_source_ == 1)
  {
    /** MIDI */
    EventRT(ME_NOTEOFF, channel, key, 0 /* Velo */);
  }
}

void Mixer::EventRT(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b)
{
  if (event_type == 0) return;
  midi_.SendEvent(channel, event_type, a, b);
}

void Mixer::PlayRecord(uint16_t channel, uint8_t key, float volume)
{
  /** Record mode */
  if (current_source_ == 0)
  {
    /** WAVE */
    mixing_record_.emplace_back(MixingRecord{ record_time_ms_, channel });
  }
  else if (current_source_ == 1)
  {
    /** MIDI */
    EventRecord(ME_NOTEON, channel, key, (uint8_t)(0x7F * volume) /* Velo */);
  }
}

void Mixer::StopRecord(uint16_t channel, uint8_t key)
{
  /** Record mode */
  if (current_source_ == 0)
  {
    /** WAVE */
    // Assume mixing record is in time order ...
    auto *c = GetMixChannel(channel);
    if (c == 0)
      return;
    for (auto ii = mixing_record_.rbegin(); ii != mixing_record_.rend(); ++ii)
    {
      if (ii->channel == channel)
      {
        const uint32_t time_length = GetMilisecondFromByte(c->remain_byte, info_);
        if (time_length + ii->ms < record_time_ms_)
          c->remain_byte = GetByteFromMilisecond(record_time_ms_ - ii->ms, info_);
        break;
      }
    }
  }
  else if (current_source_ == 1)
  {
    /** MIDI */
    EventRecord(ME_NOTEOFF, channel, key, 0 /* Velo */);
  }
}

void Mixer::EventRecord(uint8_t event_type, uint8_t channel, uint8_t a, uint8_t b)
{
  if (event_type == 0) return;
  midi_mixing_record_.emplace_back(MidiMixingRecord{ record_time_ms_, channel, event_type, a, b });
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


void Mixer::SetRecordMode(uint32_t time_ms)
{
  is_record_mode_ = true;
  record_time_ms_ = time_ms;
}

void Mixer::FinishRecordMode()
{
  is_record_mode_ = false;
}

void Mixer::ClearRecord()
{
  mixing_record_.clear();
  midi_mixing_record_.clear();
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
        EventRT(midi_event_ii->event_type, midi_event_ii->channel, midi_event_ii->a, midi_event_ii->b);
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

Sound* Mixer::GetSound(uint16_t channel)
{
  auto *c = GetMixChannel(channel);
  if (c)
    return c->s;
  else return 0;
}

void Mixer::SetChannelVolume(uint16_t channel, float v)
{
  auto *c = GetMixChannel(channel);
  if (c)
    c->volume = v;
}

MixChannel* Mixer::GetMixChannel(uint16_t channel)
{
  auto ii = channels_.find((current_group_ << 16) | channel);
  if (ii == channels_.end())
    return 0;
  else
    return &ii->second;
}

void Mixer::Clear()
{
  // don't need to touch soundinfo, maybe
  ClearRecord();
  FreeAllSound();
}

}