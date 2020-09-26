#include "CamWnd.h"

#include "igl.h"
#include "ibrush.h"
#include "iclipper.h"
#include "icolourscheme.h"
#include "ieventmanager.h"
#include "imainframe.h"
#include "itextstream.h"
#include "iorthoview.h"
#include "icamera.h"

#include <time.h>
#include <fmt/format.h>

#include "util/ScopedBoolLock.h"
#include "iselectiontest.h"
#include "selectionlib.h"
#include "gamelib.h"
#include "CamRenderer.h"
#include "CameraSettings.h"
#include "GlobalCameraWndManager.h"
#include "render/RenderStatistics.h"
#include "render/RenderableCollectionWalker.h"
#include "wxutil/MouseButton.h"
#include "registry/adaptors.h"
#include "selection/OccludeSelector.h"
#include "selection/Device.h"
#include "selection/SelectionTest.h"
#include "FloorHeightWalker.h"
#include "ObjectFinder.h"

#include "debugging/debugging.h"
#include "debugging/gl.h"
#include <wx/sizer.h>
#include "util/ScopedBoolLock.h"
#include <functional>
#include <sigc++/retype_return.h>

namespace ui
{

namespace
{
    const std::size_t MSEC_PER_FRAME = 16;

    const char* const FAR_CLIP_IN_TEXT = N_("Move far clip plane closer");
    const char* const FAR_CLIP_OUT_TEXT = N_("Move far clip plane further away");
    const char* const FAR_CLIP_DISABLED_TEXT = N_(" (currently disabled in preferences)");

