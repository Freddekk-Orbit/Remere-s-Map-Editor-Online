#include <wx/app.h>
#include <wx/aui/aui.h>
#include <wx/event.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/glcanvas.h>
#include <wx/string.h>
#include <wx/wx.h>
#include <wx/wxprec.h>

// Compiled into the Wasm target so the stub include path stays honest.
// When upstream RME sources are imported, <wx/...> resolves here instead of
// the desktop toolkit.

void rme_wx_stub_sanity() {
	wxString title("Remere's Map Editor");
	wxFileName map_path("/uploads/world.otbm");
	wxFrame frame(nullptr, wxID_ANY, title);
	(void)map_path.GetFullPath();
	(void)frame.GetTitle();
}
