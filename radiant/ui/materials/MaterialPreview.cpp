#include "MaterialPreview.h"

#include "ibrush.h"
#include "ientity.h"
#include "ieclass.h"
#include "string/convert.h"
#include "math/pi.h"
#include "wxutil/dialog/MessageBox.h"
#include "wxutil/Bitmap.h"
#include <wx/toolbar.h>

namespace ui
{

namespace
{
    const char* const FUNC_STATIC_CLASS = "func_static";
}

MaterialPreview::MaterialPreview(wxWindow* parent) :
    RenderPreview(parent, true),
    _sceneIsReady(false),
    _defaultCamDistanceFactor(2.0f)
{
    _testModelSkin.reset(new TestModelSkin("model"));
    GlobalModelSkinCache().addNamedSkin(_testModelSkin);

    _testRoomSkin.reset(new TestModelSkin("room"));
    GlobalModelSkinCache().addNamedSkin(_testRoomSkin);

    setupToolbar();
}

MaterialPreview::~MaterialPreview()
{
    if (_testModelSkin)
    {
        GlobalModelSkinCache().removeSkin(_testModelSkin->getName());
        _testModelSkin.reset();
    }

    if (_testRoomSkin)
    {
        GlobalModelSkinCache().removeSkin(_testRoomSkin->getName());
        _testRoomSkin.reset();
    }
}

void MaterialPreview::setupToolbar()
{
    // Add one additional toolbar for particle-related stuff
    wxToolBar* toolbar = new wxToolBar(_mainPanel, wxID_ANY);
    toolbar->SetToolBitmapSize(wxSize(16, 16));

    _testModelCubeButton = toolbar->AddRadioTool(wxID_ANY, "", wxutil::GetLocalBitmap("cube.png", wxART_TOOLBAR));
    _testModelCubeButton->SetShortHelp(_("Show Cube"));
    toolbar->ToggleTool(_testModelCubeButton->GetId(), true);
    
    _testModelSphereButton = toolbar->AddRadioTool(wxID_ANY, "", wxutil::GetLocalBitmap("sphere.png", wxART_TOOLBAR));
    _testModelSphereButton->SetShortHelp(_("Show Sphere"));

    toolbar->Bind(wxEVT_TOOL, &MaterialPreview::onTestModelSelectionChanged, this, _testModelCubeButton->GetId());
    toolbar->Bind(wxEVT_TOOL, &MaterialPreview::onTestModelSelectionChanged, this, _testModelSphereButton->GetId());

    toolbar->Realize();

    addToolbar(toolbar);
}

const MaterialPtr& MaterialPreview::getMaterial()
{
    return _material;
}

void MaterialPreview::updateModelSkin()
{
    // Let the model update its remaps
    auto skinnedModel = std::dynamic_pointer_cast<SkinnedModel>(_model);

    if (skinnedModel)
    {
        skinnedModel->skinChanged(_testModelSkin->getName());
    }
}

void MaterialPreview::updateRoomSkin()
{
    // Let the model update its remaps
    auto skinnedRoom = std::dynamic_pointer_cast<SkinnedModel>(_room);

    if (skinnedRoom)
    {
        skinnedRoom->skinChanged(_testRoomSkin->getName());
    }
}

void MaterialPreview::setMaterial(const MaterialPtr& material)
{
    bool hadMaterial = _material != nullptr;

    _material = material;
    _sceneIsReady = false;

    if (_model)
    {
        // Assign the material to the temporary skin
        _testModelSkin->setRemapMaterial(_material);

        updateModelSkin();
    }

    if (!hadMaterial && _material)
    {
        setLightingModeEnabled(true);
        startPlayback();
    }
    else if (hadMaterial && !_material)
    {
        stopPlayback();
    }

    queueDraw();
}

void MaterialPreview::onMaterialChanged()
{
    if (!_material) return;

    _renderSystem->onMaterialChanged(_material->getName());
    queueDraw();
}

bool MaterialPreview::onPreRender()
{
    if (!_sceneIsReady)
    {
        prepareScene();
    }

    // Update the rotation of the func_static
    if (_model)
    {
        auto time = _renderSystem->getTime();

        // one full rotation per 10 seconds
        auto newAngle = 2 * c_pi * time / 10000;

        Node_getEntity(_entity)->setKeyValue("angle", string::to_string(radians_to_degrees(newAngle)));
    }

    return RenderPreview::onPreRender();
}

void MaterialPreview::prepareScene()
{
    _sceneIsReady = true;
}

bool MaterialPreview::canDrawGrid()
{
    return false;
}

void MaterialPreview::setupSceneGraph()
{
    RenderPreview::setupSceneGraph();

    try
    {
        _rootNode = std::make_shared<scene::BasicRootNode>();

        setupRoom();

        _entity = GlobalEntityModule().createEntity(
            GlobalEntityClassManager().findClass(FUNC_STATIC_CLASS));

        _rootNode->addChildNode(_entity);

        setupTestModel();

        getScene()->setRoot(_rootNode);

        // Set up the light
        _light = GlobalEntityModule().createEntity(
            GlobalEntityClassManager().findClass("light"));

        Node_getEntity(_light)->setKeyValue("light_radius", "600 600 600");
        Node_getEntity(_light)->setKeyValue("origin", "250 250 250");

        _rootNode->addChildNode(_light);

        // Reset the default view, facing down to the model from diagonally above the bounding box
        double distance = _model->localAABB().getRadius() * _defaultCamDistanceFactor;

        setViewOrigin(Vector3(1, 1, 1) * distance);
        setViewAngles(Vector3(34, 135, 0));
    }
    catch (std::runtime_error&)
    {
        wxutil::Messagebox::ShowError(fmt::format(_("Unable to setup the preview,\n"
            "could not find the entity class {0}"), FUNC_STATIC_CLASS));
    }
}

void MaterialPreview::setupRoom()
{
    _room = GlobalModelCache().getModelNodeForStaticResource("preview/room_cuboid.ase");

    auto roomEntity = GlobalEntityModule().createEntity(
        GlobalEntityClassManager().findClass(FUNC_STATIC_CLASS));

    _rootNode->addChildNode(roomEntity);

    roomEntity->addChildNode(_room);
    updateRoomSkin();
}

void MaterialPreview::setupTestModel()
{
    if (_entity && _model)
    {
        _entity->removeChildNode(_model);
        _model.reset();
    }

    // Load the pre-defined model from the resources path
    if (_testModelCubeButton->IsToggled())
    {
        _model = GlobalModelCache().getModelNodeForStaticResource("preview/cube.ase");
    }
    else // sphere
    {
        _model = GlobalModelCache().getModelNodeForStaticResource("preview/sphere.ase");
    }

    // The test model is a child of this entity
    _entity->addChildNode(_model);

    updateModelSkin();
}

void MaterialPreview::onTestModelSelectionChanged(wxCommandEvent& ev)
{
    setupTestModel();
    queueDraw();
}

}