    const unsigned int MOVE_NONE = 0;
    const unsigned int MOVE_FORWARD = 1 << 0;
    const unsigned int MOVE_BACK = 1 << 1;
    const unsigned int MOVE_ROTRIGHT = 1 << 2;
    const unsigned int MOVE_ROTLEFT = 1 << 3;
    const unsigned int MOVE_STRAFERIGHT = 1 << 4;
    const unsigned int MOVE_STRAFELEFT = 1 << 5;
    const unsigned int MOVE_UP = 1 << 6;
    const unsigned int MOVE_DOWN = 1 << 7;
    const unsigned int MOVE_PITCHUP = 1 << 8;
    const unsigned int MOVE_PITCHDOWN = 1 << 9;
    const unsigned int MOVE_ALL = MOVE_FORWARD | MOVE_BACK | MOVE_ROTRIGHT | MOVE_ROTLEFT | MOVE_STRAFERIGHT | MOVE_STRAFELEFT | MOVE_UP | MOVE_DOWN | MOVE_PITCHUP | MOVE_PITCHDOWN;
}

inline Vector2 windowvector_for_widget_centre(wxutil::GLWidget& widget)
{
    wxSize size = widget.GetSize();
    return Vector2(static_cast<Vector2::ElementType>(size.GetWidth() / 2), static_cast<Vector2::ElementType>(size.GetHeight() / 2));
}

// ---------- CamWnd Implementation --------------------------------------------------

CamWnd::CamWnd(wxWindow* parent) :
    MouseToolHandler(IMouseToolGroup::Type::CameraView),
    _mainWxWidget(loadNamedPanel(parent, "CamWndPanel")),
    _id(++_maxId),
    _view(true),
    _camera(_view, std::bind(&CamWnd::queueDraw, this), std::bind(&CamWnd::forceRedraw, this)),
    _drawing(false),
    _wxGLWidget(new wxutil::GLWidget(_mainWxWidget, std::bind(&CamWnd::onRender, this), "CamWnd")),
    _timer(this),
    _timerLock(false),
    _freeMoveEnabled(false),
    _freeMoveFlags(0),
    _freeMoveTimer(this),
    _deferredMotionDelta(std::bind(&CamWnd::onDeferredMotionDelta, this, std::placeholders::_1, std::placeholders::_2)),
    _strafe(false),
    _strafeForward(false)
{
    Bind(wxEVT_TIMER, &CamWnd::onFrame, this, _timer.GetId());
    Bind(wxEVT_TIMER, &CamWnd::onFreeMoveTimer, this, _freeMoveTimer.GetId());

    constructGUIComponents();

    // Connect the mouse button events
    _wxGLWidget->Bind(wxEVT_LEFT_DOWN, &CamWnd::onGLMouseButtonPress, this);
    _wxGLWidget->Bind(wxEVT_LEFT_DCLICK, &CamWnd::onGLMouseButtonPress, this);
    _wxGLWidget->Bind(wxEVT_LEFT_UP, &CamWnd::onGLMouseButtonRelease, this);
    _wxGLWidget->Bind(wxEVT_RIGHT_DOWN, &CamWnd::onGLMouseButtonPress, this);
    _wxGLWidget->Bind(wxEVT_RIGHT_DCLICK, &CamWnd::onGLMouseButtonPress, this);
    _wxGLWidget->Bind(wxEVT_RIGHT_UP, &CamWnd::onGLMouseButtonRelease, this);
    _wxGLWidget->Bind(wxEVT_MIDDLE_DOWN, &CamWnd::onGLMouseButtonPress, this);
    _wxGLWidget->Bind(wxEVT_MIDDLE_DCLICK, &CamWnd::onGLMouseButtonPress, this);
    _wxGLWidget->Bind(wxEVT_MIDDLE_UP, &CamWnd::onGLMouseButtonRelease, this);
	_wxGLWidget->Bind(wxEVT_AUX1_DOWN, &CamWnd::onGLMouseButtonPress, this);
	_wxGLWidget->Bind(wxEVT_AUX1_DCLICK, &CamWnd::onGLMouseButtonPress, this);
	_wxGLWidget->Bind(wxEVT_AUX1_UP, &CamWnd::onGLMouseButtonRelease, this);
	_wxGLWidget->Bind(wxEVT_AUX2_DOWN, &CamWnd::onGLMouseButtonPress, this);
	_wxGLWidget->Bind(wxEVT_AUX2_DCLICK, &CamWnd::onGLMouseButtonPress, this);
	_wxGLWidget->Bind(wxEVT_AUX2_UP, &CamWnd::onGLMouseButtonRelease, this);

    // Now add the handlers for the non-freelook mode, the events are activated by this
    addHandlersMove();

    // Clicks are eaten when the FreezePointer is active, request to receive them
    _freezePointer.connectMouseEvents(
        std::bind(&CamWnd::onGLMouseButtonPress, this, std::placeholders::_1),
        std::bind(&CamWnd::onGLMouseButtonRelease, this, std::placeholders::_1));

    // Subscribe to the global scene graph update
    GlobalSceneGraph().addSceneObserver(this);

    _glExtensionsInitialisedNotifier = GlobalRenderSystem().signal_extensionsInitialised().connect(
        sigc::mem_fun(this, &CamWnd::onGLExtensionsInitialised));
}

wxWindow* CamWnd::getMainWidget() const
{ 
    return _mainWxWidget;
}

void CamWnd::constructToolbar()
{
    // If lighting is not available, grey out the lighting button
    wxToolBar* camToolbar = findNamedObject<wxToolBar>(_mainWxWidget, "CamToolbar");

    const wxToolBarToolBase* wireframeBtn = getToolBarToolByLabel(camToolbar, "wireframeBtn");
    const wxToolBarToolBase* flatShadeBtn = getToolBarToolByLabel(camToolbar, "flatShadeBtn");
    const wxToolBarToolBase* texturedBtn = getToolBarToolByLabel(camToolbar, "texturedBtn");
    const wxToolBarToolBase* lightingBtn = getToolBarToolByLabel(camToolbar, "lightingBtn");

    if (!GlobalRenderSystem().shaderProgramsAvailable())
    {
        //lightingBtn->set_sensitive(false);
        camToolbar->EnableTool(lightingBtn->GetId(), false);
    }

    // Listen for render-mode changes, and set the correct active button to
    // start with.
    getCameraSettings()->signalRenderModeChanged().connect(
        sigc::mem_fun(this, &CamWnd::updateActiveRenderModeButton)
    );
    updateActiveRenderModeButton();

    // Connect button signals
    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onRenderModeButtonsChanged, this, wireframeBtn->GetId());
    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED,&CamWnd::onRenderModeButtonsChanged, this, flatShadeBtn->GetId());
    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onRenderModeButtonsChanged, this, texturedBtn->GetId());
    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onRenderModeButtonsChanged, this, lightingBtn->GetId());

    // Far clip buttons.
    wxToolBar* miscToolbar = static_cast<wxToolBar*>(_mainWxWidget->FindWindow("MiscToolbar"));

    const wxToolBarToolBase* clipPlaneInButton = getToolBarToolByLabel(miscToolbar, "clipPlaneInButton");
    const wxToolBarToolBase* clipPlaneOutButton = getToolBarToolByLabel(miscToolbar, "clipPlaneOutButton");

    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onFarClipPlaneInClick, this, clipPlaneInButton->GetId());
    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onFarClipPlaneOutClick, this, clipPlaneOutButton->GetId());

    setFarClipButtonSensitivity();

    GlobalRegistry().signalForKey(RKEY_ENABLE_FARCLIP).connect(
        sigc::mem_fun(*this, &CamWnd::setFarClipButtonSensitivity)
    );

    const wxToolBarToolBase* startTimeButton = getToolBarToolByLabel(miscToolbar, "startTimeButton");
    const wxToolBarToolBase* stopTimeButton = getToolBarToolByLabel(miscToolbar, "stopTimeButton");

    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onStartTimeButtonClick, this, startTimeButton->GetId());
    _mainWxWidget->GetParent()->Bind(wxEVT_COMMAND_TOOL_CLICKED, &CamWnd::onStopTimeButtonClick, this, stopTimeButton->GetId());

    // Stop time, initially
    stopRenderTime();

    // Handle hiding or showing the toolbar (Preferences/Settings/Camera page)
    updateToolbarVisibility();
    GlobalRegistry().signalForKey(RKEY_SHOW_CAMERA_TOOLBAR).connect(
        sigc::mem_fun(this, &CamWnd::updateToolbarVisibility)
    );
}

void CamWnd::updateToolbarVisibility()
{
    bool visible = getCameraSettings()->showCameraToolbar();

    _mainWxWidget->FindWindow("CamToolbar")->Show(visible);
    _mainWxWidget->FindWindow("MiscToolbar")->Show(visible);

    // WxWidgets/GTK doesn't seem to automatically refresh the layout when we
    // hide/show the toolbars
    _mainWxWidget->GetSizer()->Layout();
}

void CamWnd::onGLExtensionsInitialised()
{
    // If lighting is not available, grey out the lighting button
    wxToolBar* camToolbar = findNamedObject<wxToolBar>(_mainWxWidget, "CamToolbar");
    const wxToolBarToolBase* lightingBtn = getToolBarToolByLabel(camToolbar, "lightingBtn");

    camToolbar->EnableTool(lightingBtn->GetId(), GlobalRenderSystem().shaderProgramsAvailable());
}

