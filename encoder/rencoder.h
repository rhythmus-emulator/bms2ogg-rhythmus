#ifndef RENCODER_H
#define RENCODER_H

#include <string>

class REncoder
{
public:
  REncoder();
  void SetInput(const std::string& filename);
  void SetOutput(const std::string& filename);
  void SetSoundType(const std::string& soundtype);
  void SetQuality(double quality);
  void SetPitch(double pitch);
  void SetTempo(double tempo);
  void SetVolume(double vol);
  void SetStopDuplicatedSound(bool v);
  bool Encode();
  virtual void OnUpdateProgress(double progress);
private:
  std::string filename_in_;
  std::string filename_out_;
  double quality_;
  double tempo_length_;
  double pitch_;
  double volume_ch_;
  std::string sound_type_;
  uint16_t sound_bps_;
  uint16_t sound_rate_;
  uint16_t sound_ch_;
  bool stop_prev_note_;
};

#endif