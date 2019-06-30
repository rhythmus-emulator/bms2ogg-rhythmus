#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "rencoder.h"

class ParseArgs
{
public:
  void AddParseArgs(const std::string& key, const std::string& hint = "", bool is_necessary = false);
  void AddParseArgs(const std::string& key, const std::string& hint = "", const std::string& default_value = "", bool is_necessary = false);
  void PrintHelp();
  bool Parse(int argc, char **argv);
  
  bool IsKeyExists(const std::string& key);
  std::string& GetValue(const std::string& key);

private:
  std::vector<std::tuple<std::string, std::string, std::string> > args_necessary_, args_optional_;
  std::map<std::string, std::string> keys_;
};

void ParseArgs::AddParseArgs(const std::string& key, const std::string& hint, bool is_necessary)
{
  AddParseArgs(key, hint, "", is_necessary);
}

void ParseArgs::AddParseArgs(const std::string& key, const std::string& hint, const std::string& default_value, bool is_necessary)
{
  if (is_necessary)
    args_necessary_.push_back({ key, hint, default_value /* useless */ });
  else
    args_optional_.push_back({ key, hint, default_value });
}

void ParseArgs::PrintHelp()
{
  std::cout << "Usage:";
  for (auto& t : args_necessary_)
    std::cout << " [" << std::get<0>(t) << "]";
  for (auto& t : args_optional_)
  {
    std::cout << " -" << std::get<0>(t);
    if (!std::get<1>(t).empty())
      std::cout << " (" << std::get<1>(t) << ")";
    if (!std::get<2>(t).empty())
      std::cout << " (default " << std::get<1>(t) << ")";
  }
  std::cout << std::endl;
}

bool ParseArgs::Parse(int argc, char **argv)
{
  int necessary_count = 0;
  for (int i = 0; i < argc; ++i)
  {
    if (necessary_count < args_necessary_.size())
    {
      keys_[std::get<0>(args_necessary_[necessary_count])] = argv[i];
      necessary_count++;
    }
    else
    {
      if (argv[i][0] == '-' && argc > i + 1)
      {
        bool is_valid_option = false;
        for (auto &t : args_optional_)
        {
          if (std::get<0>(t) == (argv[i] + 1))
          {
            is_valid_option = true;
            break;
          }
        }
        if (!is_valid_option)
          return false;

        keys_[argv[i] + 1] = argv[i + 1];
        ++i;
      }
      else return false;
    }
  }
  if (necessary_count < args_necessary_.size()) return false;

  // set default value if necessary
  for (auto &t : args_optional_)
  {
    if (!IsKeyExists(std::get<0>(t)) && !std::get<2>(t).empty())
      keys_[std::get<0>(t)] = std::get<2>(t);
  }

  return true;
}

bool ParseArgs::IsKeyExists(const std::string& key)
{
  return (keys_.find(key) != keys_.end());
}

std::string& ParseArgs::GetValue(const std::string& key)
{
  return keys_.find(key)->second;
}

class REncoder_CLI : public REncoder
{
public:
  virtual void OnUpdateProgress(double progress)
  {
    // TODO
  }
};

int main(int argc, char **argv)
{
  ParseArgs args;
  args.AddParseArgs("input_path", "File/folder path of chart file.", true);
  args.AddParseArgs("output_path", "Output path of encoded file. (STDOUT) to print output to STDOUT. Automatically write to same directory with executor if not set.", false);
  args.AddParseArgs("type", "Type of output file. Set automatically if output path is set.", false);
  args.AddParseArgs("quality", "Quality of sound file, 0 ~ 1. (for ogg / flac).", "0.6", false);
  args.AddParseArgs("pitch", "Pitch effect, bigger than zero.", "1.0", false);
  args.AddParseArgs("tempo", "Tempo length effect, bigger than zero.", "1.0", false);
  args.AddParseArgs("volume", "Set volume of key sound", "0.8", false);
  args.AddParseArgs("stop_duplicated_sound", "Stop previous channel when same channel input detected.", "true", false);

  if (!args.Parse(argc, argv))
  {
    args.PrintHelp();
    return 0;
  }

  REncoder_CLI e;
  e.SetInput(args.GetValue("input_path"));
  if (args.IsKeyExists("output_path")) e.SetOutput(args.GetValue("output_path"));
  if (args.IsKeyExists("type")) e.SetSoundType(args.GetValue("type"));
  e.SetQuality(atof(args.GetValue("quality").c_str()));
  e.SetPitch(atof(args.GetValue("pitch").c_str()));
  e.SetTempo(atof(args.GetValue("tempo").c_str()));
  e.SetVolume(atof(args.GetValue("volume").c_str()));
  e.SetStopDuplicatedSound(args.GetValue("stop_duplicated_sound") == "true");

  if (e.Encode())
    std::cout << "Encoding finished successfully." << std::endl;
  else
    std::cout << "Encoding failed." << std::endl;

  return 0;
}