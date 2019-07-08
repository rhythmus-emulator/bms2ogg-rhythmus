#include "wx/wx.h"
#include "wx/spinctrl.h"
#include "rencoder.h"
#include "rutil.h"  // for utf8 path
#include <thread>
#include <sstream>

enum
{
  ID_BTN_ENCODING_START,
  ID_BTN_EXIT,
  ID_BTN_ABOUT,
  ID_BTN_HTML_EXPORT,
  ID_BTN_INPUTPATH_DLG,
  ID_BTN_OUTPUTPATH_DLG,
  ID_SLD_QUALITY,
  ID_SLD_VOLUME,
  ID_SPIN_PITCH,
  ID_SPIN_TEMPO,
  ID_SPIN_CHARTIDX,
};
static int RENCODER_EVT = 1000;
DECLARE_EVENT_TYPE(EVT_PROGRESS_UPDATE, -1)
DECLARE_EVENT_TYPE(EVT_FINISH, -1)
DEFINE_EVENT_TYPE(EVT_PROGRESS_UPDATE)
DEFINE_EVENT_TYPE(EVT_FINISH)

class REncoder_GUI : public REncoder
{
public:
  wxFrame* frame = nullptr;
  double prog = 0;
  bool success = false;
  std::string msg;

  virtual void OnUpdateProgress(double progress)
  {
    prog = progress;
    if (!frame) return;
    auto* evthdlr = frame->GetEventHandler();
    wxPostEvent(evthdlr,
      wxCommandEvent(EVT_PROGRESS_UPDATE, RENCODER_EVT));
  }
};

// Declared as global variable as it's available in singleton
static REncoder_GUI rencoder;
static bool is_encoding = false;

class REncoderApp : public wxApp
{
public:
  virtual bool OnInit();
  virtual int OnExit();
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

  std::unique_ptr<wxStaticText>
    label_chartidx, label_chartname;
  std::unique_ptr<wxSpinCtrl> spin_chartidx;

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
    btn_encoding, btn_about, btn_html_export;

  void OnBtnEncodingStart(wxCommandEvent& event);
  void OnBtnAbout(wxCommandEvent& event);
  void OnBtnHtmlExport(wxCommandEvent& event);
  void OnBtnInputFileDialog(wxCommandEvent& event);
  void OnBtnOutputFileDialog(wxCommandEvent& event);
  void OnSpinChartFocus(wxFocusEvent& event);
  void OnSpinChartidxChanged(wxSpinEvent& event);
  void OnSldQualityChanged(wxCommandEvent& event);
  void OnSldVolumeChanged(wxCommandEvent& event);
  void OnProgressUpdate(wxCommandEvent& event);
  void OnFinish(wxCommandEvent& event);
  wxDECLARE_EVENT_TABLE();
};

