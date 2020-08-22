#pragma once

#include <wx/app.h>
#include <sigc++/connection.h>
#include "modulesystem/ApplicationContextImpl.h"
#include "module/CoreModule.h"

/**
* Main application class required by wxWidgets
*
* This is the longest-lived class in the system, and is instantiated
* automatically by wxWidgets in place of an explicit main function. Pre-event
* loop initialisation occurs in OnInit(), and post-event loop shutdown in
* OnExit().
*
* Not to be confused with the RadiantModule which implements the IRadiant
* interface.
*/
class RadiantApp : 
	public wxApp
{
	// The RadiantApp owns the ApplicationContext which is then passed to the
	// ModuleRegistry as a reference.
	radiant::ApplicationContextImpl _context;

	std::unique_ptr<module::CoreModule> _coreModule;

	sigc::connection _modulesUnloadingHandler;

public:
	bool OnInit() override;
	int OnExit() override;

	// Override this to allow for custom command line args
	void OnInitCmdLine(wxCmdLineParser& parser) override;

	bool OnExceptionInMainLoop() override;

private:
	void onStartupEvent(wxCommandEvent& ev);
	void onModulesUnloading();
};
