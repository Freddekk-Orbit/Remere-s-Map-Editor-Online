#pragma once

// Minimal wxWidgets compatibility layer for the RME WebAssembly port.
// Upstream sources include <wx/...> headers; CMake points those includes here
// so Phase 1+ can compile without linking the real wxWidgets library.
//
// New UI must use Dear ImGui. These types exist only so C++ core that still
// mentions wxString / wxFileName / event macros can be brought over incrementally.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define wxUSE_GLCANVAS 1
#define wxUSE_GUI 1
#define wxMAJOR_VERSION 3
#define wxMINOR_VERSION 2
#define wxRELEASE_NUMBER 0
#define wxCHECK_VERSION(major, minor, release) \
	((wxMAJOR_VERSION > (major)) || ((wxMAJOR_VERSION == (major)) && (wxMINOR_VERSION > (minor))) \
		|| ((wxMAJOR_VERSION == (major)) && (wxMINOR_VERSION == (minor)) && (wxRELEASE_NUMBER >= (release))))

#ifndef wxT
	#define wxT(x) x
#endif
#ifndef _
	#define _(x) x
#endif
#ifndef wxS
	#define wxS(x) x
#endif

#define wxANY_ID (-1)
#define wxID_ANY (-1)
#define wxID_OK 5100
#define wxID_CANCEL 5101
#define wxID_YES 5103
#define wxID_NO 5104
#define wxID_CLOSE 5105
#define wxID_SAVE 5106
#define wxID_OPEN 5107
#define wxID_EXIT 5006
#define wxID_ABOUT 5014
#define wxID_PREFERENCES 5022
#define wxID_UNDO 5031
#define wxID_REDO 5032
#define wxID_CUT 5033
#define wxID_COPY 5034
#define wxID_PASTE 5035
#define wxID_DELETE 5038
#define wxID_SELECTALL 5039
#define wxID_NEW 5002
#define wxID_LOWEST 4999
#define wxID_HIGHEST 5999

#define wxOK 0x0004
#define wxCANCEL 0x0008
#define wxYES 0x0002
#define wxNO 0x0001
#define wxYES_NO (wxYES | wxNO)
#define wxICON_INFORMATION 0x0800
#define wxICON_WARNING 0x0100
#define wxICON_ERROR 0x0200
#define wxICON_QUESTION 0x0400

#define wxHORIZONTAL 0x0004
#define wxVERTICAL 0x0008
#define wxBOTH (wxHORIZONTAL | wxVERTICAL)
#define wxEXPAND 0x2000
#define wxALL 0x0010
#define wxALIGN_CENTER 0x0900
#define wxALIGN_LEFT 0x0000
#define wxALIGN_RIGHT 0x0200

#define wxDefaultCoord (-1)

#define wxDECLARE_EVENT_TABLE()
#define DECLARE_EVENT_TABLE()
#define wxBEGIN_EVENT_TABLE(a, b)
#define BEGIN_EVENT_TABLE(a, b)
#define wxEND_EVENT_TABLE()
#define END_EVENT_TABLE()
#define EVT_MENU(id, fn)
#define EVT_IDLE(fn)
#define EVT_CLOSE(fn)
#define EVT_SIZE(fn)
#define EVT_PAINT(fn)
#define EVT_TIMER(id, fn)
#define EVT_COMMAND(id, evt, fn)
#define EVT_BUTTON(id, fn)
#define EVT_CHOICE(id, fn)
#define EVT_CHECKBOX(id, fn)
#define EVT_TEXT(id, fn)
#define EVT_SLIDER(id, fn)
#define EVT_TOOL(id, fn)
#define EVT_AUITOOLBAR_TOOL_DROPDOWN(id, fn)
#define EVT_NOTEBOOK_PAGE_CHANGED(id, fn)
#define EVT_LIST_ITEM_SELECTED(id, fn)
#define EVT_TREE_SEL_CHANGED(id, fn)
#define EVT_KEY_DOWN(fn)
#define EVT_KEY_UP(fn)
#define EVT_LEFT_DOWN(fn)
#define EVT_LEFT_UP(fn)
#define EVT_RIGHT_DOWN(fn)
#define EVT_MOTION(fn)
#define EVT_MOUSEWHEEL(fn)
#define EVT_ERASE_BACKGROUND(fn)
#define EVT_SET_FOCUS(fn)
#define EVT_KILL_FOCUS(fn)
#define EVT_DROP_FILES(fn)