void CamWnd::setFarClipButtonSensitivity()
{
    // Only enabled if cubic clipping is enabled.
    bool enabled = registry::getValue<bool>(RKEY_ENABLE_FARCLIP, true);

    wxToolBar* miscToolbar = static_cast<wxToolBar*>(_mainWxWidget->FindWindow("MiscToolbar"));

    wxToolBarToolBase* clipPlaneInButton = 
        const_cast<wxToolBarToolBase*>(getToolBarToolByLabel(miscToolbar, "clipPlaneInButton"));
    wxToolBarToolBase* clipPlaneOutButton = 
        const_cast<wxToolBarToolBase*>(getToolBarToolByLabel(miscToolbar, "clipPlaneOutButton"));

    miscToolbar->EnableTool(clipPlaneInButton->GetId(), enabled);
    miscToolbar->EnableTool(clipPlaneOutButton->GetId(), enabled);

    // Update tooltips so users know why they are disabled
    clipPlaneInButton->SetShortHelp(fmt::format("{0}{1}", _(FAR_CLIP_IN_TEXT), (enabled ? "" : _(FAR_CLIP_DISABLED_TEXT))));
    clipPlaneOutButton->SetShortHelp(fmt::format("{0}{1}", _(FAR_CLIP_OUT_TEXT), (enabled ? "" : _(FAR_CLIP_DISABLED_TEXT))));
}

void CamWnd::constructGUIComponents()
{
    constructToolbar();

    // Set up wxGL widget
    _wxGLWidget->SetCanFocus(false);
    _wxGLWidget->SetMinClientSize(wxSize(CAMWND_MINSIZE_X, CAMWND_MINSIZE_Y));
    _wxGLWidget->Bind(wxEVT_SIZE, &CamWnd::onGLResize, this);
    _wxGLWidget->Bind(wxEVT_MOUSEWHEEL, &CamWnd::onMouseScroll, this);

    _mainWxWidget->GetSizer()->Add(_wxGLWidget, 1, wxEXPAND); 
}

CamWnd::~CamWnd()
{
    // Stop the timer, it might still fire even during shutdown
    _timer.Stop();

    // Unsubscribe from the global scene graph update
    GlobalSceneGraph().removeSceneObserver(this);

    if (_freeMoveEnabled)
    {
        disableFreeMove();
    }

    removeHandlersMove();

    // Notify the camera manager about our destruction
    GlobalCamera().removeCamWnd(_id);
}

SelectionTestPtr CamWnd::createSelectionTestForPoint(const Vector2& point)
{
    return _camera.createSelectionTestForPoint(point);
}

const VolumeTest& CamWnd::getVolumeTest() const
{
    return _view;
}

int CamWnd::getDeviceWidth() const
{
    return _camera.getDeviceWidth();
}

int CamWnd::getDeviceHeight() const
{
    return _camera.getDeviceHeight();
}

void CamWnd::startRenderTime()
{
    if (_timer.IsRunning())
    {
        // Timer is already running, just reset the preview time
        GlobalRenderSystem().setTime(0);
    }
    else
    {
        // Timer is not enabled, we're paused or stopped
        _timer.Start(MSEC_PER_FRAME);
        _timerLock = false; // reset the lock, just in case
    }

    wxToolBar* miscToolbar = static_cast<wxToolBar*>(_mainWxWidget->FindWindow("MiscToolbar"));

    const wxToolBarToolBase* stopTimeButton = getToolBarToolByLabel(miscToolbar, "stopTimeButton");
    miscToolbar->EnableTool(stopTimeButton->GetId(), true);
}

void CamWnd::onStartTimeButtonClick(wxCommandEvent& ev)
{
    startRenderTime();
}

void CamWnd::onStopTimeButtonClick(wxCommandEvent& ev)
{
    stopRenderTime();
}

void CamWnd::onFrame(wxTimerEvent& ev)
{
    // Calling wxTheApp->Yield() might cause another timer callback if enough 
    // time has passed during rendering. Calling Yield() within Yield() 
    // might in the end cause stack overflows and is caught by wxWidgets.
    if (!_timerLock)
    {
        util::ScopedBoolLock lock(_timerLock);

        GlobalRenderSystem().setTime(GlobalRenderSystem().getTime() + _timer.GetInterval());

        // Mouse movement is handled via idle callbacks, so let's give the app a chance to react
        wxTheApp->ProcessIdle();

        _wxGLWidget->Refresh();
    }
}

void CamWnd::stopRenderTime()
{
    _timer.Stop();

    wxToolBar* miscToolbar = static_cast<wxToolBar*>(_mainWxWidget->FindWindow("MiscToolbar"));

    const wxToolBarToolBase* startTimeButton = getToolBarToolByLabel(miscToolbar, "startTimeButton");
    const wxToolBarToolBase* stopTimeButton = getToolBarToolByLabel(miscToolbar, "stopTimeButton");

    miscToolbar->EnableTool(startTimeButton->GetId(), true);
    miscToolbar->EnableTool(stopTimeButton->GetId(), false);
}

