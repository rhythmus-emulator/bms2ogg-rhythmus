#include "wx/wx.h"
#include "wx/spinctrl.h"
#include "rencoder.h"
#include <thread>

class REncoder_GUI : public REncoder
{
public:
  wxGauge* g = nullptr;
  wxButton* b = nullptr;
  wxListBox* st = nullptr;

  virtual void OnUpdateProgress(double progress)
  {
    if (!g) return;
    g->SetValue(static_cast<int>(100 * progress));
  }
};

// Declared as global variable as it's available in singleton
static REncoder_GUI rencoder;
static bool is_encoding = false;

class REncoderApp : public wxApp
{
public:
  virtual bool OnInit();
};

class OptionFrame : public wxFrame
{
public:
  OptionFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:

  std::unique_ptr<wxPanel> panel;
  std::unique_ptr<wxStaticText>
    label_input_path, label_output_path;
  std::unique_ptr<wxTextCtrl> text_input_path;
  std::unique_ptr<wxTextCtrl> text_output_path;
  std::unique_ptr<wxButton> btn_input_path_dialog;
  std::unique_ptr<wxButton> btn_output_path_dialog;

  std::unique_ptr<wxStaticText> label_quality, label_quality_val;
  std::unique_ptr<wxSlider> sld_quality;

  std::unique_ptr<wxStaticText> label_volume, label_volume_val;
  std::unique_ptr<wxSlider> sld_volume;

  std::unique_ptr<wxStaticText> label_pitch;
  std::unique_ptr<wxSpinCtrlDouble> spin_pitch;
  std::unique_ptr<wxStaticText> label_tempo;
  std::unique_ptr<wxSpinCtrlDouble> spin_tempo;

  std::unique_ptr<wxGauge> gaugebar;
  std::unique_ptr<wxListBox> statusctrl;

  std::unique_ptr<wxButton>
    btn_encoding, btn_about;

  void OnBtnEncodingStart(wxCommandEvent& event);
  void OnBtnAbout(wxCommandEvent& event);
  void OnBtnInputFileDialog(wxCommandEvent& event);
  void OnBtnOutputFileDialog(wxCommandEvent& event);
  void OnSldQualityChanged(wxCommandEvent& event);
  void OnSldVolumeChanged(wxCommandEvent& event);
  wxDECLARE_EVENT_TABLE();
};
enum
{
  ID_BTN_ENCODING_START,
  ID_BTN_EXIT,
  ID_BTN_ABOUT,
  ID_BTN_INPUTPATH_DLG,
  ID_BTN_OUTPUTPATH_DLG,
  ID_SLD_QUALITY,
  ID_SLD_VOLUME,
  ID_SPIN_PITCH,
  ID_SPIN_TEMPO,
};

OptionFrame::OptionFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
  : wxFrame(NULL, wxID_ANY, title, pos, size,
    wxDEFAULT_FRAME_STYLE & ~wxMAXIMIZE_BOX & ~wxRESIZE_BORDER)
{
  panel.reset(new wxPanel(this, wxID_ANY));

  label_input_path.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Input file path", wxPoint(10, 10)));
  text_input_path.reset(new wxTextCtrl(panel.get(), wxID_ANY,
    wxEmptyString, wxPoint(120, 10), wxSize(250, 18)));
  btn_input_path_dialog.reset(new wxButton(panel.get(), ID_BTN_INPUTPATH_DLG,
    "...", wxPoint(376, 10), wxSize(20, 18)));

  label_output_path.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Output file path", wxPoint(10, 35)));
  text_output_path.reset(new wxTextCtrl(panel.get(), wxID_ANY,
    wxEmptyString, wxPoint(120, 35), wxSize(250, 18)));
  btn_output_path_dialog.reset(new wxButton(panel.get(), ID_BTN_OUTPUTPATH_DLG,
    "...", wxPoint(376, 35), wxSize(20, 18)));

  label_quality.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Quality (Ogg)", wxPoint(10, 60)));
  sld_quality.reset(new wxSlider(panel.get(), ID_SLD_QUALITY, 6, 0, 10,
    wxPoint(120, 60), wxSize(250, 18), wxSL_HORIZONTAL));
  label_quality_val.reset(new wxStaticText(panel.get(), wxID_ANY,
    "0.6", wxPoint(376, 60)));

  label_volume.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Keysound Volume", wxPoint(10, 85)));
  sld_volume.reset(new wxSlider(panel.get(), ID_SLD_VOLUME, 80, 0, 100,
    wxPoint(120, 85), wxSize(250, 18), wxSL_HORIZONTAL));
  label_volume_val.reset(new wxStaticText(panel.get(), wxID_ANY,
    "80", wxPoint(376, 85)));

  label_pitch.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Pitch effector", wxPoint(10, 110)));
  spin_pitch.reset(new wxSpinCtrlDouble(panel.get(), ID_SPIN_PITCH, wxEmptyString,
    wxPoint(120, 110), wxSize(100, 18), wxSP_ARROW_KEYS, 0, 10, 1.0, 0.1));

  label_tempo.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Tempo effector", wxPoint(10, 135)));
  spin_tempo.reset(new wxSpinCtrlDouble(panel.get(), ID_SPIN_TEMPO, wxEmptyString,
    wxPoint(120, 135), wxSize(100, 18), wxSP_ARROW_KEYS, 0, 10, 1.0, 0.1));

  gaugebar.reset(new wxGauge(panel.get(), wxID_ANY, 100,
    wxPoint(10, 180), wxSize(300, 20)));
  btn_encoding.reset(new wxButton(panel.get(), ID_BTN_ENCODING_START,
    "START", wxPoint(320, 178), wxSize(70, 24)));
  statusctrl.reset(new wxListBox(panel.get(), wxID_ANY,
    wxPoint(10, 210), wxSize(380, 120)));
  btn_about.reset(new wxButton(panel.get(), ID_BTN_ABOUT,
    "ABOUT", wxPoint(10, 340), wxSize(60, 24)));

  // add initial message to statusctrl
  statusctrl->Insert("Ready.", 0);

  // let rencoder know ctrl
  rencoder.g = gaugebar.get();
  rencoder.b = btn_encoding.get();
  rencoder.st = statusctrl.get();

  // set window to center position
  Center();
}

