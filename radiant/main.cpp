/**
 * DarkRadiant's application entry point, defines the wxApp instance
 */
#include "i18n.h"

#include "log/LogFile.h"
#include "log/PIDFile.h"
#include "log/LogStream.h"
#include "modulesystem/ModuleRegistry.h"
#include "modulesystem/ApplicationContextImpl.h"

#ifndef POSIX
#include "settings/LanguageManager.h"
#endif

#include <wx/wxprec.h>
#include <wx/app.h>
#include <wx/cmdline.h>

#ifdef POSIX
#include <libintl.h>
#endif
#include <exception>

#if defined (_DEBUG) && defined (WIN32) && defined (_MSC_VER)
#include "crtdbg.h"
#endif

void crt_init()
{
#if defined (_DEBUG) && defined (WIN32) && defined (_MSC_VER)
  _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
}

wxDEFINE_EVENT(EV_RadiantStartup, wxCommandEvent);

/// Main application class required by wxWidgets
/**
 * This is the longest-lived class in the system, and is instantiated
 * automatically by wxWidgets in place of an explicit main function. Pre-event
 * loop initialisation occurs in OnInit(), and post-event loop shutdown in
 * OnExit().
 *
 * Not to be confused with the RadiantModule which implements the IRadiant
 * interface.
 */
class RadiantApp : public wxApp
{
    // The RadiantApp owns the ApplicationContext which is then passed to the
    // ModuleRegistry as a refernce.
    radiant::ApplicationContextImpl _context;

public:

	virtual bool OnInit()
	{
		if ( !wxApp::OnInit() ) return false;

        // Initialise the debug flags
        crt_init();

        // Set the stream references for rMessage(), redirect std::cout, etc.
        applog::LogStream::InitialiseStreams();

        // Stop wx's unhelpful debug messages about missing keyboard accel
        // strings from cluttering up the console
        wxLog::SetLogLevel(wxLOG_Warning);

        // Initialise the context (application path / settings path, is
        // OS-specific)
        _context.initialise(wxApp::argc, wxApp::argv);
        module::ModuleRegistry::Instance().setContext(_context);

        // The settings path is set, start logging now
        applog::LogFile::create("darkradiant.log");

#ifndef POSIX
        // Initialise the language based on the settings in the user settings folder
        language::LanguageManager().init(_context);
#endif

#ifdef POSIX
        // greebo: not sure if this is needed
        // Other POSIX gettext initialisation
        setlocale(LC_ALL, "");
        textdomain(GETTEXT_PACKAGE);
        bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
#endif

        // reset some locale settings back to standard c
        // this is e.g. needed for parsing float values from textfiles
        setlocale(LC_NUMERIC, "C");
        setlocale(LC_TIME, "C");

		wxInitAllImageHandlers();

		// Register to the start up signal
		Connect(EV_RadiantStartup, wxCommandEventHandler(RadiantApp::onStartupEvent), NULL, this);

        // Activate the Popup Error Handler
        _context.initErrorHandler();

        AddPendingEvent(wxCommandEvent(EV_RadiantStartup));

		return true;
	}

    int OnExit()
    {
        // Issue a shutdown() call to all the modules
        module::GlobalModuleRegistry().shutdownModules();

        // Close the logfile
        applog::LogFile::close();
        applog::LogStream::ShutdownStreams();

        return wxApp::OnExit();
    }

    // Override this to allow for custom command line args
    void OnInitCmdLine(wxCmdLineParser& parser)
    {
        // Remove '/' as parameter starter to allow for "/path" style args
        parser.SetSwitchChars("-");

        parser.AddLongSwitch("disable-sound", _("Disable sound for this session."));
        parser.AddLongOption("verbose", _("Verbose logging."));

        parser.AddParam("Map file", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
        parser.AddParam("fs_game=<game>", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
        parser.AddParam("fs_game_base=<gamebase>", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
    }

private:
	void onStartupEvent(wxCommandEvent& ev)
	{
		// Create the radiant.pid file in the settings folder
        // (emits a warning if the file already exists (due to a previous startup failure))
        applog::PIDFile pidFile(PID_FILENAME);

        module::ModuleRegistry::Instance().loadModules();
        
        module::ModuleRegistry::Instance().initialiseModules();

        // Scope ends here, PIDFile is deleted by its destructor
	}
};

// Main entry point for the application.
wxIMPLEMENT_APP(RadiantApp);

