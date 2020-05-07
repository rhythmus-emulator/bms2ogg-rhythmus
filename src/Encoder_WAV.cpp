#include "Encoder.h"
#include "rparser.h"  /* for rutil module */
#include <memory.h>

namespace rmixer
{
  
#define WAVChunkHeader \
  uint8_t chunk_id[4]; \
  uint32_t chunk_size;

typedef struct
{
  WAVChunkHeader
  uint8_t format[4];
} WAVHeader;

typedef struct
{
  WAVChunkHeader
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;         // sample_rate denotes the sampling rate.
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
} WAVFmtChunk;

typedef struct
{
  WAVChunkHeader
} WAVDataChunk;

typedef struct
{
  WAVChunkHeader
  uint8_t subsection_id[4];
} WAVINFOChunk;

typedef struct
{
  WAVChunkHeader
  uint8_t data[256];
} WAVINFOSubChunk;

struct RawChunk
{
  void *p;          // raw data which is going to written in WAVE file.
  size_t chunksize; // size actually written to filestream. not written in WAVE metadata.
};

RawChunk MakeWAVINFOSubChunk(const std::string& section_id, const std::string& data)
{
  WAVINFOSubChunk &c = *new WAVINFOSubChunk();
  size_t datasize = data.size() + 1;
  memset(c.data, 0, sizeof(c.data));
  memcpy(c.chunk_id, section_id.c_str(), 4);
  c.chunk_size = datasize;
  memcpy((char*)c.data, data.c_str(), datasize);
  return { &c, datasize + 8 };
}

Encoder_WAV::Encoder_WAV(const Sound &sound) : Encoder(sound) {}

bool Encoder_WAV::Write(const std::string& path)
{
  std::vector<RawChunk> wavheaders;
  WAVHeader h;
  WAVFmtChunk h_fmt;
  WAVDataChunk h_data;
  WAVINFOChunk h_meta;
  std::vector<RawChunk> wavinfoheaders;
  size_t total_metadata_size = 0;
  uint32_t file_size = 0; // file_size - 8

  /* check is format suitable */
  if (!(
    (info_.bitsize == 8 && info_.is_signed == 0) ||
    (info_.bitsize == 16 && info_.is_signed == 1) ||
    (info_.bitsize == 24 && info_.is_signed == 1) ||
    (info_.bitsize == 32 && info_.is_signed == 1) ||
    (info_.bitsize == 32 && info_.is_signed == 2) ))
    return false;

  /* fill header first */
  memcpy(h.chunk_id, "RIFF", 4);
  h.chunk_size = 0;   // filled later
  memcpy(h.format, "WAVE", 4);
  wavheaders.push_back({ &h, sizeof(h) });

  /* fill fmt / data section */
  memcpy(h_fmt.chunk_id, "fmt ", 4);
  h_fmt.chunk_size = sizeof(h_fmt) - 8;
  h_fmt.audio_format = (info_.is_signed == 1 ? 1 : 3 /* IEEE float */);
  h_fmt.num_channels = info_.channels;
  h_fmt.sample_rate = info_.rate;
  h_fmt.byte_rate = h_fmt.sample_rate * h_fmt.num_channels * info_.bitsize / 8;
  h_fmt.block_align = h_fmt.num_channels * info_.bitsize / 8;
  h_fmt.bits_per_sample = info_.bitsize;
  wavheaders.push_back({ &h_fmt, sizeof(h_fmt) });

  memcpy(h_data.chunk_id, "data", 4);
  h_data.chunk_size = total_buffer_size_;
  wavheaders.push_back({ &h_data, sizeof(h_data) });
  for (auto &x : buffers_)
    wavheaders.push_back({ (void*)x.p, x.s });

  /* fill metadata if necessary */
  std::string metavalue;
  {
    if (GetMetadata("TITLE", metavalue))
    {
      auto c = MakeWAVINFOSubChunk("ISBJ", metavalue.c_str());
      total_metadata_size += c.chunksize + 8;
      wavinfoheaders.push_back(c);
    }
  }
  {
    if (GetMetadata("ARTIST", metavalue))
    {
      auto c = MakeWAVINFOSubChunk("IART", metavalue.c_str());
      total_metadata_size += c.chunksize + 8;
      wavinfoheaders.push_back(c);
    }
  }
  if (total_metadata_size > 0)
  {
    memcpy(h_meta.chunk_id, "LIST", 4);
    h_meta.chunk_size = total_metadata_size;
    memcpy(h_meta.subsection_id, "INFO", 4);
    wavheaders.push_back({ &h_meta, sizeof(h_meta) });
    for (auto& chunk : wavinfoheaders)
      wavheaders.push_back(chunk);
  }

  /* calculate total file size and fill RIFF chunk size */
  for (auto &i : wavheaders)
    file_size += i.chunksize;
  h.chunk_size = file_size - 8; /* self header description size */

  FILE *fp = rutil::fopen_utf8(path.c_str(), "wb");
  if (!fp) return false;
  for (auto &chunk : wavheaders)
    fwrite(chunk.p, 1, chunk.chunksize, fp);
  fclose(fp);

  // cleanup
  for (auto& chunk : wavinfoheaders)
    free(chunk.p);

  return true;
}

}
