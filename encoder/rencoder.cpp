#include "rencoder.h"
#include "Encoder.h"
#include "Sampler.h"
#include "Mixer.h"

#include "Song.h"
#include "ChartUtil.h"

REncoder::REncoder()
  : quality_(0.6), tempo_length_(1.0), pitch_(1.0), volume_ch_(0.8),
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

void REncoder::SetHTMLOutPath(const std::string& out_path)
{
  html_out_path_ = out_path;
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
  OnUpdateProgress(0.0);
  if (filename_in_.empty()) return false;

  // load chart first.
  rparser::Song s;
  if (!s.Open(filename_in_))
  {
    std::cerr << "Failed to read chart file." << std::endl;
    return false;
  }
  rparser::Chart *c = s.GetChart();
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

  // mixing prepare : load sound files
  rhythmus::SoundInfo sinfo{ sound_bps_, sound_ch_, sound_rate_ };
  rhythmus::Mixer mixer(sinfo);
  rparser::Directory *songresource = s.GetDirectory();
  const auto &md = c->GetMetaData();
  int si = 0;
  for (auto &ii : md.GetSoundChannel()->fn)
  {
    std::cout << "resource name is : " << ii.second << " (ch" << ii.first << ")" << std::endl;
    auto* fd = songresource->Get(ii.second, true);
    if (!mixer.LoadSound(ii.first, *fd))
      std::cerr << "Failed loading sound file: " << ii.first << std::endl;
    mixer.SetChannelVolume(ii.first, volume_ch_);
    OnUpdateProgress(++si / (double)md.GetSoundChannel()->fn.size() * 0.5);
  }
  OnUpdateProgress(0.5);

  // mixing start
  // TODO: detailed progress for encoding
  rhythmus::PCMBuffer *sound_out =
    new rhythmus::SoundVariableBuffer(sinfo);

  // -- MIDI event first
  {
    mixer.SetSoundSource(1);  // we know it's midi event, so set MIDI mixing mode first
    uint8_t mc, ma, mb;
    for (auto &e : c->GetEventNoteData())
    {
      mixer.SetRecordMode((uint32_t)e.GetTimePos());
      e.GetMidiCommand(mc, ma, mb);
      mixer.SendEvent(mc, ma, mb);
    }
  }

  // -- Sound event
  {
    for (auto &n : c->GetNoteData())
    {
      // MIDI or PCM source data?
      mixer.SetSoundSource(n.channel_type);
      if (n.channel_type == 0)
      {
        // PCM
        mixer.SetRecordMode(n.time_msec);
        if (stop_prev_note_) mixer.Stop(n.value);    // Stop previous note if exists
        mixer.Play(n.value);
      }
      else
      {
        // MIDI
        const uint32_t curtime = (uint32_t)n.GetTimePos();
        const uint32_t endtime = curtime + n.effect.duration_ms;
        mixer.SetRecordMode(curtime);
        mixer.Play(n.value, n.effect.key, n.effect.volume);
        mixer.SetRecordMode(endtime);
        mixer.Stop(n.value, n.effect.key);
      }
    }
  }

  mixer.MixRecord(*sound_out);
  OnUpdateProgress(0.9);

  // mixing end... release buffer here.
  mixer.Clear();

  // effector if necessary.
  if (tempo_length_ != 1.0 || pitch_ != 1.0)
  {
    rhythmus::Sound *s = new rhythmus::Sound();
    rhythmus::SoundVariableBufferToSoundBuffer(*(rhythmus::SoundVariableBuffer*)sound_out, *s);
    delete sound_out;
    sound_out = s;
    rhythmus::Sampler sampler(*s);
    sampler.SetPitch(pitch_);
    sampler.SetTempo(tempo_length_);
    rhythmus::Sound newsound;
    sampler.Resample(newsound);
    std::swap(*s, newsound);
  }
  OnUpdateProgress(1.0);

  // create proper encoder
  rhythmus::Encoder *encoder = nullptr;
  if (enc_signature == "WAV")
    encoder = new rhythmus::Encoder_WAV(*sound_out);
  else if (enc_signature == "OGG")
    encoder = new rhythmus::Encoder_OGG(*sound_out);
  else if (enc_signature == "FLAC")
    encoder = new rhythmus::Encoder_FLAC(*sound_out);
  else exit(-1); /* should not happen */
  encoder->SetMetadata("TITLE", md.title);
  encoder->SetMetadata("SUBTITLE", md.subtitle);
  encoder->SetMetadata("ARTIST", md.artist);
  encoder->SetMetadata("SUBARTIST", md.subartist);
  encoder->SetQuality(quality_);
  // TODO: write albumart data.
  // TODO: write to STDOUT if necessary.
  encoder->Write(filename_out_);

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
  encoder->Close();
  delete encoder;
  delete sound_out;
  s.Close();

  OnUpdateProgress(1.0);
  return true;
}

void REncoder::OnUpdateProgress(double progress) { /* Do nothing by default */ }