void CamWnd::onRenderModeButtonsChanged(wxCommandEvent& ev)
{
    if (ev.GetInt() == 0) // un-toggled
    {
        return; // Don't react on UnToggle events
    }

    wxToolBar* camToolbar = static_cast<wxToolBar*>(_mainWxWidget->FindWindow("CamToolbar"));

    // This function will be called twice, once for the inactivating button and
    // once for the activating button
    if (getToolBarToolByLabel(camToolbar, "texturedBtn")->GetId() == ev.GetId())
    {
        getCameraSettings()->setRenderMode(RENDER_MODE_TEXTURED);
    }
    else if (getToolBarToolByLabel(camToolbar, "wireframeBtn")->GetId() == ev.GetId())
    {
        getCameraSettings()->setRenderMode(RENDER_MODE_WIREFRAME);
    }
    else if (getToolBarToolByLabel(camToolbar, "flatShadeBtn")->GetId() == ev.GetId())
    {
        getCameraSettings()->setRenderMode(RENDER_MODE_SOLID);
    }
    else if (getToolBarToolByLabel(camToolbar, "lightingBtn")->GetId() == ev.GetId())
    {
        getCameraSettings()->setRenderMode(RENDER_MODE_LIGHTING);
    }
}

void CamWnd::updateActiveRenderModeButton()
{
    wxToolBar* camToolbar = static_cast<wxToolBar*>(_mainWxWidget->FindWindow("CamToolbar"));

    switch (getCameraSettings()->getRenderMode())
    {
    case RENDER_MODE_WIREFRAME:
        camToolbar->ToggleTool(getToolBarToolByLabel(camToolbar, "wireframeBtn")->GetId(), true);
        break;
    case RENDER_MODE_SOLID:
        camToolbar->ToggleTool(getToolBarToolByLabel(camToolbar, "flatShadeBtn")->GetId(), true);
        break;
    case RENDER_MODE_TEXTURED:
        camToolbar->ToggleTool(getToolBarToolByLabel(camToolbar, "texturedBtn")->GetId(), true);
        break;
    case RENDER_MODE_LIGHTING:
        camToolbar->ToggleTool(getToolBarToolByLabel(camToolbar, "lightingBtn")->GetId(), true);
        break;
    default:
        assert(false);
    }
}

int CamWnd::getId()
{
    return _id;
}

void CamWnd::jumpToObject(SelectionTest& selectionTest)
{
    // Find a suitable target node
    camera::ObjectFinder finder(selectionTest);
    GlobalSceneGraph().root()->traverse(finder);

    if (finder.getNode())
    {
        // A node has been found, get the bounding box
        AABB found = finder.getNode()->worldAABB();

        // Focus the view at the center of the found AABB
        // Set the camera and the views to the given point
        GlobalCameraManager().focusCamera(found.origin, _camera.getCameraAngles());
        GlobalXYWndManager().setOrigin(found.origin);
    }
}

void CamWnd::changeFloor(const bool up)
{
    float current = _camera.getCameraOrigin()[2] - 48;
    float bestUp;
    float bestDown;
    camera::FloorHeightWalker walker(current, bestUp, bestDown);
    GlobalSceneGraph().root()->traverse(walker);

    if (up && bestUp != game::current::getValue<float>("/defaults/maxWorldCoord")) {
        current = bestUp;
    }

    if (!up && bestDown != -game::current::getValue<float>("/defaults/maxWorldCoord")) {
        current = bestDown;
    }

    const Vector3& org = _camera.getCameraOrigin();
    _camera.setCameraOrigin(Vector3(org[0], org[1], current + 48));

    _camera.updateModelview();
    update();
    GlobalCamera().movedNotify();
}

void CamWnd::setFreeMoveFlags(unsigned int mask)
{
    if ((~_freeMoveFlags & mask) != 0 && _freeMoveFlags == 0)
    {
        _freeMoveTimer.Start(10);
    }

    _freeMoveFlags |= mask;
}

void CamWnd::clearFreeMoveFlags(unsigned int mask)
{
    if ((_freeMoveFlags & ~mask) == 0 && _freeMoveFlags != 0)
    {
        _freeMoveTimer.Stop();
    }

    _freeMoveFlags &= ~mask;
}

void CamWnd::handleFreeMovement(float timePassed)
{
    int angleSpeed = getCameraSettings()->angleSpeed();
    int movementSpeed = getCameraSettings()->movementSpeed();

    auto angles = _camera.getCameraAngles();

    // Update angles
    if (_freeMoveFlags & MOVE_ROTLEFT)
        angles[camera::CAMERA_YAW] += 15 * timePassed * angleSpeed;

    if (_freeMoveFlags & MOVE_ROTRIGHT)
        angles[camera::CAMERA_YAW] -= 15 * timePassed * angleSpeed;

    if (_freeMoveFlags & MOVE_PITCHUP)
    {
        angles[camera::CAMERA_PITCH] += 15 * timePassed * angleSpeed;

        if (angles[camera::CAMERA_PITCH] > 90)
            angles[camera::CAMERA_PITCH] = 90;
    }

    if (_freeMoveFlags & MOVE_PITCHDOWN)
    {
        angles[camera::CAMERA_PITCH] -= 15 * timePassed * angleSpeed;

        if (angles[camera::CAMERA_PITCH] < -90)
            angles[camera::CAMERA_PITCH] = -90;
    }

    _camera.setCameraAngles(angles);

    // Update position
    auto origin = _camera.getCameraOrigin();

    if (_freeMoveFlags & MOVE_FORWARD)
        origin += -_camera.getForwardVector() * (timePassed * movementSpeed);
    if (_freeMoveFlags & MOVE_BACK)
        origin += -_camera.getForwardVector() * (-timePassed * movementSpeed);
    if (_freeMoveFlags & MOVE_STRAFELEFT)
        origin += _camera.getRightVector() * (-timePassed * movementSpeed);
    if (_freeMoveFlags & MOVE_STRAFERIGHT)
        origin += _camera.getRightVector() * (timePassed * movementSpeed);
    if (_freeMoveFlags & MOVE_UP)
        origin += g_vector3_axis_z * (timePassed * movementSpeed);
    if (_freeMoveFlags & MOVE_DOWN)
        origin += g_vector3_axis_z * (-timePassed * movementSpeed);
    
    _camera.setCameraOrigin(origin);
}

