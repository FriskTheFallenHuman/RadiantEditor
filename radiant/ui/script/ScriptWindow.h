#pragma once

#include "icommandsystem.h"

#include <wx/panel.h>
#include "wxutil/ConsoleView.h"

class wxCommandEvent;
namespace wxutil { class PythonSourceViewCtrl; }

namespace ui
{

class ScriptWindow :
	public wxPanel
{
private:
	// Use a standard console window for the script output
	wxutil::ConsoleView* _outView;

	wxutil::PythonSourceViewCtrl* _view;

public:
	ScriptWindow(wxWindow* parent);

	/**
	 * greebo: Static command target for toggling the script window.
	 */
	static void toggle(const cmd::ArgumentList& args);

private:
	void onRunScript(wxCommandEvent& ev);
};

} // namespace script