OptionFrame::OptionFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
  : wxFrame(NULL, wxID_ANY, title, pos, size,
    wxDEFAULT_FRAME_STYLE & ~wxMAXIMIZE_BOX & ~wxRESIZE_BORDER)
{
  panel.reset(new wxPanel(this, wxID_ANY));
  int ypos;

  ypos = 10;
  label_input_path.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Input file path", wxPoint(10, ypos)));
  text_input_path.reset(new wxTextCtrl(panel.get(), wxID_ANY,
    wxEmptyString, wxPoint(120, ypos), wxSize(250, 18)));
  btn_input_path_dialog.reset(new wxButton(panel.get(), ID_BTN_INPUTPATH_DLG,
    "...", wxPoint(376, ypos), wxSize(20, 18)));

  ypos += 25;
  label_output_path.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Output file path", wxPoint(10, ypos)));
  text_output_path.reset(new wxTextCtrl(panel.get(), wxID_ANY,
    wxEmptyString, wxPoint(120, ypos), wxSize(250, 18)));
  btn_output_path_dialog.reset(new wxButton(panel.get(), ID_BTN_OUTPUTPATH_DLG,
    "...", wxPoint(376, ypos), wxSize(20, 18)));

  ypos += 25;
  label_chartidx.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Chart file index", wxPoint(10, ypos)));
  spin_chartidx.reset(new wxSpinCtrl(panel.get(), ID_SPIN_CHARTIDX,
    wxEmptyString, wxPoint(120, ypos), wxSize(100, 18)));
  spin_chartidx->SetMax(0);
  spin_chartidx->Bind(wxEVT_SET_FOCUS, &OptionFrame::OnSpinChartFocus, this);
  label_chartname.reset(new wxStaticText(panel.get(), wxID_ANY,
    "(Any chart file will be used)", wxPoint(240, ypos)));

  ypos += 25;
  label_quality.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Quality (Ogg)", wxPoint(10, ypos)));
  sld_quality.reset(new wxSlider(panel.get(), ID_SLD_QUALITY, 6, 0, 10,
    wxPoint(120, ypos), wxSize(250, 18), wxSL_HORIZONTAL));
  label_quality_val.reset(new wxStaticText(panel.get(), wxID_ANY,
    "0.6", wxPoint(376, ypos)));

  ypos += 25;
  label_volume.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Keysound Volume", wxPoint(10, ypos)));
  sld_volume.reset(new wxSlider(panel.get(), ID_SLD_VOLUME, 80, 0, 100,
    wxPoint(120, ypos), wxSize(250, 18), wxSL_HORIZONTAL));
  label_volume_val.reset(new wxStaticText(panel.get(), wxID_ANY,
    "80", wxPoint(376, ypos)));

  ypos += 25;
  label_pitch.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Pitch effector", wxPoint(10, ypos)));
  spin_pitch.reset(new wxSpinCtrlDouble(panel.get(), ID_SPIN_PITCH, wxEmptyString,
    wxPoint(120, ypos), wxSize(100, 18), wxSP_ARROW_KEYS, 0, 10, 1.0, 0.1));
  
  ypos += 25;
  label_tempo.reset(new wxStaticText(panel.get(), wxID_ANY,
    "Tempo effector", wxPoint(10, ypos)));
  spin_tempo.reset(new wxSpinCtrlDouble(panel.get(), ID_SPIN_TEMPO, wxEmptyString,
    wxPoint(120, ypos), wxSize(100, 18), wxSP_ARROW_KEYS, 0, 10, 1.0, 0.1));

  ypos += 45;
  gaugebar.reset(new wxGauge(panel.get(), wxID_ANY, 100,
    wxPoint(10, ypos), wxSize(300, 20)));
  btn_encoding.reset(new wxButton(panel.get(), ID_BTN_ENCODING_START,
    "START", wxPoint(320, ypos-2), wxSize(70, 24)));
  statusctrl.reset(new wxListBox(panel.get(), wxID_ANY,
    wxPoint(10, ypos+30), wxSize(380, 120)));

  ypos += 160;
  btn_about.reset(new wxButton(panel.get(), ID_BTN_ABOUT,
    "ABOUT", wxPoint(10, ypos), wxSize(60, 24)));
  btn_html_export.reset(new wxButton(panel.get(), ID_BTN_HTML_EXPORT,
    "Export to HTML", wxPoint(80, ypos), wxSize(100, 24)));

  // add initial message to statusctrl
  statusctrl->Insert("Ready.", 0);

  // let rencoder know ctrl
  rencoder.frame = this;

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

void OptionFrame::OnSpinChartFocus(wxFocusEvent& event)
{
  static std::string prev_filename;
  std::string path_in;
#ifdef WIN32
  rutil::EncodeFromWStr(text_input_path->GetValue().ToStdWstring(), path_in, rutil::E_UTF8);
#else
  path_in = text_input_path->GetValue().ToStdString();
#endif
  // only attempt reload when filename is change
  if (prev_filename != path_in)
  {
    rencoder.SetInput(path_in);
    rencoder.PreloadChart();
    if (rencoder.GetChartList().size() == 0)
    {
      label_chartname->SetLabel("(No chart file)");
      spin_chartidx->SetMax(0);
    }
    else spin_chartidx->SetMax(rencoder.GetChartList().size() - 1);
    prev_filename = path_in;
  }

  // To continue with ctrl focus
  event.Skip();
}

void OptionFrame::OnSpinChartidxChanged(wxSpinEvent& event)
{
  const auto& v = rencoder.GetChartList();
  int i = spin_chartidx->GetValue();
  if (i >= 0 && i < v.size())
  {
    std::string fname = rutil::GetFilename(v[i]);
#ifdef WIN32
    std::wstring wstr;
    rutil::DecodeToWStr(fname, wstr, rutil::E_UTF8);
    label_chartname->SetLabel(wstr);
#else
    label_chartname->SetLabel(fname);
#endif
  }
  else
  {
    // should not happen
    label_chartname->SetLabel("(No chart file)");
  }
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

// Used for redirect std::cerr to status ctrl.
class redirect_stream
{
private:
  std::ostream& os;
  std::streambuf* const osbuf;
public:
  redirect_stream(std::ostream& lhs, std::ostream& rhs = std::cout)
    : os(rhs), osbuf(os.rdbuf())
  {
    os.rdbuf(lhs.rdbuf());
  }
  ~redirect_stream()
  {
    os.rdbuf(osbuf);
  }
};

void encoderfunc()
{
  // MUST executed only one thread at a time
  if (is_encoding) return;
  is_encoding = true;

  // capture std::cerr
  // released automatically when task is done...
  std::ostringstream oss;
  redirect_stream hijack_stream(oss, std::cerr);
  rencoder.msg.clear();

  rencoder.success = rencoder.Encode();

  // prepare task message
  rencoder.msg = oss.str();

  // inform the task is ended
  if (rencoder.frame)
    wxPostEvent(rencoder.frame->GetEventHandler(),
      wxCommandEvent(EVT_FINISH, RENCODER_EVT));

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
#ifdef WIN32
  std::string spath_utf8;
  rutil::EncodeFromWStr(text_input_path->GetValue().ToStdWstring(), spath_utf8, rutil::E_UTF8);
  rencoder.SetInput(spath_utf8);
  rutil::EncodeFromWStr(text_output_path->GetValue().ToStdWstring(), spath_utf8, rutil::E_UTF8);
  rencoder.SetOutput(spath_utf8);
#else
  rencoder.SetInput(text_input_path->GetValue().ToStdString());
  rencoder.SetOutput(text_output_path->GetValue().ToStdString());
#endif
  int chartidx = spin_chartidx->GetValue();

  rencoder.SetChartIndex(chartidx);
  rencoder.SetQuality(sld_quality->GetValue() / 10.0);
  rencoder.SetPitch(spin_pitch->GetValue());
  rencoder.SetTempo(spin_tempo->GetValue());
  rencoder.SetVolume(sld_volume->GetValue() / 100.0);

  // start encoder thread
  std::thread t(encoderfunc);
  t.detach();
}

void OptionFrame::OnProgressUpdate(wxCommandEvent& event)
{
  gaugebar->SetValue(static_cast<int>(rencoder.prog * 100));
}

void OptionFrame::OnFinish(wxCommandEvent& event)
{
  // add redirected message from std::cerr
  std::vector<std::string> msgs;
  rutil::split(rencoder.msg, '\n', msgs);
  for (auto& m : msgs)
  {
    if (m.size())
      statusctrl->Insert(m, statusctrl->GetCount());
  }

  // add finish message
  if (rencoder.success)
    statusctrl->Insert("Encoding successfully done.", statusctrl->GetCount());
  else
    statusctrl->Insert("Encoding failed.", statusctrl->GetCount());
  btn_encoding->Enable();
}

void OptionFrame::OnBtnAbout(wxCommandEvent& event)
{
  wxMessageBox("By @lazykuna\nVersion r190702\nhttps://github.com/rhythmus-emulator/rencoder");
}

void OptionFrame::OnBtnHtmlExport(wxCommandEvent& event)
{
  wxFileDialog saveFileDialog(this, "Select file name to save.",
    "", "", "HTML file/*.htm",
    wxFD_SAVE);
  if (saveFileDialog.ShowModal() != wxID_OK)
    return;

  std::string path_in, path_out;
#ifdef WIN32
  rutil::EncodeFromWStr(text_input_path->GetValue().ToStdWstring(), path_in, rutil::E_UTF8);
  rutil::EncodeFromWStr(saveFileDialog.GetPath().ToStdWstring(), path_out, rutil::E_UTF8);
#else
  path_in = text_input_path->GetValue().ToStdString();
  path_out = saveFileDialog.GetPath().ToStdString();
#endif
  int chartidx = spin_chartidx->GetValue();
  
  rencoder.SetInput(path_in);
  rencoder.SetChartIndex(chartidx);
  if (rencoder.ExportToHTML(path_out))
  {
    wxMessageBox(
      "Chart to Html Exporting done.\nMUST need proper css/js file to make it work!",
      "Rhythmus-Encoder");
  }
  else
  {
    wxMessageBox(
      "Failed to export in Html. Check chart file and output path is valid.",
      "Rhythmus-Encoder", wxICON_ERROR);
  }
}

wxBEGIN_EVENT_TABLE(OptionFrame, wxFrame)
EVT_BUTTON(ID_BTN_ENCODING_START, OptionFrame::OnBtnEncodingStart)
EVT_BUTTON(ID_BTN_ABOUT, OptionFrame::OnBtnAbout)
EVT_BUTTON(ID_BTN_HTML_EXPORT, OptionFrame::OnBtnHtmlExport)
EVT_BUTTON(ID_BTN_INPUTPATH_DLG, OptionFrame::OnBtnInputFileDialog)
EVT_BUTTON(ID_BTN_OUTPUTPATH_DLG, OptionFrame::OnBtnOutputFileDialog)
EVT_SLIDER(ID_SLD_QUALITY, OptionFrame::OnSldQualityChanged)
EVT_SLIDER(ID_SLD_VOLUME, OptionFrame::OnSldVolumeChanged)
EVT_SPINCTRL(ID_SPIN_CHARTIDX, OptionFrame::OnSpinChartidxChanged)
EVT_COMMAND(RENCODER_EVT, EVT_PROGRESS_UPDATE, OptionFrame::OnProgressUpdate)
EVT_COMMAND(RENCODER_EVT, EVT_FINISH, OptionFrame::OnFinish)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(REncoderApp);

bool REncoderApp::OnInit()
{
  OptionFrame *frame = new OptionFrame(
    "Rhythmus encoder r190700", wxDefaultPosition, wxSize(420, 440));
  frame->Show(true);
  return true;
}

int REncoderApp::OnExit()
{
  // make unaccessible to main frame work if WTHR is alive
  rencoder.frame = nullptr;
  return 0;
}