void CamWnd::onFreeMoveTimer(wxTimerEvent& ev)
{
    _deferredMotionDelta.flush();

    float time_seconds = _keyControlTimer.Time() / static_cast<float>(1000);

    _keyControlTimer.Start();

    if (time_seconds > 0.05f)
    {
        time_seconds = 0.05f; // 20fps
    }

    handleFreeMovement(time_seconds * 5.0f);

    queueDraw();
    GlobalCamera().movedNotify();
}

void CamWnd::enableFreeMove()
{
    ASSERT_MESSAGE(!_freeMoveEnabled, "EnableFreeMove: free-move was already enabled");
    _freeMoveEnabled = true;
    clearFreeMoveFlags(MOVE_ALL);

    removeHandlersMove();

    update();
}

void CamWnd::disableFreeMove()
{
    ASSERT_MESSAGE(_freeMoveEnabled, "DisableFreeMove: free-move was not enabled");
    _freeMoveEnabled = false;
    clearFreeMoveFlags(MOVE_ALL);

    addHandlersMove();

    update();
}

bool CamWnd::freeMoveEnabled() const
{
    return _freeMoveEnabled;
}

void CamWnd::performFreeMove(int dx, int dy)
{
    int angleSpeed = getCameraSettings()->angleSpeed();

    auto origin = _camera.getCameraOrigin();
    auto angles = _camera.getCameraAngles();

    // free strafe mode, toggled by the keyboard modifiers
    if (_strafe)
    {
        const float strafespeed = GlobalCamera().getCameraStrafeSpeed();
        const float forwardStrafeFactor = GlobalCamera().getCameraForwardStrafeFactor();

        origin -= _camera.getRightVector() * strafespeed * dx;

        if (_strafeForward)
        {
            origin += _camera.getForwardVector() * strafespeed * dy * forwardStrafeFactor;
        }
        else {
            origin += _camera.getForwardVector() * strafespeed * dy;
        }
    }
    else // free rotation
    {
        const float dtime = 0.1f;
        const float zAxisFactor = getCameraSettings()->invertMouseVerticalAxis() ? -1.0f : 1.0f;

        angles[camera::CAMERA_PITCH] += dy * dtime * angleSpeed * zAxisFactor;
        angles[camera::CAMERA_YAW] += dx * dtime * angleSpeed;

        if (angles[camera::CAMERA_PITCH] > 90)
            angles[camera::CAMERA_PITCH] = 90;
        else if (angles[camera::CAMERA_PITCH] < -90)
            angles[camera::CAMERA_PITCH] = -90;

        if (angles[camera::CAMERA_YAW] >= 360)
            angles[camera::CAMERA_YAW] -= 360;
        else if (angles[camera::CAMERA_YAW] <= 0)
            angles[camera::CAMERA_YAW] += 360;
    }

    _camera.setCameraOrigin(origin);
    _camera.setCameraAngles(angles);
}

void CamWnd::onDeferredMotionDelta(int x, int y)
{
    performFreeMove(-x, -y);
    queueDraw();
    GlobalCamera().movedNotify();
}