#define wxDECLARE_EVENT(name, type) inline const int name = 0
#define wxDEFINE_EVENT(name, type)
#define wxDECLARE_DYNAMIC_CLASS(name)
#define wxIMPLEMENT_DYNAMIC_CLASS(name, base)
#define wxDECLARE_CLASS(name)
#define wxIMPLEMENT_CLASS(name, base)
#define wxIMPLEMENT_APP(appclass)
#define IMPLEMENT_APP(appclass)

#define wxOVERRIDE override

using wxWindowID = int;
using wxCoord = int;
using wxByte = unsigned char;
using wxUint8 = uint8_t;
using wxUint16 = uint16_t;
using wxUint32 = uint32_t;
using wxInt32 = int32_t;
using wxUIntPtr = uintptr_t;
using WXTYPE = int;

class wxString {
public:
	wxString() = default;
	wxString(const char* value) :
		value_(value ? value : "") { }
	wxString(const std::string& value) :
		value_(value) { }
	wxString(std::string&& value) :
		value_(std::move(value)) { }
	wxString(std::string_view value) :
		value_(value) { }
	wxString(char ch, size_t count = 1) :
		value_(count, ch) { }

	static wxString FromUTF8(const char* value) {
		return wxString(value);
	}
	static wxString FromUTF8(const std::string& value) {
		return wxString(value);
	}
	static wxString Format(const char* fmt, ...) {
		(void)fmt;
		return {};
	}

	const char* c_str() const { return value_.c_str(); }
	const char* utf8_str() const { return value_.c_str(); }
	std::string ToStdString() const { return value_; }
	std::string ToUTF8() const { return value_; }
	const std::string& utf8_string() const { return value_; }

	bool empty() const { return value_.empty(); }
	bool IsEmpty() const { return value_.empty(); }
	size_t length() const { return value_.size(); }
	size_t Length() const { return value_.size(); }
	size_t size() const { return value_.size(); }
	void clear() { value_.clear(); }
	void Clear() { value_.clear(); }

	wxString& operator=(const char* value) {
		value_ = value ? value : "";
		return *this;
	}
	wxString& operator=(const std::string& value) {
		value_ = value;
		return *this;
	}
	wxString& operator+=(const wxString& other) {
		value_ += other.value_;
		return *this;
	}
	wxString& operator+=(const char* other) {
		if (other) {
			value_ += other;
		}
		return *this;
	}
	wxString operator+(const wxString& other) const { return wxString(value_ + other.value_); }
	wxString operator+(const char* other) const { return wxString(value_ + (other ? other : "")); }

	bool operator==(const wxString& other) const { return value_ == other.value_; }
	bool operator==(const char* other) const { return value_ == (other ? other : ""); }
	bool operator!=(const wxString& other) const { return !(*this == other); }
	bool operator<(const wxString& other) const { return value_ < other.value_; }
	char operator[](size_t index) const { return value_.at(index); }

	int Cmp(const wxString& other) const { return value_.compare(other.value_); }
	int CmpNoCase(const wxString& other) const {
		const size_t n = std::min(value_.size(), other.value_.size());
		for (size_t i = 0; i < n; ++i) {
			const int a = std::tolower(static_cast<unsigned char>(value_[i]));
			const int b = std::tolower(static_cast<unsigned char>(other.value_[i]));
			if (a != b) {
				return a - b;
			}
		}
		if (value_.size() == other.value_.size()) {
			return 0;
		}
		return value_.size() < other.value_.size() ? -1 : 1;
	}

