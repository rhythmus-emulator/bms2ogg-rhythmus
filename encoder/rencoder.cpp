#include "rencoder.h"
#include "Mixer.h"
#include "SoundPool.h"

#include "Song.h"
#include "ChartUtil.h"

REncoder::REncoder()
  : chart_index_(0), quality_(0.6), tempo_length_(1.0), pitch_(1.0), volume_ch_(0.8),
    sound_bps_(16), sound_ch_(2), sound_rate_(44100), stop_prev_note_(true)
{}

void REncoder::SetInput(const std::string& filename)
{
  filename_in_ = filename;
}

void REncoder::SetOutput(const std::string& out_filename)
{
  filename_out_ = out_filename;
  SetSoundType(rutil::GetExtension(filename_out_));
}

bool REncoder::PreloadChart()
{
  rparser::Song s;
  chartnamelist_.clear();

  // load chart and fetch filename list
  if (!s.Open(filename_in_))
    return false;
  for (size_t i = 0; i < s.GetChartCount(); ++i)
  {
    rparser::Chart *c = s.GetChart(i);
    chartnamelist_.push_back(c->GetFilename());
    s.CloseChart();
  }

  s.Close();
}

const std::vector<std::string>& REncoder::GetChartList() const
{
  return chartnamelist_;
}

void REncoder::SetChartIndex(int index)
{
  chart_index_ = index;
}

void REncoder::SetSoundType(const std::string& soundtype)
{
  sound_type_ = soundtype;
}

void REncoder::SetQuality(double quality)
{
  quality_ = quality;
}

void REncoder::SetPitch(double pitch)
{
  pitch_ = pitch;
}

void REncoder::SetTempo(double tempo)
{
  tempo_length_ = tempo;
}

void REncoder::SetVolume(double vol)
{
  volume_ch_ = vol;
}

void REncoder::SetStopDuplicatedSound(bool v)
{
  stop_prev_note_ = v;
}

bool REncoder::Encode()
{
  using namespace rmixer;
  OnUpdateProgress(0.0);
  if (filename_in_.empty()) return false;

  // load chart first.
  rparser::Song s;
  if (!s.Open(filename_in_))
  {
    std::cerr << "Failed to read chart file." << std::endl;
    return false;
  }
  rparser::Chart *c = s.GetChart(chart_index_);
  if (!c)
  {
    s.Close();
    return false;
  }
  c->Invalidate();

  if (sound_type_.empty())
  {
    //std::cerr << "Output sound format is not set." << std::endl;
    //return false;
    
    // default is OGG format.
    sound_type_ = "ogg";
  }
  if (filename_out_.empty())
  {
    filename_out_ = c->GetMetaData().title + "." + sound_type_;
  }

  // filter out proper extension
  const std::string enc_signature = rutil::upper(sound_type_);
  if (enc_signature != "WAV"
    && enc_signature != "OGG"
    && enc_signature != "FLAC")
  {
    std::cerr << "Unknown encoding sound file type." << std::endl;
    return false;
  }

  // mixing prepare
  SoundInfo sinfo( sound_bps_, sound_ch_, sound_rate_ );
  KeySoundPoolWithTime soundpool;
  Mixer mixer(sinfo);
  Sound out;
  constexpr size_t kChannelCount = 2048;
  soundpool.Initalize(kChannelCount);

  // load sound files
  // TODO: set callback with OnUpdateProgress
  soundpool.LoadFromChart(s, *c);
  OnUpdateProgress(0.3);

  // set volume
  for (size_t i = 0; i < kChannelCount; ++i)
  {
    Sound *s = soundpool.GetSound(i);
    if (s) s->SetVolume(volume_ch_);
  }

  // do mixing & release unused resource here
  soundpool.RecordToSound(out);
  OnUpdateProgress(0.6);
  soundpool.Clear();

  // effector if necessary.
  if (tempo_length_ != 1.0 || pitch_ != 1.0)
  {
    out.Resample(pitch_, tempo_length_, 1.0);
  }
  OnUpdateProgress(0.75);

  // save file
  std::map<std::string, std::string> metadata;
  auto &md = c->GetMetaData();
  metadata["TITLE"] = md.title;
  metadata["SUBTITLE"] = md.subtitle;
  metadata["ARTIST"] = md.artist;
  metadata["SUBARTIST"] = md.subartist;
  // TODO: write albumart data.
  // TODO: write to STDOUT if necessary.
  out.Save(
    filename_out_,
    metadata,
    quality_
  );
  OnUpdateProgress(1.0);

  // is it necessary to export chart html?
  if (!html_out_path_.empty())
  {
    std::string html;
    rparser::ExportToHTML(*c, html);
    FILE *f = rutil::fopen_utf8(html_out_path_, "wb");
    if (!f)
    {
      std::cerr << "HTML exporting failed : I/O failed." << std::endl;
      return false;
    }
    else
    {
      fwrite(html.c_str(), 1, html.size(), f);
      fclose(f);
    }
  }

  // cleanup.
  s.Close();

  OnUpdateProgress(1.0);
  return true;
}

void REncoder::OnUpdateProgress(double progress) { /* Do nothing by default */ }

bool REncoder::ExportToHTML(const std::string& outpath)
{
  rparser::Song s;
  if (!s.Open(filename_in_))
    return false;
  rparser::Chart* c = s.GetChart(chart_index_);
  if (!c)
  {
    s.Close();
    return false;
  }
  c->Invalidate();

  std::string buf;
  rparser::ExportToHTML(*c, buf);
  FILE* fp = rutil::fopen_utf8(outpath, "wb");
  if (!fp) return false;
  bool r = (buf.size() == fwrite(buf.c_str(), 1, buf.size(), fp));
  fclose(fp);

  s.Close();
  return r;
}