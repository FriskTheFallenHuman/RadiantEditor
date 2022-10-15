#pragma once

#include "imodule.h"
#include <functional>

class IMainFrameLayout
{
public:
    /**
     * Destructor
     */
    virtual ~IMainFrameLayout() {}

    /**
     * Each MainFrame layout has a unique name.
     */
    virtual std::string getName() = 0;

    /**
     * Use this to let the Layout construct its widgets and
     * restore its state from the Registry, if appropriate.
     */
    virtual void activate() = 0;

    /**
     * Advises this layout to destruct its widgets and remove itself
     * from the MainFrame.
     */
    virtual void deactivate() = 0;

    /**
     * greebo: Each layout should implement this command to maximise
     * the camera view and restore it again at the next call. The layout
     * is activated with a un-maximised camera view, so the first call
     * to this method will maximise it. On deactivation, the layout should
     * take care of un-maxmising the camview (if necessary) before saving
     * its state.
     */
    virtual void toggleFullscreenCameraView() = 0;

    /**
     * Loads and applies the stored state from the registry.
     */
    virtual void restoreStateFromRegistry() = 0;

    /**
     * Opens a floating window with the named control as content.
     */
    virtual void createFloatingControl(const std::string& controlName) = 0;
};
typedef std::shared_ptr<IMainFrameLayout> IMainFrameLayoutPtr;

/**
 * This represents a function to create a mainframe layout like this:
 *
 * IMainFrameLayoutPtr createInstance();
 */
typedef std::function<IMainFrameLayoutPtr()> CreateMainFrameLayoutFunc;

constexpr const char* const MODULE_MAINFRAME_LAYOUT_MANAGER("MainFrameLayoutManager");

class IMainFrameLayoutManager :
	public RegisterableModule
{
public:
	/**
	 * Retrieves a layout with the given name. Returns NULL if not found.
	 */
	virtual IMainFrameLayoutPtr getLayout(const std::string& name) = 0;

	/**
	 * Register a layout by passing a name and a function to create such a layout.
 	 */
	virtual void registerLayout(const std::string& name, const CreateMainFrameLayoutFunc& func) = 0;

	/**
	 * greebo: Registers all layout commands to the eventmanager.
	 */
	virtual void registerCommands() = 0;
};

// This is the accessor for the mainframe module
inline IMainFrameLayoutManager& GlobalMainFrameLayoutManager()
{
	static module::InstanceReference<IMainFrameLayoutManager> _reference(MODULE_MAINFRAME_LAYOUT_MANAGER);
	return _reference;
}