	wxString Lower() const {
		wxString out = *this;
		std::transform(out.value_.begin(), out.value_.end(), out.value_.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return out;
	}
	wxString Upper() const {
		wxString out = *this;
		std::transform(out.value_.begin(), out.value_.end(), out.value_.begin(), [](unsigned char c) {
			return static_cast<char>(std::toupper(c));
		});
		return out;
	}
	wxString Trim(bool fromRight = true) const {
		std::string copy = value_;
		const auto pred = [](unsigned char c) { return std::isspace(c) == 0; };
		if (fromRight) {
			copy.erase(std::find_if(copy.rbegin(), copy.rend(), pred).base(), copy.end());
		} else {
			copy.erase(copy.begin(), std::find_if(copy.begin(), copy.end(), pred));
		}
		return wxString(std::move(copy));
	}

	bool Contains(const wxString& needle) const { return value_.find(needle.value_) != std::string::npos; }
	bool StartsWith(const wxString& prefix) const { return value_.rfind(prefix.value_, 0) == 0; }
	bool EndsWith(const wxString& suffix) const {
		return value_.size() >= suffix.value_.size()
			&& value_.compare(value_.size() - suffix.value_.size(), suffix.value_.size(), suffix.value_) == 0;
	}

	friend wxString operator+(const char* lhs, const wxString& rhs) {
		return wxString(std::string(lhs ? lhs : "") + rhs.value_);
	}

private:
	std::string value_;
};

using wxArrayString = std::vector<wxString>;

inline wxString wxEmptyString;

class wxPoint {
public:
	int x = 0;
	int y = 0;
	wxPoint() = default;
	wxPoint(int x_, int y_) :
		x(x_), y(y_) { }
};

class wxSize {
public:
	int x = 0;
	int y = 0;
	wxSize() = default;
	wxSize(int w, int h) :
		x(w), y(h) { }
	int GetWidth() const { return x; }
	int GetHeight() const { return y; }
	void Set(int w, int h) {
		x = w;
		y = h;
	}
};

class wxRect {
public:
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	wxRect() = default;
	wxRect(int x_, int y_, int w, int h) :
		x(x_), y(y_), width(w), height(h) { }
	wxRect(const wxPoint& pos, const wxSize& size) :
		x(pos.x), y(pos.y), width(size.x), height(size.y) { }
};

class wxColour {
public:
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	unsigned char a = 255;
	wxColour() = default;
	wxColour(unsigned char r_, unsigned char g_, unsigned char b_, unsigned char a_ = 255) :
		r(r_), g(g_), b(b_), a(a_) { }
	unsigned char Red() const { return r; }
	unsigned char Green() const { return g; }
	unsigned char Blue() const { return b; }
	unsigned char Alpha() const { return a; }
};

inline const wxPoint wxDefaultPosition{wxDefaultCoord, wxDefaultCoord};
inline const wxSize wxDefaultSize{wxDefaultCoord, wxDefaultCoord};

class wxObject {
public:
	virtual ~wxObject() = default;
};

class wxEvent : public wxObject {
public:
	void Skip(bool skip = true) { skipped_ = skip; }
	bool GetSkipped() const { return skipped_; }
	int GetId() const { return id_; }
	void SetId(int id) { id_ = id; }

protected:
	int id_ = wxID_ANY;
	bool skipped_ = true;
};

class wxCommandEvent : public wxEvent {
public:
	wxCommandEvent() = default;
	explicit wxCommandEvent(int id) { SetId(id); }
	void SetString(const wxString& value) { string_ = value; }
	const wxString& GetString() const { return string_; }
	void SetInt(int value) { int_ = value; }
	int GetInt() const { return int_; }

private:
	wxString string_;
	int int_ = 0;
};

class wxCloseEvent : public wxEvent {
public:
	void Veto(bool veto = true) { vetoed_ = veto; }
	bool GetVeto() const { return vetoed_; }
	void CanVeto(bool can) { can_veto_ = can; }

private:
	bool vetoed_ = false;
	bool can_veto_ = true;
};

class wxIdleEvent : public wxEvent { };
class wxSizeEvent : public wxEvent { };
class wxPaintEvent : public wxEvent { };
class wxKeyEvent : public wxEvent {
public:
	int GetKeyCode() const { return 0; }
};
class wxMouseEvent : public wxEvent {
public:
	int GetX() const { return 0; }
	int GetY() const { return 0; }
	int GetWheelRotation() const { return 0; }
};
class wxTimerEvent : public wxEvent { };
class wxFocusEvent : public wxEvent { };
class wxDropFilesEvent : public wxEvent { };
class wxAuiToolBarEvent : public wxCommandEvent { };
class wxNotebookEvent : public wxCommandEvent { };
class wxListEvent : public wxCommandEvent { };
class wxTreeEvent : public wxCommandEvent { };

class wxEvtHandler : public wxObject {
public:
	virtual bool ProcessEvent(wxEvent&) { return false; }
	void Bind(int, const std::function<void(wxCommandEvent&)>&) { }
	void Unbind(int, const std::function<void(wxCommandEvent&)>&) { }
};

class wxWindow : public wxEvtHandler {
public:
	wxWindow() = default;
	wxWindow(wxWindow*, wxWindowID, const wxPoint& = wxDefaultPosition, const wxSize& = wxDefaultSize) { }
	virtual ~wxWindow() = default;