void CamWnd::Cam_Draw()
{
    wxSize glSize = _wxGLWidget->GetSize();

    if (_camera.getDeviceWidth() != glSize.GetWidth() || _camera.getDeviceHeight() != glSize.GetHeight())
    {
        _camera.setDeviceWidth(glSize.GetWidth());
        _camera.setDeviceHeight(glSize.GetHeight());
        _camera.updateProjection();
    }

    int height = _camera.getDeviceHeight();
    int width = _camera.getDeviceWidth();

    if (width == 0 || height == 0)
    {
        return; // otherwise we'll receive OpenGL errors in ortho rendering below
    }

    glViewport(0, 0, width, height);

    // enable depth buffer writes
    glDepthMask(GL_TRUE);

    Vector3 clearColour(0, 0, 0);

    if (getCameraSettings()->getRenderMode() != RENDER_MODE_LIGHTING) 
    {
        clearColour = GlobalColourSchemeManager().getColour("camera_background");
    }

    glClearColor(clearColour[0], clearColour[1], clearColour[2], 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render::RenderStatistics::Instance().resetStats();

    _view.resetCullStats();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(_camera.getProjection());

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(_camera.getModelView());

    // one directional light source directly behind the viewer
    {
        GLfloat inverse_cam_dir[4], ambient[4], diffuse[4];//, material[4];

        ambient[0] = ambient[1] = ambient[2] = 0.4f;
        ambient[3] = 1.0f;
        diffuse[0] = diffuse[1] = diffuse[2] = 0.4f;
        diffuse[3] = 1.0f;
        //material[0] = material[1] = material[2] = 0.8f;
        //material[3] = 1.0f;

        const auto& forward = _camera.getForwardVector();
        inverse_cam_dir[0] = forward[0];
        inverse_cam_dir[1] = forward[1];
        inverse_cam_dir[2] = forward[2];
        inverse_cam_dir[3] = 0;

        glLightfv(GL_LIGHT0, GL_POSITION, inverse_cam_dir);

        glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

        glEnable(GL_LIGHT0);
    }


    // Set the allowed render flags for this view
    unsigned int allowedRenderFlags = RENDER_DEPTHTEST
                                     | RENDER_MASKCOLOUR
                                     | RENDER_DEPTHWRITE
                                     | RENDER_ALPHATEST
                                     | RENDER_BLEND
                                     | RENDER_CULLFACE
                                     | RENDER_OFFSETLINE
                                     | RENDER_VERTEX_COLOUR
                                     | RENDER_POINT_COLOUR;

    // Add mode-specific render flags
    switch (getCameraSettings()->getRenderMode())
    {
        case RENDER_MODE_WIREFRAME:
            break;

        case RENDER_MODE_SOLID:
            allowedRenderFlags |= RENDER_FILL
                                | RENDER_LIGHTING
                                | RENDER_SMOOTH
                                | RENDER_SCALED;

            break;

        case RENDER_MODE_TEXTURED:
            allowedRenderFlags |= RENDER_FILL
                                | RENDER_LIGHTING
                                | RENDER_TEXTURE_2D
                                | RENDER_SMOOTH
                                | RENDER_SCALED;

            break;

        case RENDER_MODE_LIGHTING:
            allowedRenderFlags |= RENDER_FILL
                                | RENDER_LIGHTING
                                | RENDER_TEXTURE_2D
                                | RENDER_TEXTURE_CUBEMAP
                                | RENDER_VERTEX_COLOUR
                                | RENDER_SMOOTH
                                | RENDER_SCALED
                                | RENDER_BUMP
                                | RENDER_PROGRAM;

            break;

        default:
            allowedRenderFlags = 0;

            break;
    }

    if (!getCameraSettings()->solidSelectionBoxes())
    {
        allowedRenderFlags |= RENDER_LINESTIPPLE
                            | RENDER_POLYGONSTIPPLE;
    }

    {
        CamRenderer renderer(allowedRenderFlags, _primitiveHighlightShader,
                             _faceHighlightShader, _view.getViewer());

        render::RenderableCollectionWalker::CollectRenderablesInScene(renderer, _view);

        // Render any active mousetools
        for (const ActiveMouseTools::value_type& i : _activeMouseTools)
        {
            i.second->render(GlobalRenderSystem(), renderer, _view);
        }

        renderer.render(_camera.getModelView(), _camera.getProjection());
    }

    // greebo: Draw the clipper's points (skipping the depth-test)
    {
        glDisable(GL_DEPTH_TEST);

        glColor4f(1, 1, 1, 1);
        glPointSize(5);

        if (GlobalClipper().clipMode()) {
            GlobalClipper().draw(1.0f);
        }

        glPointSize(1);
    }

    // prepare for 2d stuff
    glColor4f(1, 1, 1, 1);

    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, (float)width, 0, (float)height, -100, 100);

    glScalef(1, -1, 1);
    glTranslatef(0, -(float)height, 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (GLEW_VERSION_1_3)
    {
        glClientActiveTexture(GL_TEXTURE0);
        glActiveTexture(GL_TEXTURE0);
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_DEPTH_TEST);
    glColor3f( 1.f, 1.f, 1.f );
    glLineWidth(1);

    // draw the crosshair in free move mode
    if (_freeMoveEnabled)
    {
        glBegin( GL_LINES );
        glVertex2f( (float)width / 2.f, (float)height / 2.f + 6 );
        glVertex2f( (float)width / 2.f, (float)height / 2.f + 2 );
        glVertex2f( (float)width / 2.f, (float)height / 2.f - 6 );
        glVertex2f( (float)width / 2.f, (float)height / 2.f - 2 );
        glVertex2f( (float)width / 2.f + 6, (float)height / 2.f );
        glVertex2f( (float)width / 2.f + 2, (float)height / 2.f );
        glVertex2f( (float)width / 2.f - 6, (float)height / 2.f );
        glVertex2f( (float)width / 2.f - 2, (float)height / 2.f );
        glEnd();
    }

    glRasterPos3f(1.0f, static_cast<float>(height) - 1.0f, 0.0f);

    GlobalOpenGL().drawString(render::RenderStatistics::Instance().getStatString());

    glRasterPos3f(1.0f, static_cast<float>(height) - 11.0f, 0.0f);

    GlobalOpenGL().drawString(_view.getCullStats());

    drawTime();

    if (!_activeMouseTools.empty())
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, (float)width, 0, (float)height, -100, 100);

        for (const ActiveMouseTools::value_type& i : _activeMouseTools)
        {
            i.second->renderOverlay();
        }
    }

    // bind back to the default texture so that we don't have problems
    // elsewhere using/modifying texture maps between contexts
    glBindTexture( GL_TEXTURE_2D, 0 );
}

bool CamWnd::onRender()
{
    if (_drawing) return false;

    util::ScopedBoolLock lock(_drawing);

    if (GlobalMainFrame().screenUpdatesEnabled())
    {
        debug::assertNoGlErrors();

        Cam_Draw();

        debug::assertNoGlErrors();

        return true;
    }

    return false;
}

void CamWnd::benchmark()
{
    double dStart = clock() / 1000.0;

    for (int i=0 ; i < 100 ; i++)
    {
        Vector3 angles;
        angles[camera::CAMERA_ROLL] = 0;
        angles[camera::CAMERA_PITCH] = 0;
        angles[camera::CAMERA_YAW] = static_cast<double>(i * (360.0 / 100.0));
        _camera.setCameraAngles(angles);
    }

    double dEnd = clock() / 1000.0;

    rMessage() << fmt::format("{0:5.2lf}", (dEnd - dStart)) << " seconds\n";
}

void CamWnd::onSceneGraphChange()
{
    // Just pass the call to the update method
    update();
}

// ----------------------------------------------------------

