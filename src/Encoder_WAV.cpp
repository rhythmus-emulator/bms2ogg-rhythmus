#include "Encoder.h"
#include "rutil.h"

typedef struct
{
  uint8_t chunk_id[4];
  uint32_t chunk_size;
  uint8_t format[4];
  uint8_t subchunk1_id[4];
  uint32_t subchunk1_size;
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;         // sample_rate denotes the sampling rate.
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
  uint8_t subchunk2_id[4];
  uint32_t subchunk2_size;      // subchunk2_size denotes the number of samples.
} WAVHeader;

namespace rhythmus
{

Encoder_WAV::Encoder_WAV(const Sound &sound) : Encoder(sound) {}

Encoder_WAV::Encoder_WAV(const SoundMixer &mixer) : Encoder(mixer) {}

bool Encoder_WAV::Write(const std::string& path)
{
  WAVHeader h;
  uint32_t chunk_size = sizeof(WAVHeader) + total_buffer_size_ - 8; // file_size - 8
  uint32_t data_chunk_size = total_buffer_size_;
  memcpy(h.chunk_id, "RIFF", 4);
  h.chunk_size = chunk_size;
  memcpy(h.format, "WAVE", 4);
  memcpy(h.subchunk1_id, "fmt ", 4);
  h.subchunk1_size = 16;
  h.audio_format = 1;
  h.num_channels = info_.channels;
  h.sample_rate = info_.rate;
  h.byte_rate = h.sample_rate * h.num_channels * info_.bitsize / 8;
  h.block_align = h.num_channels * info_.bitsize / 8;
  h.bits_per_sample = info_.bitsize;
  memcpy(h.subchunk2_id, "data", 4);
  h.subchunk2_size = data_chunk_size;

  FILE *fp = rutil::fopen_utf8(path.c_str(), "wb");
  if (!fp) return false;
  fwrite((void*)&h, sizeof(WAVHeader), 1, fp);
  for (auto &x : buffers_)
  {
    fwrite(x.p, 1, x.s, fp);
  }
  fclose(fp);

  return true;
}

}