	virtual bool Show(bool show = true) {
		shown_ = show;
		return true;
	}
	virtual bool Hide() { return Show(false); }
	virtual void SetSize(const wxSize& size) { size_ = size; }
	virtual void SetSize(int, int, int w, int h) { size_ = wxSize(w, h); }
	virtual wxSize GetSize() const { return size_; }
	virtual void SetPosition(const wxPoint& pos) { pos_ = pos; }
	virtual wxPoint GetPosition() const { return pos_; }
	virtual void SetLabel(const wxString& label) { label_ = label; }
	virtual wxString GetLabel() const { return label_; }
	virtual void SetName(const wxString& name) { name_ = name; }
	virtual wxString GetName() const { return name_; }
	virtual void Enable(bool enable = true) { enabled_ = enable; }
	virtual bool IsEnabled() const { return enabled_; }
	virtual void Refresh(bool = true) { }
	virtual void Update() { }
	virtual void Destroy() { }
	virtual void SetFocus() { }
	virtual void Raise() { }
	virtual void Centre(int = 0) { }
	virtual void Center(int flags = 0) { Centre(flags); }
	virtual wxWindow* GetParent() const { return parent_; }
	virtual void SetParent(wxWindow* parent) { parent_ = parent; }
	virtual int FromDIP(int value) const { return value; }
	virtual wxSize FromDIP(const wxSize& value) const { return value; }
	void SetClientSize(const wxSize& size) { size_ = size; }
	wxSize GetClientSize() const { return size_; }
	void SetMinSize(const wxSize&) { }
	void SetMaxSize(const wxSize&) { }
	void SetToolTip(const wxString&) { }
	void SetCursor(int) { }
	void Freeze() { }
	void Thaw() { }
	void Layout() { }

protected:
	wxWindow* parent_ = nullptr;
	wxString name_;
	wxString label_;
	wxPoint pos_;
	wxSize size_{800, 600};
	bool shown_ = true;
	bool enabled_ = true;
};

class wxPanel : public wxWindow {
public:
	wxPanel() = default;
	wxPanel(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) :
		wxWindow(parent, id, pos, size) { }
};

class wxControl : public wxWindow { };
class wxTopLevelWindow : public wxWindow {
public:
	void SetTitle(const wxString& title) { title_ = title; }
	wxString GetTitle() const { return title_; }

private:
	wxString title_;
};

class wxFrame : public wxTopLevelWindow {
public:
	wxFrame() = default;
	wxFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) :
		wxTopLevelWindow() {
		SetParent(parent);
		SetTitle(title);
		(void)id;
		(void)pos;
		(void)size;
	}
	void CreateStatusBar(int = 1) { }
	void SetStatusText(const wxString&, int = 0) { }
	void SetMenuBar(void*) { }
	void* GetMenuBar() const { return nullptr; }
	void SetToolBar(void*) { }
};

class wxDialog : public wxTopLevelWindow {
public:
	wxDialog() = default;
	wxDialog(wxWindow* parent, wxWindowID id, const wxString& title) {
		SetParent(parent);
		SetTitle(title);
		(void)id;
	}
	int ShowModal() { return wxID_CANCEL; }
	void EndModal(int) { }
};

class wxApp : public wxEvtHandler {
public:
	virtual ~wxApp() = default;
	virtual bool OnInit() { return true; }
	virtual int OnExit() { return 0; }
	virtual void OnEventLoopEnter(void*) { }
	virtual void OnFatalException() { }
	void SetTopWindow(wxWindow* window) { top_ = window; }
	wxWindow* GetTopWindow() const { return top_; }

private:
	wxWindow* top_ = nullptr;
};

