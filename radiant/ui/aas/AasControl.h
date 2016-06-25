#pragma once

#include <wx/event.h>
#include <memory>
#include "iaasfile.h"
#include "map/RenderableAasFile.h"

class wxWindow;
class wxButton;
class wxToggleButton;
class wxSizer;

namespace ui
{

/**
 * greebo: An AasControl contains a set of widgets needed
 * to control an associated AAS file.
 *
 * Multiple of these Controls can be packed as children into the
 * owning AasControlDialog.
 */
class AasControl :
	public wxEvtHandler
{
private:
	wxToggleButton* _toggle;
	wxButton* _refreshButton;
	wxSizer* _buttonHBox;

	// Locks down the callbacks during widget update
	bool _updateActive;

    // The AAS file this control is referring to
    map::AasFileInfo _info;

    // The AAS file reference (can be empty)
    map::IAasFilePtr _aasFile;

    // The renderable that is attached to the rendersystem when active
    map::RenderableAasFile _renderable;

public:
	AasControl(wxWindow* parent, const map::AasFileInfo& info);

    virtual ~AasControl();

	// Returns the widgets for packing this object into a container/table
	wxSizer* getButtons();
	wxToggleButton* getToggle();

	// Updates the state of all widgets
	void update();

private:
    void ensureAasFileLoaded();

	void onToggle(wxCommandEvent& ev);
	void onRefresh(wxCommandEvent& ev);
};
typedef std::shared_ptr<AasControl> AasControlPtr;

} // namespace ui
