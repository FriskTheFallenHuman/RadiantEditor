#include "AuiLayout.h"

#include "i18n.h"
#include "ui/imenumanager.h"
#include "ui/igroupdialog.h"
#include "ui/imainframe.h"
#include "ui/iuserinterface.h"

#include <wx/sizer.h>

#include "camera/CameraWndManager.h"
#include "ui/texturebrowser/TextureBrowser.h"
#include "xyview/GlobalXYWnd.h"

namespace ui
{

namespace
{
    const std::string RKEY_ROOT = "user/ui/mainFrame/aui";

    // Minimum size of docked panels
    const wxSize MIN_SIZE(128, 128);

    // Return a pane info with default options
    wxAuiPaneInfo DEFAULT_PANE_INFO(const std::string& caption,
                                    const wxSize& minSize)
    {
        return wxAuiPaneInfo().Caption(caption).CloseButton(false)
                              .BestSize(minSize).MinSize(minSize);
    }
}

AuiLayout::AuiLayout()
: _auiMgr(nullptr, wxAUI_MGR_ALLOW_FLOATING | wxAUI_MGR_VENETIAN_BLINDS_HINT |
                   wxAUI_MGR_LIVE_RESIZE)
{
}

void AuiLayout::addPane(wxWindow* window, const wxAuiPaneInfo& info)
{
    // Give the pane a deterministic name so we can restore perspective
    wxAuiPaneInfo nameInfo = info;
    nameInfo.Name(std::to_string(_panes.size()));

    // Add and store the pane
    _auiMgr.AddPane(window, nameInfo);
    _panes.push_back(window);
}

std::string AuiLayout::getName()
{
    return AUI_LAYOUT_NAME;
}

void AuiLayout::activate()
{
    auto topLevelParent = GlobalMainFrame().getWxTopLevelWindow();

    // AUI manager can't manage a Sizer, we need to create an actual wxWindow
    // container
    auto managedArea = new wxWindow(topLevelParent, wxID_ANY);
    _auiMgr.SetManagedWindow(managedArea);
    GlobalMainFrame().getWxMainContainer()->Add(managedArea, 1, wxEXPAND);

    auto notebookPanel = new wxPanel(managedArea, wxID_ANY);
    notebookPanel->SetSizer(new wxBoxSizer(wxVERTICAL));

    GlobalGroupDialog().reparentNotebook(notebookPanel);

    // Hide the floating window
    GlobalGroupDialog().hideDialogWindow();

    // Add a new texture browser to the group dialog pages
    auto textureBrowserControl = GlobalUserInterface().findControl(UserControl::TextureBrowser);
    assert(textureBrowserControl);

    // Texture Page
    {
        IGroupDialog::PagePtr page(new IGroupDialog::Page);

        page->name = "textures";
        page->windowLabel = _("Texture Browser");
        page->page = textureBrowserControl->createWidget(notebookPanel);
        page->tabIcon = "icon_texture.png";
        page->tabLabel = _("Textures");
		page->position = IGroupDialog::Page::Position::TextureBrowser;

        GlobalGroupDialog().addPage(page);
    }

    auto orthoViewControl = GlobalUserInterface().findControl(UserControl::OrthoView);
    auto cameraControl = GlobalUserInterface().findControl(UserControl::Camera);
    assert(cameraControl);
    assert(orthoViewControl);

    // Add the camera and notebook to the left, as with the Embedded layout, and
    // the 2D view on the right
    wxSize size = topLevelParent->GetSize();
    size.Scale(0.5, 1.0);
    addPane(cameraControl->createWidget(managedArea),
            DEFAULT_PANE_INFO(_("Camera"), size).Left().Position(0));
    addPane(notebookPanel,
            DEFAULT_PANE_INFO(_("Properties"), size).Left().Position(1));
    addPane(orthoViewControl->createWidget(managedArea),
            DEFAULT_PANE_INFO(_("2D View"), size).CenterPane());
    _auiMgr.Update();

    // Nasty hack to get the panes sized properly. Since BestSize() is
    // completely ignored (at least on Linux), we have to add the panes with a
    // large *minimum* size and then reset this size after the initial addition.
    for (wxWindow* w: _panes)
    {
        _auiMgr.GetPane(w).MinSize(MIN_SIZE);
    }
    _auiMgr.Update();

    // If we have a stored perspective, load it
    std::string storedPersp = GlobalRegistry().get(RKEY_ROOT);
    if (!storedPersp.empty())
    {
        _auiMgr.LoadPerspective(storedPersp);
    }

    // Hide the camera toggle option for non-floating views
    GlobalMenuManager().setVisibility("main/view/cameraview", false);
    // Hide the console/texture browser toggles for non-floating/non-split views
    GlobalMenuManager().setVisibility("main/view/textureBrowser", false);

    // Restore all floating XY views
    GlobalXYWnd().restoreState();
}

void AuiLayout::deactivate()
{
    // Save all floating XYWnd states
    GlobalXYWnd().saveState();

    // Store perspective
    GlobalRegistry().set(RKEY_ROOT, _auiMgr.SavePerspective().ToStdString());

    // Show the camera toggle option again
    GlobalMenuManager().setVisibility("main/view/cameraview", true);
    GlobalMenuManager().setVisibility("main/view/textureBrowser", true);

    // Remove all previously stored pane information
    // GlobalRegistry().deleteXPath(RKEY_ROOT + "//pane");

    // Delete all active views
    GlobalXYWndManager().destroyViews();

    // Give the notebook back to the GroupDialog
    GlobalGroupDialog().reparentNotebookToSelf();

    // Hide the group dialog
    GlobalGroupDialog().hideDialogWindow();

    GlobalGroupDialog().removePage("textures"); // do this after destroyWindow()

    // Get a reference to the managed window, it might be cleared by UnInit()
    auto managedWindow = _auiMgr.GetManagedWindow();

    // Unregister the AuiMgr from the event handlers of the managed window
    // otherwise we run into crashes during shutdown (#5586)
    _auiMgr.UnInit();

    managedWindow->Destroy();
}

void AuiLayout::restoreStateFromRegistry()
{
}

void AuiLayout::toggleFullscreenCameraView()
{
}

// The creation function, needed by the mainframe layout manager
AuiLayoutPtr AuiLayout::CreateInstance()
{
    return AuiLayoutPtr(new AuiLayout);
}

} // namespace ui