class wxEventLoopBase { };
class wxSingleInstanceChecker {
public:
	bool IsAnotherRunning() const { return false; }
};

class wxFileName {
public:
	wxFileName() = default;
	explicit wxFileName(const wxString& path) :
		path_(path.ToStdString()) { }
	explicit wxFileName(const std::string& path) :
		path_(path) { }
	explicit wxFileName(const char* path) :
		path_(path ? path : "") { }

	static wxFileName DirName(const wxString& path) { return wxFileName(path); }
	static wxFileName FileName(const wxString& path) { return wxFileName(path); }

	void Assign(const wxString& path) { path_ = path.ToStdString(); }
	void Assign(const wxFileName& other) { path_ = other.path_; }
	void Clear() { path_.clear(); }

	wxString GetFullPath() const { return wxString(path_); }
	wxString GetPath() const {
		const auto pos = path_.find_last_of("/\\");
		if (pos == std::string::npos) {
			return {};
		}
		return wxString(path_.substr(0, pos));
	}
	wxString GetFullName() const {
		const auto pos = path_.find_last_of("/\\");
		if (pos == std::string::npos) {
			return wxString(path_);
		}
		return wxString(path_.substr(pos + 1));
	}
	wxString GetName() const {
		wxString full = GetFullName();
		const std::string s = full.ToStdString();
		const auto pos = s.find_last_of('.');
		if (pos == std::string::npos) {
			return full;
		}
		return wxString(s.substr(0, pos));
	}
	wxString GetExt() const {
		const auto pos = path_.find_last_of('.');
		if (pos == std::string::npos) {
			return {};
		}
		return wxString(path_.substr(pos + 1));
	}
	void SetExt(const wxString& ext) {
		const auto pos = path_.find_last_of('.');
		if (pos == std::string::npos) {
			path_ += ".";
			path_ += ext.ToStdString();
		} else {
			path_ = path_.substr(0, pos + 1) + ext.ToStdString();
		}
	}
	void SetFullName(const wxString& name) {
		const auto pos = path_.find_last_of("/\\");
		if (pos == std::string::npos) {
			path_ = name.ToStdString();
		} else {
			path_ = path_.substr(0, pos + 1) + name.ToStdString();
		}
	}
	bool HasExt() const { return !GetExt().IsEmpty(); }
	bool FileExists() const;
	bool DirExists() const;
	bool IsOk() const { return !path_.empty(); }
	bool IsDir() const { return DirExists(); }

private:
	std::string path_;
};

class wxPathList {
public:
	void Add(const wxString&) { }
	wxString FindValidPath(const wxString& name) const { return name; }
};

class wxStandardPaths {
public:
	static wxStandardPaths& Get() {
		static wxStandardPaths instance;
		return instance;
	}
	wxString GetUserDataDir() const { return wxString("/persist"); }
	wxString GetResourcesDir() const { return wxString("/assets"); }
	wxString GetExecutablePath() const { return wxString("/"); }
	wxString GetDataDir() const { return wxString("/assets"); }
};

class wxFile {
public:
	enum OpenMode { read, write, read_write, write_append };
	wxFile() = default;
	explicit wxFile(const wxString&) { }
	bool Open(const wxString&, OpenMode = read) { return false; }
	bool IsOpened() const { return false; }
	void Close() { }
	ssize_t Read(void*, size_t) { return 0; }
	ssize_t Write(const void*, size_t) { return 0; }
};

class wxDir {
public:
	explicit wxDir(const wxString&) { }
	bool IsOpened() const { return false; }
	bool HasFiles(const wxString& = {}) const { return false; }
	bool GetFirst(wxString*, const wxString& = {}, int = 0) const { return false; }
	bool GetNext(wxString*) const { return false; }
	static bool Exists(const wxString&);
};

class wxMenu : public wxEvtHandler {
public:
	explicit wxMenu(const wxString& = {}) { }
	void Append(int, const wxString&, const wxString& = {}) { }
	void AppendCheckItem(int, const wxString&) { }
	void AppendSeparator() { }
	void AppendSubMenu(wxMenu*, const wxString&) { }
	void Enable(int, bool) { }
	void Check(int, bool) { }
	bool IsChecked(int) const { return false; }
};