void OptionFrame::OnBtnInputFileDialog(wxCommandEvent& event)
{
  wxFileDialog openFileDialog(this, "Select rhythmgame chart compatible file.",
    "", "", "Rhythmgame chart file|*.bms;*.bme;*.bml;*.vos;*.zip;*.osu|Any file|*.*",
    wxFD_OPEN);
  if (openFileDialog.ShowModal() != wxID_OK)
    return;
  text_input_path->SetLabelText(openFileDialog.GetPath());
}

void OptionFrame::OnBtnOutputFileDialog(wxCommandEvent& event)
{
  wxFileDialog saveFileDialog(this, "Select file name to save.",
    "", "", "OGG|*.ogg|FLAC|*.flac|WAV|*.wav",
    wxFD_SAVE);
  if (saveFileDialog.ShowModal() != wxID_OK)
    return;
  text_output_path->SetLabelText(saveFileDialog.GetPath());
}

void OptionFrame::OnSldQualityChanged(wxCommandEvent& event)
{
  double v = sld_quality->GetValue() / 10.0;
  char s[256];
  gcvt(v, 1, s);
  label_quality_val->SetLabelText(s);
}

void OptionFrame::OnSldVolumeChanged(wxCommandEvent& event)
{
  int i = sld_volume->GetValue();
  char s[256];
  itoa(i, s, 10);
  label_volume_val->SetLabelText(s);
}

void encoderfunc()
{
  // MUST executed only one thread at a time
  if (is_encoding) return;
  is_encoding = true;

  if (rencoder.Encode())
    rencoder.st->Insert("Encoding successfully done.", 0xffffffff);
  else
    rencoder.st->Insert("Encoding failed.", 0xffffffff);
  rencoder.b->Enable();

  is_encoding = false;
}

void OptionFrame::OnBtnEncodingStart(wxCommandEvent& event)
{
  // prepare ctrls
  btn_encoding->Disable();
  gaugebar->SetValue(0);
  statusctrl->Clear();
  statusctrl->Insert("Start encoding.", 0);

  // prepare encoder
  rencoder.SetInput(text_input_path->GetValue().ToStdString());
  rencoder.SetOutput(text_output_path->GetValue().ToStdString());
  //rencoder.SetQuality();
  //rencoder.SetPitch();
  //rencoder.SetTempo();
  //rencoder.SetVolume();

  // start encoder
  std::thread t(encoderfunc);
}

void OptionFrame::OnBtnAbout(wxCommandEvent& event)
{
  wxMessageBox("By @lazykuna\nVersion r190702\nhttps://github.com/rhythmus-emulator/rencoder");
}

wxBEGIN_EVENT_TABLE(OptionFrame, wxFrame)
EVT_BUTTON(ID_BTN_ENCODING_START, OptionFrame::OnBtnEncodingStart)
EVT_BUTTON(ID_BTN_ABOUT, OptionFrame::OnBtnAbout)
EVT_BUTTON(ID_BTN_INPUTPATH_DLG, OptionFrame::OnBtnInputFileDialog)
EVT_BUTTON(ID_BTN_OUTPUTPATH_DLG, OptionFrame::OnBtnOutputFileDialog)
EVT_SLIDER(ID_SLD_QUALITY, OptionFrame::OnSldQualityChanged)
EVT_SLIDER(ID_SLD_VOLUME, OptionFrame::OnSldVolumeChanged)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(REncoderApp);

bool REncoderApp::OnInit()
{
  OptionFrame *frame = new OptionFrame(
    "Rhythmus encoder r190700", wxDefaultPosition, wxSize(420, 415));
  frame->Show(true);
  return true;
}
