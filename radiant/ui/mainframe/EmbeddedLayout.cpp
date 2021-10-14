#include "EmbeddedLayout.h"

#include "i18n.h"
#include "itextstream.h"
#include "ieventmanager.h"
#include "imenumanager.h"
#include "igroupdialog.h"
#include "imainframe.h"
#include "ui/ientityinspector.h"

#include <wx/sizer.h>
#include <wx/splitter.h>
#include <functional>

#include "camera/CameraWndManager.h"
#include "ui/texturebrowser/TextureBrowser.h"
#include "xyview/GlobalXYWnd.h"

namespace ui
{

    namespace
    {
        const std::string RKEY_EMBEDDED_ROOT = "user/ui/mainFrame/embedded";
        const std::string RKEY_HORIZ_POS = RKEY_EMBEDDED_ROOT + "/mainSplitterPos";
        const std::string RKEY_VERT_POS = RKEY_EMBEDDED_ROOT + "/groupCamSplitterPos";
    }

std::string EmbeddedLayout::getName()
{
    return EMBEDDED_LAYOUT_NAME;
}

void EmbeddedLayout::activate()
{
    wxFrame* topLevelParent = GlobalMainFrame().getWxTopLevelWindow();

    // Splitters
    _horizPane = new wxutil::Splitter(
        topLevelParent, RKEY_HORIZ_POS, wxSP_LIVE_UPDATE | wxSP_3D | wxWANTS_CHARS,
        "EmbeddedHorizPane"
    );

    GlobalMainFrame().getWxMainContainer()->Add(_horizPane, 1, wxEXPAND);

    // Allocate a new OrthoView and set its ViewType to XY
    XYWndPtr xywnd = GlobalXYWnd().createEmbeddedOrthoView(XY, _horizPane);

    // CamGroup Pane
    _groupCamPane = new wxutil::Splitter(
        _horizPane, RKEY_VERT_POS, wxSP_LIVE_UPDATE | wxSP_3D | wxWANTS_CHARS,
        "EmbeddedVertPane"
    );

    // Create a new camera window and parent it
    _camWnd = GlobalCamera().createCamWnd(_groupCamPane);

    wxPanel* notebookPanel = new wxPanel(_groupCamPane, wxID_ANY);
    notebookPanel->SetSizer(new wxBoxSizer(wxVERTICAL));

    GlobalGroupDialog().reparentNotebook(notebookPanel);

    // Hide the floating window
    GlobalGroupDialog().hideDialogWindow();

    // Add a new texture browser to the group dialog pages
    wxWindow* textureBrowser = new TextureBrowser(notebookPanel);

    // Texture Page
    {
        IGroupDialog::PagePtr page(new IGroupDialog::Page);

        page->name = "textures";
        page->windowLabel = _("Texture Browser");
        page->page = textureBrowser;
        page->tabIcon = "icon_texture.png";
        page->tabLabel = _("Textures");
		page->position = IGroupDialog::Page::Position::TextureBrowser;

        GlobalGroupDialog().addPage(page);
    }

    // Add the camGroup pane to the left and the GL widget to the right
    _groupCamPane->SplitHorizontally(_camWnd->getMainWidget(), notebookPanel);
    _horizPane->SplitVertically(_groupCamPane, xywnd->getGLWidget());

    topLevelParent->Layout();

    // Enable sash position persistence
    _horizPane->connectToRegistry();
    _groupCamPane->connectToRegistry();

    // Hide the camera toggle option for non-floating views
    GlobalMenuManager().setVisibility("main/view/cameraview", false);
    // Hide the console/texture browser toggles for non-floating/non-split views
    GlobalMenuManager().setVisibility("main/view/textureBrowser", false);
}

void EmbeddedLayout::deactivate()
{
    // Show the camera toggle option again
    GlobalMenuManager().setVisibility("main/view/cameraview", true);
    GlobalMenuManager().setVisibility("main/view/textureBrowser", true);

    // Remove all previously stored pane information
    GlobalRegistry().deleteXPath(RKEY_EMBEDDED_ROOT + "//pane");

    // Delete all active views
    GlobalXYWndManager().destroyViews();

    // Delete the CamWnd
    _camWnd.reset();

    // Give the notebook back to the GroupDialog
    GlobalGroupDialog().reparentNotebookToSelf();

    // Hide the group dialog
    GlobalGroupDialog().hideDialogWindow();

    GlobalGroupDialog().removePage("textures"); // do this after destroyWindow()

	delete _horizPane;

    // Those two have been deleted by the above, so NULL the references
    _horizPane = nullptr;
    _groupCamPane = nullptr;

    _savedPositions.reset();
}

void EmbeddedLayout::restoreStateFromRegistry()
{
}

void EmbeddedLayout::toggleFullscreenCameraView()
{
    if (_savedPositions)
    {
        _horizPane->SetSashPosition(_savedPositions->horizSashPosition);
        _groupCamPane->SetSashPosition(_savedPositions->groupCamSashPosition);

        _savedPositions.reset();
    }
    else
    {
        _savedPositions.reset(new SavedPositions{
            _horizPane->GetSashPosition(), 
            _groupCamPane->GetSashPosition()
        });

        // Maximise the camera, wxWidgets will clip the coordinates
        _horizPane->SetSashPosition(2000000);
        _groupCamPane->SetSashPosition(2000000);
    }
}

// The creation function, needed by the mainframe layout manager
EmbeddedLayoutPtr EmbeddedLayout::CreateInstance()
{
    return EmbeddedLayoutPtr(new EmbeddedLayout);
}

} // namespace ui