class wxMenuBar : public wxWindow {
public:
	void Append(wxMenu*, const wxString&) { }
	wxMenu* GetMenu(size_t) const { return nullptr; }
	void Enable(int, bool) { }
	void Check(int, bool) { }
};

class wxTimer : public wxEvtHandler {
public:
	void SetOwner(wxEvtHandler*, int = wxID_ANY) { }
	bool Start(int, bool = false) { return true; }
	void Stop() { }
	bool IsRunning() const { return false; }
};

class wxDC : public wxObject {
public:
	void Clear() { }
	void SetPen(const wxObject&) { }
	void SetBrush(const wxObject&) { }
	void DrawRectangle(int, int, int, int) { }
	void DrawText(const wxString&, int, int) { }
	void SetUserScale(double, double) { }
};
class wxPaintDC : public wxDC {
public:
	explicit wxPaintDC(wxWindow*) { }
};
class wxClientDC : public wxDC {
public:
	explicit wxClientDC(wxWindow*) { }
};
class wxMemoryDC : public wxDC { };
class wxBufferedPaintDC : public wxDC {
public:
	explicit wxBufferedPaintDC(wxWindow*) { }
};

class wxImage : public wxObject {
public:
	wxImage() = default;
	wxImage(int, int) { }
	bool IsOk() const { return false; }
	int GetWidth() const { return 0; }
	int GetHeight() const { return 0; }
};
class wxBitmap : public wxObject {
public:
	wxBitmap() = default;
	wxBitmap(int, int) { }
	explicit wxBitmap(const wxImage&) { }
	bool IsOk() const { return false; }
	wxImage ConvertToImage() const { return {}; }
};
class wxIcon : public wxBitmap { };
class wxCursor : public wxObject { };
class wxPen : public wxObject {
public:
	wxPen() = default;
	wxPen(const wxColour&, int = 1) { }
};
class wxBrush : public wxObject {
public:
	wxBrush() = default;
	explicit wxBrush(const wxColour&) { }
};

class wxGLContext : public wxObject { };
class wxGLCanvas : public wxWindow {
public:
	wxGLCanvas() = default;
	wxGLCanvas(wxWindow* parent, wxWindowID id = wxID_ANY) :
		wxWindow(parent, id) { }
	bool SetCurrent(const wxGLContext&) const { return true; }
	void SwapBuffers() { }
};

class wxAuiManager : public wxEvtHandler {
public:
	void SetManagedWindow(wxWindow*) { }
	void AddPane(wxWindow*, int, const wxString& = {}) { }
	void Update() { }
	void UnInit() { }
};
class wxAuiNotebook : public wxWindow { };
class wxAuiToolBar : public wxWindow {
public:
	void AddTool(int, const wxString&, const wxBitmap&, const wxString& = {}) { }
	void Realize() { }
};

class wxNotebook : public wxWindow { };
class wxListCtrl : public wxWindow { };
class wxTreeCtrl : public wxWindow { };
class wxTextCtrl : public wxControl { };
class wxStaticText : public wxControl { };
class wxButton : public wxControl { };
class wxBitmapButton : public wxButton { };
class wxCheckBox : public wxControl { };
class wxChoice : public wxControl { };
class wxComboBox : public wxControl { };
class wxSlider : public wxControl { };
class wxSpinCtrl : public wxControl { };
class wxRadioBox : public wxControl { };
class wxToggleButton : public wxControl { };
class wxStatusBar : public wxWindow { };
class wxToolBar : public wxWindow { };
class wxScrolledWindow : public wxPanel { };
class wxSplitterWindow : public wxWindow { };
class wxMiniFrame : public wxFrame { };
class wxHtmlWindow : public wxWindow { };
class wxGrid : public wxWindow { };
class wxVListBox : public wxWindow { };
class wxGenericDirCtrl : public wxWindow { };
class wxCollapsiblePane : public wxWindow { };

