#include "GameSetupPageTdm.h"

#include "i18n.h"
#include "imodule.h"
#include "igame.h"

#include "wxutil/dialog/MessageBox.h"
#include "wxutil/PathEntry.h"

#include <fmt/format.h>
#include <wx/sizer.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/panel.h>

#include "string/trim.h"
#include "os/file.h"
#include "os/path.h"
#include "os/dir.h"

namespace ui
{

GameSetupPageTdm::GameSetupPageTdm(wxWindow* parent, const game::IGamePtr& game) :
	GameSetupPage(parent, game),
	_missionEntry(nullptr),
	_enginePathEntry(nullptr)
{
	wxFlexGridSizer* table = new wxFlexGridSizer(2, 2, wxSize(6, 6));
	table->AddGrowableCol(1);
	this->SetSizer(table);

	_enginePathEntry = new wxutil::PathEntry(this, true);
	_enginePathEntry->getEntryWidget()->SetMinClientSize(
		wxSize(_enginePathEntry->getEntryWidget()->GetCharWidth() * 30, -1));

	table->Add(new wxStaticText(this, wxID_ANY, _("DarkMod Path")), 0, wxALIGN_CENTRE_VERTICAL);
	table->Add(_enginePathEntry, 1, wxEXPAND);

	_enginePathEntry->getEntryWidget()->Bind(wxEVT_TEXT, [&](wxCommandEvent& ev)
	{
		populateAvailableMissionPaths();
	});

	_enginePathEntry->getEntryWidget()->SetToolTip(_("This is the path where your TheDarkMod.exe is located."));

	_missionEntry = new wxComboBox(this, wxID_ANY);
	_missionEntry->SetMinClientSize(wxSize(_missionEntry->GetCharWidth() * 30, -1));
	_missionEntry->SetToolTip(_("The FM folder name of the mission you want to work on, e.g. 'saintlucia'."));

	table->Add(new wxStaticText(this, wxID_ANY, _("Mission")), 0, wxALIGN_CENTRE_VERTICAL);
	table->Add(_missionEntry, 1, wxEXPAND);

	// Determine the fms folder name where the missions are stored, defaults to "fms/"
	_fmFolder = "fms/";
	
	xml::NodeList nodes = game->getLocalXPath("/gameSetup/missionFolder");

	if (!nodes.empty())
	{
		std::string value = nodes[0].getAttributeValue("value");

		if (!value.empty())
		{
			_fmFolder = os::standardPathWithSlash(value);
		}
	}

	populateAvailableMissionPaths();
}

const char* GameSetupPageTdm::TYPE()
{
	return "TDM";
}

const char* GameSetupPageTdm::getType()
{
	return TYPE();
}

bool GameSetupPageTdm::onPreSave()
{
	constructPaths();

	// Validate the engine path first, otherwise we can't do anything
	if (!os::fileOrDirExists(_config.enginePath))
	{
		return true; // proceed to normal validation routine, which will error out anyway
	}

	// Check the mod path (=mission path), if not empty
	if (!_config.modPath.empty() && !os::fileOrDirExists(_config.modPath))
	{
		// Mod name is not empty, but mod folder doesnt' exist, this might indicate that 
		// the user wants to start a new mission, ask him whether we should create the folder
		std::string missionName = _missionEntry->GetValue().ToStdString();
		std::string msg = fmt::format(_("The mission path {0} doesn't exist.\nDo you intend to start a "
			"new mission and create that folder?"), _config.modPath);

		if (wxutil::Messagebox::Show(fmt::format(_("Start a new Mission named {0}?"), missionName),
			msg, IDialog::MESSAGE_ASK, wxGetTopLevelParent(this)) == wxutil::Messagebox::RESULT_YES)
		{
			// User wants to create the path
			rMessage() << "Creating mission directory " << _config.modPath << std::endl;

			// Create the directory
			if (!os::makeDirectory(_config.modPath))
			{
				wxutil::Messagebox::Show(_("Could not create directory"), fmt::format(_("Failed to create the folder {0}"), _config.modPath),
					IDialog::MessageType::MESSAGE_ERROR, wxGetTopLevelParent(this));

				// Veto the event
				return false;
			}

			// Everything went smooth, proceed to the normal save routine 
			return true;
		}
		
		// User doesn't want to create a new mission, so veto the save event
		return false;
	}

	// Engine path is OK, mod path is empty or exists already
	return true;
}

void GameSetupPageTdm::validateSettings()
{
	constructPaths();

	std::string errorMsg;

	// Validate the engine path first, otherwise we can't do anything
	if (!os::fileOrDirExists(_config.enginePath))
	{
		// Engine path doesn't exist
		errorMsg += fmt::format(_("Engine path \"{0}\" does not exist.\n"), _config.enginePath);
	}

	// Check if the TheDarkMod.exe file is in the right place
	fs::path darkmodExePath = _config.enginePath;

	// No engine path set so far, search the game file for default values
	const std::string ENGINE_EXECUTABLE_ATTRIBUTE =
#if defined(WIN32)
		"engine_win32"
#elif defined(__linux__) || defined (__FreeBSD__)
		"engine_linux"
#elif defined(__APPLE__)
		"engine_macos"
#else
#error "unknown platform"
#endif
		;

	std::string exeName = _game->getKeyValue(ENGINE_EXECUTABLE_ATTRIBUTE);

	if (!os::fileOrDirExists(darkmodExePath / exeName))
	{
		// engine executable not present
		errorMsg += fmt::format(_("The engine executable \"{0}\" could not be found in the specified folder.\n"), exeName);
	}

	// Check the mod path (=mission path), if not empty
	if (!_config.modPath.empty() && !os::fileOrDirExists(_config.modPath))
	{
		// Path not existent => error
		errorMsg += fmt::format(_("The mission path \"{0}\" does not exist.\n"), _config.modPath);
	}

	if (!errorMsg.empty())
	{
		throw GameSettingsInvalidException(errorMsg);
	}
}

void GameSetupPageTdm::onPageShown()
{
	// Load the values from the registry if the controls are still empty
	if (_enginePathEntry->getValue().empty())
	{
		_config.enginePath = registry::getValue<std::string>(RKEY_ENGINE_PATH);
		_config.enginePath = os::standardPathWithSlash(_config.enginePath);

		// If the engine path is empty, consult the .game file for a fallback value
		if (_config.enginePath.empty())
		{
			// No engine path set so far, search the game file for default values
			const std::string ENGINEPATH_ATTRIBUTE =
#if defined(WIN32)
				"enginepath_win32"
#elif defined(__linux__) || defined (__FreeBSD__)
				"enginepath_linux"
#elif defined(__APPLE__)
				"enginepath_macos"
#else
#error "unknown platform"
#endif
				;

			_config.enginePath = os::standardPathWithSlash(_game->getKeyValue(ENGINEPATH_ATTRIBUTE));
		}

		_enginePathEntry->setValue(_config.enginePath);
	}

	if (_missionEntry->GetValue().empty())
	{
		// Check if we have a valid mod path
		_config.modPath = registry::getValue<std::string>(RKEY_MOD_PATH);

		if (!_config.modPath.empty())
		{
			// Extract the mission name from the absolute mod path, if possible
			std::string fmPath = _config.enginePath + _fmFolder;

			std::string fmName = os::getRelativePath(_config.modPath, fmPath);
			string::trim(fmName, "/");

			_missionEntry->SetValue(fmName);
		}
	}
}

void GameSetupPageTdm::constructPaths()
{
	_config.enginePath = _enginePathEntry->getEntryWidget()->GetValue().ToStdString();

	// Make sure it's a well formatted path
	_config.enginePath = os::standardPathWithSlash(_config.enginePath);
	
	// No mod base path necessary
	_config.modBasePath.clear();

	// Load the mission folder name from the registry
	std::string fmName = _missionEntry->GetValue().ToStdString();

	if (!fmName.empty())
	{
		// greebo: #3480 check if the mod path is absolute. If not, append it to the engine path
		_config.modPath = fs::path(fmName).is_absolute() ? fmName : _config.enginePath + _fmFolder + string::trim_copy(fmName, "/");

		// Normalise the path as last step
		_config.modPath = os::standardPathWithSlash(_config.modPath);
	}
	else
	{
		// No mission name, no modpath
		_config.modPath.clear();
	}
}

void GameSetupPageTdm::populateAvailableMissionPaths()
{
	// Remember the current value, the Clear() call will remove any text
	std::string previousMissionValue = _missionEntry->GetValue().ToStdString();

	_missionEntry->Clear();

	fs::path enginePath = _enginePathEntry->getValue();

	if (enginePath.empty())
	{
		return;
	}

	try
	{
		fs::path fmPath = enginePath / _fmFolder;

		os::foreachItemInDirectory(fmPath.string(), [&](const fs::path& fmFolder)
		{
			// Skip the mission preview image folder
			if (fmFolder.filename() == "_missionshots") return;

			_missionEntry->AppendString(fmFolder.filename().string());
		});
	}
	catch (const os::DirectoryNotFoundException&)
	{}

	// Keep the previous value after repopulation
	_missionEntry->SetValue(previousMissionValue);
}

}
