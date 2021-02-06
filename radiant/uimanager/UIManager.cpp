#include "UIManager.h"
#include "module/StaticModule.h"

#include "i18n.h"
#include "itextstream.h"
#include "iregistry.h"
#include "iradiant.h"
#include "istatusbarmanager.h"
#include "icounter.h"
#include "imainframe.h"
#include "icommandsystem.h"
#include "ieventmanager.h"
#include "colourscheme/ColourSchemeEditor.h"
#include "GroupDialog.h"
#include "debugging/debugging.h"
#include "wxutil/dialog/MessageBox.h"
#include "selectionlib.h"

#include "animationpreview/MD5AnimationViewer.h"
#include "LocalBitmapArtProvider.h"

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <memory>

namespace ui
{

IDialogManager& UIManager::getDialogManager()
{
	return *_dialogManager;
}

IMenuManager& UIManager::getMenuManager() {
	return *_menuManager;
}

IToolbarManager& UIManager::getToolbarManager() {
	return *_toolbarManager;
}

IGroupDialog& UIManager::getGroupDialog() {
	return GroupDialog::Instance();
}

void UIManager::clear()
{
	_menuManager->clear();
	_dialogManager.reset();

	wxFileSystem::CleanUpHandlers();
	wxArtProvider::Delete(_bitmapArtProvider);
	_bitmapArtProvider = nullptr;
}

const std::string& UIManager::ArtIdPrefix() const
{
	return LocalBitmapArtProvider::ArtIdPrefix();
}

const std::string& UIManager::getName() const
{
	static std::string _name(MODULE_UIMANAGER);
	return _name;
}

const StringSet& UIManager::getDependencies() const
{
	static StringSet _dependencies;

	if (_dependencies.empty())
	{
		_dependencies.insert(MODULE_EVENTMANAGER);
		_dependencies.insert(MODULE_XMLREGISTRY);
		_dependencies.insert(MODULE_SELECTIONSYSTEM);
		_dependencies.insert(MODULE_COMMANDSYSTEM);
		_dependencies.insert(MODULE_COUNTER);
		_dependencies.insert(MODULE_STATUSBARMANAGER);
	}

	return _dependencies;
}

void UIManager::initialiseModule(const IApplicationContext& ctx)
{
	rMessage() << getName() << "::initialiseModule called" << std::endl;

	_bitmapArtProvider = new LocalBitmapArtProvider();
	wxArtProvider::Push(_bitmapArtProvider);

	_dialogManager = std::make_shared<DialogManager>();

    _menuManager = std::make_shared<MenuManager>();
	_menuManager->loadFromRegistry();

    _toolbarManager = std::make_shared<ToolbarManager>();
	_toolbarManager->initialise();

	GlobalCommandSystem().addCommand("AnimationPreview", MD5AnimationViewer::Show);
	GlobalCommandSystem().addCommand("EditColourScheme", ColourSchemeEditor::DisplayDialog);

	GlobalMainFrame().signal_MainFrameShuttingDown().connect(
        sigc::mem_fun(this, &UIManager::clear)
    );

	wxFileSystem::AddHandler(new wxLocalFSHandler);
	wxXmlResource::Get()->InitAllHandlers();

	std::string fullPath = ctx.getRuntimeDataPath() + "ui/";
	wxXmlResource::Get()->Load(fullPath + "*.xrc");

	_selectionChangedConn = GlobalSelectionSystem().signal_selectionChanged().connect(
		[this](const ISelectable&) { requestIdleCallback(); }
	);

	_countersChangedConn = GlobalCounters().signal_countersChanged().connect(
		[this]() { requestIdleCallback(); }
	);

	updateCounterStatusBar();
}

void UIManager::shutdownModule()
{
	_countersChangedConn.disconnect();
	_selectionChangedConn.disconnect();
	_menuManager->clear();
}

void UIManager::onIdle()
{
	updateCounterStatusBar();
}

void UIManager::updateCounterStatusBar()
{
	const SelectionInfo& info = GlobalSelectionSystem().getSelectionInfo();
	auto& counterMgr = GlobalCounters();

	std::string text =
		fmt::format(_("Brushes: {0:d} ({1:d}) Patches: {2:d} ({3:d}) Entities: {4:d} ({5:d})"),
			counterMgr.getCounter(counterBrushes).get(),
			info.brushCount,
			counterMgr.getCounter(counterPatches).get(),
			info.patchCount,
				counterMgr.getCounter(counterEntities).get(),
			info.entityCount);

	GlobalStatusBarManager().setText("MapCounters", text);
}

module::StaticModule<UIManager> uiManagerModule;

} // namespace ui