class wxBoxSizer : public wxObject {
public:
	explicit wxBoxSizer(int) { }
	void Add(wxWindow*, int = 0, int = 0, int = 0) { }
	void AddSpacer(int) { }
	void AddStretchSpacer(int = 1) { }
};
class wxStaticBoxSizer : public wxBoxSizer {
public:
	wxStaticBoxSizer(int orient, wxWindow*, const wxString& = {}) :
		wxBoxSizer(orient) { }
};
class wxFlexGridSizer : public wxBoxSizer {
public:
	wxFlexGridSizer(int, int, int) :
		wxBoxSizer(wxVERTICAL) { }
};
class wxStaticLine : public wxControl { };
class wxStaticBitmap : public wxControl { };

class wxFileDialog : public wxDialog {
public:
	wxFileDialog(wxWindow*, const wxString&, const wxString& = {}, const wxString& = {}, const wxString& = {}, long = 0) { }
	wxString GetPath() const { return {}; }
	wxArrayString GetPaths() const { return {}; }
};
class wxDirDialog : public wxDialog { };
class wxMessageDialog : public wxDialog { };
class wxTextEntryDialog : public wxDialog { };
class wxColourDialog : public wxDialog { };
class wxProgressDialog : public wxDialog {
public:
	wxProgressDialog(const wxString&, const wxString&, int = 100, wxWindow* = nullptr, int = 0) { }
	bool Update(int, const wxString& = {}) { return true; }
};
class wxBusyInfo {
public:
	explicit wxBusyInfo(const wxString&) { }
};
class wxTipWindow : public wxWindow { };

class wxConfigBase {
public:
	virtual ~wxConfigBase() = default;
	static wxConfigBase* Get() {
		static wxConfigBase instance;
		return &instance;
	}
	virtual bool Read(const wxString&, wxString*) { return false; }
	virtual bool Read(const wxString&, int*) { return false; }
	virtual bool Write(const wxString&, const wxString&) { return false; }
	virtual bool Write(const wxString&, int) { return false; }
};
class wxFileConfig : public wxConfigBase { };

class wxRegEx {
public:
	explicit wxRegEx(const wxString&) { }
	bool IsValid() const { return false; }
	bool Matches(const wxString&) const { return false; }
};

class wxXmlDocument {
public:
	bool Load(const wxString&) { return false; }
	bool Save(const wxString&) const { return false; }
};
class wxXmlNode { };

class wxInputStream { };
class wxOutputStream { };
class wxFileInputStream : public wxInputStream {
public:
	explicit wxFileInputStream(const wxString&) { }
	bool IsOk() const { return false; }
};
class wxFileOutputStream : public wxOutputStream {
public:
	explicit wxFileOutputStream(const wxString&) { }
	bool IsOk() const { return false; }
};
class wxMemoryInputStream : public wxInputStream {
public:
	wxMemoryInputStream(const void*, size_t) { }
};
class wxStringInputStream : public wxInputStream {
public:
	explicit wxStringInputStream(const wxString&) { }
};
class wxTextInputStream {
public:
	explicit wxTextInputStream(wxInputStream&) { }
	wxString ReadLine() { return {}; }
};
class wxZlibInputStream : public wxInputStream {
public:
	explicit wxZlibInputStream(wxInputStream&) { }
};

class wxClipboard {
public:
	static wxClipboard* Get() {
		static wxClipboard instance;
		return &instance;
	}
	bool Open() { return false; }
	void Close() { }
	bool SetData(void*) { return false; }
	bool GetData(void&) { return false; }
};
class wxTextDataObject {
public:
	explicit wxTextDataObject(const wxString& = {}) { }
	wxString GetText() const { return {}; }
};
class wxFileDataObject { };
class wxFileDropTarget { };
class wxDropTarget { };

class wxThread {
public:
	enum ExitCode : int { };
	virtual ~wxThread() = default;
	virtual ExitCode Entry() { return ExitCode{}; }
	void Run() { }
	void Wait() { }
};
class wxMutex { };
class wxCriticalSection { };
class wxCriticalSectionLocker {
public:
	explicit wxCriticalSectionLocker(wxCriticalSection&) { }
};
class wxCondition { };
class wxSemaphore { };

