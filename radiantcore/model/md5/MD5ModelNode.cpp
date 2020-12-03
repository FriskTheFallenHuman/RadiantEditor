#include "MD5ModelNode.h"

#include "ivolumetest.h"
#include "imodelcache.h"
#include "ishaders.h"
#include "iscenegraph.h"
#include <functional>

namespace md5
{

MD5ModelNode::MD5ModelNode(const MD5ModelPtr& model) :
    _model(new MD5Model(*model)) // create a copy of the incoming model, we need our own instance
{
}

const model::IModel& MD5ModelNode::getIModel() const {
    return *_model;
}

model::IModel& MD5ModelNode::getIModel() {
    return *_model;
}

bool MD5ModelNode::hasModifiedScale()
{
    return false; // not supported
}

Vector3 MD5ModelNode::getModelScale()
{
	return Vector3(1, 1, 1); // not supported
}

MD5ModelNode::~MD5ModelNode()
{
}

void MD5ModelNode::setModel(const MD5ModelPtr& model) {
    _model = model;
}

const MD5ModelPtr& MD5ModelNode::getModel() const {
    return _model;
}

// Bounded implementation
const AABB& MD5ModelNode::localAABB() const {
    return _model->localAABB();
}

std::string MD5ModelNode::name() const {
    return _model->getFilename();
}

scene::INode::Type MD5ModelNode::getNodeType() const
{
    return Type::Model;
}

void MD5ModelNode::testSelect(Selector& selector, SelectionTest& test) {
    _model->testSelect(selector, test, localToWorld());
}

bool MD5ModelNode::getIntersection(const Ray& ray, Vector3& intersection)
{
    return _model->getIntersection(ray, intersection, localToWorld());
}

bool MD5ModelNode::intersectsLight(const RendererLight& light) const
{
    return light.intersectsAABB(worldAABB());
}

void MD5ModelNode::renderSolid(RenderableCollector& collector, const VolumeTest& volume) const
{
    assert(_renderEntity);

    render(collector, volume, localToWorld(), *_renderEntity);
}

void MD5ModelNode::renderWireframe(RenderableCollector& collector, const VolumeTest& volume) const
{
    assert(_renderEntity);

    render(collector, volume, localToWorld(), *_renderEntity);
}

void MD5ModelNode::setRenderSystem(const RenderSystemPtr& renderSystem)
{
    Node::setRenderSystem(renderSystem);

    _model->setRenderSystem(renderSystem);
}

void MD5ModelNode::render(RenderableCollector& collector, const VolumeTest& volume,
        const Matrix4& localToWorld, const IRenderEntity& entity) const
{
    // Do some rough culling (per model, not per surface)
    if (volume.TestAABB(localAABB(), localToWorld) == VOLUME_OUTSIDE)
    {
        return;
    }

    // greebo: Iterate over all MD5 surfaces and render them
    for (auto i = _model->begin(); i != _model->end(); ++i)
    {
        assert(i->shader);

        // Get the Material to test the shader name against the filter system
        const MaterialPtr& surfaceShader = i->shader->getMaterial();
        if (surfaceShader->isVisible())
        {
            assert(i->shader); // shader must be captured at this point
            collector.addRenderable(
                collector.supportsFullMaterials() ? *i->shader
                                                  : *entity.getWireShader(),
                *i->surface, localToWorld, this, &entity
            );
        }
    }

    // Uncomment to render the skeleton
    //collector.SetState(entity.getWireShader(), RenderableCollector::eFullMaterials);
    //collector.addRenderable(_model->getRenderableSkeleton(), localToWorld, entity);
}

// Returns the name of the currently active skin
std::string MD5ModelNode::getSkin() const {
    return _skin;
}

void MD5ModelNode::skinChanged(const std::string& newSkinName)
{
    // greebo: Store the new skin name locally
    _skin = newSkinName;

    // greebo: Acquire the ModelSkin reference from the SkinCache
    // Note: This always returns a valid reference
    ModelSkin& skin = GlobalModelSkinCache().capture(_skin);

    _model->applySkin(skin);

    // Refresh the scene
    GlobalSceneGraph().sceneChanged();
}

} // namespace md5