void CamWnd::addHandlersMove()
{
    _wxGLWidget->Bind(wxEVT_MOTION, &CamWnd::onGLMouseMove, this);
}

void CamWnd::removeHandlersMove()
{
    _wxGLWidget->Unbind(wxEVT_MOTION, &CamWnd::onGLMouseMove, this);
}

void CamWnd::update()
{
    queueDraw();
}

Camera& CamWnd::getCamera()
{
    return _camera;
}

void CamWnd::captureStates()
{
    _faceHighlightShader = GlobalRenderSystem().capture("$CAM_HIGHLIGHT");
    _primitiveHighlightShader = GlobalRenderSystem().capture("$CAM_OVERLAY");
}

void CamWnd::releaseStates() {
    _faceHighlightShader = ShaderPtr();
    _primitiveHighlightShader = ShaderPtr();
}

void CamWnd::queueDraw()
{
    if (_drawing)
    {
        return;
    }

    _wxGLWidget->Refresh(false);
}

const Vector3& CamWnd::getCameraOrigin() const
{
    return _camera.getCameraOrigin();
}

void CamWnd::setCameraOrigin(const Vector3& origin)
{
    _camera.setCameraOrigin(origin);
}

const Vector3& CamWnd::getRightVector() const
{
    return _camera.getRightVector();
}

const Vector3& CamWnd::getUpVector() const
{
    return _camera.getUpVector();
}

const Vector3& CamWnd::getForwardVector() const
{
    return _camera.getForwardVector();
}

const Vector3& CamWnd::getCameraAngles() const
{
    return _camera.getCameraAngles();
}

void CamWnd::setCameraAngles(const Vector3& angles)
{
    _camera.setCameraAngles(angles);
}

const Matrix4& CamWnd::getModelView() const
{
    return _camera.getModelView();
}

const Matrix4& CamWnd::getProjection() const
{
    return _camera.getProjection();
}

const Frustum& CamWnd::getViewFrustum() const
{
    return _view.getFrustum();
}

void CamWnd::onFarClipPlaneOutClick(wxCommandEvent& ev) 
{
    farClipPlaneOut();
}

void CamWnd::onFarClipPlaneInClick(wxCommandEvent& ev) 
{
    farClipPlaneIn();
}

void CamWnd::farClipPlaneOut() 
{
    getCameraSettings()->setCubicScale( getCameraSettings()->cubicScale() + 1 );

    _camera.updateProjection();
    update();
}

void CamWnd::farClipPlaneIn() 
{
    getCameraSettings()->setCubicScale( getCameraSettings()->cubicScale() - 1 );

    _camera.updateProjection();
    update();
}

void CamWnd::onGLResize(wxSizeEvent& ev)
{
    getCamera().setDeviceWidth(ev.GetSize().GetWidth());
    getCamera().setDeviceHeight(ev.GetSize().GetHeight());
    getCamera().updateProjection();

    queueDraw();

    ev.Skip();
}

void CamWnd::onMouseScroll(wxMouseEvent& ev)
{
    float movementSpeed = static_cast<float>(getCameraSettings()->movementSpeed());

    if (ev.ShiftDown())
    {
        movementSpeed *= 2;
    }
    else if (ev.AltDown())
    {
        movementSpeed *= 0.1f;
    }

    // Determine the direction we are moving.
    if (ev.GetWheelRotation() > 0)
    {
        getCamera().freemoveUpdateAxes();
        _camera.setCameraOrigin(_camera.getCameraOrigin() - getCamera().getForwardVector() * movementSpeed);
    }
    else if (ev.GetWheelRotation() < 0)
    {
        getCamera().freemoveUpdateAxes();
        _camera.setCameraOrigin(_camera.getCameraOrigin() - getCamera().getForwardVector() * -movementSpeed);
    }
}

CameraMouseToolEvent CamWnd::createMouseEvent(const Vector2& point, const Vector2& delta)
{
    // When freeMove is enabled, snap the mouse coordinates to the center of the view widget
    Vector2 actualPoint = freeMoveEnabled() ? windowvector_for_widget_centre(*_wxGLWidget) : point;

    Vector2 normalisedDeviceCoords = device_constrained(
        window_to_normalised_device(actualPoint, _camera.getDeviceWidth(), _camera.getDeviceHeight()));

    return CameraMouseToolEvent(*this, normalisedDeviceCoords, delta);
}

MouseTool::Result CamWnd::processMouseDownEvent(const MouseToolPtr& tool, const Vector2& point)
{
    CameraMouseToolEvent ev = createMouseEvent(point);
    return tool->onMouseDown(ev);
}

MouseTool::Result CamWnd::processMouseUpEvent(const MouseToolPtr& tool, const Vector2& point)
{
    CameraMouseToolEvent ev = createMouseEvent(point);
    return tool->onMouseUp(ev);
}

MouseTool::Result CamWnd::processMouseMoveEvent(const MouseToolPtr& tool, int x, int y)
{
    bool mouseToolReceivesDeltas = (tool->getPointerMode() & MouseTool::PointerMode::MotionDeltas) != 0;

    // New MouseTool event, optionally passing the delta only
    CameraMouseToolEvent ev = mouseToolReceivesDeltas ?
        createMouseEvent(Vector2(0, 0), Vector2(x, y)) :
        createMouseEvent(Vector2(x, y));

    return tool->onMouseMove(ev);
}