class wxDateTime {
public:
	static wxDateTime Now() { return {}; }
	wxString FormatISOCombined() const { return {}; }
};
class wxStopWatch {
public:
	void Start(long = 0) { }
	void Pause() { }
	void Resume() { }
	long Time() const { return 0; }
};

class wxLog {
public:
	static void EnableLogging(bool) { }
};
class wxLogNull { };

class wxCmdLineParser {
public:
	void AddParam(const wxString& = {}, int = 0, int = 0) { }
	void AddOption(const wxString&, const wxString& = {}, const wxString& = {}) { }
	bool Found(const wxString&, wxString* = nullptr) const { return false; }
	int Parse(bool = true) { return 0; }
};

class wxDisplay {
public:
	explicit wxDisplay(unsigned = 0) { }
	wxRect GetClientArea() const { return {0, 0, 1280, 800}; }
	static unsigned GetCount() { return 1; }
};

class wxArtProvider {
public:
	static wxBitmap GetBitmap(const wxString&, const wxString& = {}, const wxSize& = wxDefaultSize) { return {}; }
};

class wxURL { };
class wxHTTP { };

class wxHashMap { };
template <typename T>
class wxVector : public std::vector<T> { };
template <typename T>
class wxSharedPtr {
public:
	wxSharedPtr() = default;
	explicit wxSharedPtr(T* ptr) :
		ptr_(ptr) { }
	T* get() const { return ptr_; }
	T& operator*() const { return *ptr_; }
	T* operator->() const { return ptr_; }

private:
	T* ptr_ = nullptr;
};

class wxStringTokenizer {
public:
	wxStringTokenizer() = default;
	wxStringTokenizer(const wxString&, const wxString& = {}) { }
	bool HasMoreTokens() const { return false; }
	wxString GetNextToken() { return {}; }
};

class wxBase64 { };

inline void wxLogMessage(const wxString&) { }
inline void wxLogWarning(const wxString&) { }
inline void wxLogError(const wxString&) { }
inline void wxLogDebug(const wxString&) { }
inline void wxBell() { }
inline void wxYield() { }
inline void wxMilliSleep(unsigned) { }
inline void wxSleep(int) { }
inline int wxMessageBox(const wxString&, const wxString& = {}, int = wxOK, wxWindow* = nullptr) { return wxID_OK; }
inline wxString wxGetCwd() { return wxString("/"); }
inline bool wxSetWorkingDirectory(const wxString&) { return true; }
inline bool wxFileExists(const wxString& path);
inline bool wxDirExists(const wxString& path);
inline bool wxMkdir(const wxString& path, int = 0);
inline bool wxRemoveFile(const wxString& path);
inline bool wxRenameFile(const wxString& from, const wxString& to);
inline wxString wxFileNameFromPath(const wxString& path) { return wxFileName(path).GetFullName(); }

#if defined(__EMSCRIPTEN__)
	#include <cerrno>
	#include <sys/stat.h>
	#include <unistd.h>

inline bool wxFileExists(const wxString& path) {
	struct stat st {};
	return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
inline bool wxDirExists(const wxString& path) {
	struct stat st {};
	return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
inline bool wxMkdir(const wxString& path, int) {
	return ::mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
}
inline bool wxRemoveFile(const wxString& path) {
	return ::unlink(path.c_str()) == 0;
}
inline bool wxRenameFile(const wxString& from, const wxString& to) {
	return ::rename(from.c_str(), to.c_str()) == 0;
}
inline bool wxFileName::FileExists() const {
	return wxFileExists(wxString(path_));
}
inline bool wxFileName::DirExists() const {
	return wxDirExists(wxString(path_));
}
inline bool wxDir::Exists(const wxString& path) {
	return wxDirExists(path);
}
#else
inline bool wxFileExists(const wxString&) { return false; }
inline bool wxDirExists(const wxString&) { return false; }
inline bool wxMkdir(const wxString&, int) { return false; }
inline bool wxRemoveFile(const wxString&) { return false; }
inline bool wxRenameFile(const wxString&, const wxString&) { return false; }
inline bool wxFileName::FileExists() const { return false; }
inline bool wxFileName::DirExists() const { return false; }
inline bool wxDir::Exists(const wxString&) { return false; }
#endif

#define FROM_DIP(widget, size) (size)
