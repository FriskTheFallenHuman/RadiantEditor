#pragma once

#include "i18n.h"
#include "ui/imenumanager.h"
#include "icommandsystem.h"
#include "ui/imainframe.h"

#include "wxutil/dialog/MessageBox.h"

#include <string>
#include <functional>
#include <memory>

namespace ui 
{

namespace 
{
	const std::string MENU_LAYOUTS_PARENT = "main/view";
	const std::string MENU_LAYOUTS = "layouts";
	const std::string MENU_LAYOUTS_PATH = MENU_LAYOUTS_PARENT + "/" + MENU_LAYOUTS;
	const std::string MENU_LAYOUTS_INSERT_BEFORE = "main/view/camera";
}

/**
 * This little class represents a "command target", providing
 * a callback for activating a certain layout. The class
 * registers itself with the EventManager on construction.
 */
class LayoutCommand
{
	std::string _layoutName;

	std::string _activateCommand;
public:
	LayoutCommand(const std::string& layoutName) :
		_layoutName(layoutName)
	{
		_activateCommand = "ActivateLayout" + _layoutName;
		GlobalCommandSystem().addCommand(
			_activateCommand,
			std::bind(&LayoutCommand::activateLayout, this, std::placeholders::_1)
		);

		// Add commands to menu
		menu::IMenuManager& menuManager = GlobalMenuManager();

		// Add a new folder, if not existing yet
		if (!menuManager.exists(MENU_LAYOUTS_PATH))
		{
			menuManager.insert(
				MENU_LAYOUTS_INSERT_BEFORE,
				MENU_LAYOUTS,
				menu::ItemType::Folder, _("Window Layout"),
				"", "" // no icon, no event
			);
		}

		// Add the item
		menuManager.add(MENU_LAYOUTS_PATH, _layoutName, menu::ItemType::Item, _layoutName, "", _activateCommand);
	}

	~LayoutCommand()
	{
		// Remove command from menu
        GlobalMenuManager().remove(MENU_LAYOUTS_PATH + "/" + _layoutName);

		// Remove event and command
		GlobalCommandSystem().removeCommand(_activateCommand);
	}

	// Command target for activating the layout
	void activateLayout(const cmd::ArgumentList& args)
	{
		GlobalMainFrame().setActiveLayoutName(_layoutName);
        wxutil::Messagebox::Show(_("Restart required"),
                                 _("Restart DarkRadiant to apply changes"),
                                 IDialog::MESSAGE_CONFIRM);
	}
};
typedef std::shared_ptr<LayoutCommand> LayoutCommandPtr;

} // namespace ui