void CamWnd::startCapture(const ui::MouseToolPtr& tool)
{
    if (_freezePointer.isCapturing(_wxGLWidget))
    {
        return;
    }

    unsigned int pointerMode = tool->getPointerMode();

    _freezePointer.startCapture(_wxGLWidget,
        [&](int x, int y, int mouseState) // Motion Functor
        { 
            MouseToolHandler::onGLCapturedMouseMove(x, y, mouseState); 

            if (freeMoveEnabled())
            {
                handleGLMouseMoveFreeMoveDelta(x, y, mouseState);
            }
        }, 
        [&, tool]() { MouseToolHandler::handleCaptureLost(tool); }, // called when the capture is lost.
        (pointerMode & MouseTool::PointerMode::Freeze) != 0,
        (pointerMode & MouseTool::PointerMode::Hidden) != 0,
        (pointerMode & MouseTool::PointerMode::MotionDeltas) != 0
    );
}

void CamWnd::endCapture()
{
    if (!_freezePointer.isCapturing(_wxGLWidget))
    {
        return;
    }

    _freezePointer.endCapture();
}

IInteractiveView& CamWnd::getInteractiveView()
{
    return *this;
}

void CamWnd::forceRedraw()
{
    if (_drawing)
    {
        return;
    }

    _wxGLWidget->Refresh(false);
    _wxGLWidget->Update();
}

void CamWnd::onGLMouseButtonPress(wxMouseEvent& ev)
{
    // The focus might be on some editable child window - since the
    // GL widget cannot be focused itself, let's reset the focus on the toplevel window
    // which will propagate any key events accordingly.
    GlobalMainFrame().getWxTopLevelWindow()->SetFocus();

    // Pass the call to the actual handler
    MouseToolHandler::onGLMouseButtonPress(ev);
}

void CamWnd::onGLMouseButtonRelease(wxMouseEvent& ev)
{
    MouseToolHandler::onGLMouseButtonRelease(ev);
}

void CamWnd::onGLMouseMove(wxMouseEvent& ev)
{
    MouseToolHandler::onGLMouseMove(ev);
}

void CamWnd::handleGLMouseMoveFreeMoveDelta(int x, int y, unsigned int state)
{
    _deferredMotionDelta.onMouseMotionDelta(x, y, state);

    unsigned int strafeFlags = GlobalCamera().getStrafeModifierFlags();

    _strafe = (state & strafeFlags) == strafeFlags;

    if (_strafe)
    {
        unsigned int strafeForwardFlags = GlobalCamera().getStrafeForwardModifierFlags();
        _strafeForward = (state & strafeForwardFlags) == strafeForwardFlags;
    }
    else
    {
        _strafeForward = false;
    }
}

void CamWnd::drawTime()
{
    if (GlobalRenderSystem().getTime() == 0)
    {
        return;
    }

    auto width = _camera.getDeviceWidth();
    auto height = _camera.getDeviceHeight();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, static_cast<float>(width), 0, static_cast<float>(height), -100, 100);

    glScalef(1, -1, 1);
    glTranslatef(static_cast<float>(width) - 90, -static_cast<float>(height), 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (GLEW_VERSION_1_3)
    {
        glClientActiveTexture(GL_TEXTURE0);
        glActiveTexture(GL_TEXTURE0);
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_DEPTH_TEST);

    glColor3f(1.f, 1.f, 1.f);
    glLineWidth(1);

    glRasterPos3f(1.0f, static_cast<float>(height) - 1.0f, 0.0f);

    std::size_t time = GlobalRenderSystem().getTime();
    GlobalOpenGL().drawString(fmt::format("Time: {0:.3f} sec.", (time * 0.001f)));
}

void CamWnd::handleFreeMoveKeyEvent(KeyEventType eventType, unsigned int movementFlags)
{
    if (eventType == KeyEventType::KeyPressed)
    {
        setFreeMoveFlags(movementFlags);
    }
    else
    {
        clearFreeMoveFlags(movementFlags);
    }
}

void CamWnd::handleKeyEvent(KeyEventType eventType, unsigned int freeMoveFlags, const std::function<void()>& discreteMovement)
{
    if (freeMoveEnabled())
    {
        handleFreeMoveKeyEvent(eventType, freeMoveFlags);
    }
    else if (eventType == KeyEventType::KeyPressed)
    {
        discreteMovement();
    }
}

void CamWnd::onForwardKey(KeyEventType eventType)
{
    handleKeyEvent(eventType, MOVE_FORWARD, [this]() { _camera.moveForwardDiscrete(SPEED_MOVE); });
}

void CamWnd::onBackwardKey(KeyEventType eventType)
{
    handleKeyEvent(eventType, MOVE_BACK, [this]() { _camera.moveBackDiscrete(SPEED_MOVE); });
}

void CamWnd::onLeftKey(KeyEventType eventType)
{
    handleKeyEvent(eventType, MOVE_STRAFELEFT, [this]() { _camera.rotateLeftDiscrete(); });
}

void CamWnd::onRightKey(KeyEventType eventType)
{
    handleKeyEvent(eventType, MOVE_STRAFERIGHT, [this]() { _camera.rotateRightDiscrete(); });
}

void CamWnd::onUpKey(KeyEventType eventType)
{
    handleKeyEvent(eventType, MOVE_UP, [this]() { _camera.moveUpDiscrete(SPEED_MOVE); });
}

void CamWnd::onDownKey(KeyEventType eventType)
{
    handleKeyEvent(eventType, MOVE_DOWN, [this]() { _camera.moveDownDiscrete(SPEED_MOVE); });
}

// -------------------------------------------------------------------------------

ShaderPtr CamWnd::_faceHighlightShader;
ShaderPtr CamWnd::_primitiveHighlightShader;
int CamWnd::_maxId = 0;

} // namespace
