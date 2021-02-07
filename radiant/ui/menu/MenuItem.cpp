#include "MenuItem.h"

#include "itextstream.h"
#include "ieventmanager.h"

#include "MenuFolder.h"
#include "wxutil/bitmap.h"

namespace ui
{

namespace menu
{

MenuItem::MenuItem() :
	_menuItem(nullptr)
{}

wxMenuItem* MenuItem::getMenuItem()
{
	if (_menuItem == nullptr)
	{
		construct();
	}

	return _menuItem;
}

void MenuItem::construct()
{
	if (_menuItem != nullptr)
	{
		MenuElement::constructChildren();
		return;
	}

	if (!isVisible())
	{
		MenuElement::constructChildren();
		return;
	}

	// Get the parent menu
	MenuElementPtr parent = getParent();

	if (!parent || !std::dynamic_pointer_cast<MenuFolder>(parent))
	{
		rWarning() << "Cannot construct item without a parent menu" << std::endl;
		return;
	}

	wxMenu* menu = std::static_pointer_cast<MenuFolder>(parent)->getMenu();

	std::string caption = _caption;

	// Create a new MenuElement
	_menuItem = new wxMenuItem(nullptr, _nextMenuItemId++, caption);
	
	if (!_icon.empty())
	{
		_menuItem->SetBitmap(wxutil::getBitmap(_icon));
	}

	bool isToggle = GlobalEventManager().findEvent(_event)->isToggle();
	_menuItem->SetCheckable(isToggle);

	int pos = parent->getMenuPosition(shared_from_this());
	menu->Insert(pos, _menuItem);

	GlobalEventManager().registerMenuItem(_event, _menuItem);

	if (_event.empty())
	{
		// Disable items without event
		menu->Enable(_menuItem->GetId(), false);
	}

	MenuElement::constructChildren();
}

void MenuItem::setAccelerator(const std::string& accelStr)
{
	if (_menuItem == nullptr) return;

	std::string caption = _caption + "\t " + accelStr;
	_menuItem->SetItemLabel(caption);
}

void MenuItem::deconstruct()
{
	// Destruct children first
	MenuElement::deconstructChildren();

	if (_menuItem != nullptr)
	{
		// Try to lookup the event name
		if (!_event.empty())
		{
			GlobalEventManager().unregisterMenuItem(_event, _menuItem);
		}

		if (_menuItem->GetMenu() != nullptr)
		{
			_menuItem->GetMenu()->Remove(_menuItem);
		}

		delete _menuItem;
		_menuItem = nullptr;
	}
}

bool MenuItem::isToggle() const
{
	return _menuItem != nullptr && _menuItem->IsCheckable();
}

void MenuItem::setToggled(bool isToggled)
{
	assert(isToggle());

	if (_menuItem != nullptr)
	{
		_menuItem->Check(isToggled);
	}
}

}

}
