#pragma once

#include "icommandsystem.h"
#include "wxutil/dialog/DialogBase.h"
#include "GameSetupPage.h"

class wxChoicebook;
class wxBookCtrlEvent;

namespace ui
{

/**
* A dialog aimed to assist the user with setting up their
* engine/game/mod paths. Use the Show() method to run the dialog,
* after this action the current settings are stored in the XMLRegistry:
*
* Game Type:	 RKEY_GAME_TYPE
* Engine Path:	 RKEY_ENGINE_PATH
* Mod Path:		 RKEY_FS_GAME
* Mod Base Path: RKEY_FS_GAME_BASE
*
* Registry key paths are stored in the igame.h header
*/
class GameSetupDialog :
	public wxutil::DialogBase
{
private:
	wxChoicebook* _book;

	GameSetupDialog(wxWindow* parent);

public:
	std::string getSelectedGameType();

	// The result after the user is done with the dialog
	struct Result
	{
		std::string gameName;	 // Display name of the selected game
		std::string enginePath;	 // selected engine path
		std::string modPath;	 // selected mod path
		std::string modBasePath; // selected mod base path
	};

	/** greebo: The command target to show the Game settings preferences.
	*/
	static Result Show(const cmd::ArgumentList& args);

private:
	GameSetupPage* getSelectedPage();
	GameSetupPage* getPage(int num);
	void setSelectedPage(const std::string& name);

	void initialiseControls();

	void onSave(wxCommandEvent& ev);
	void onCancel(wxCommandEvent& ev);
	void onPageChanged(wxBookCtrlEvent& ev);
};

}
