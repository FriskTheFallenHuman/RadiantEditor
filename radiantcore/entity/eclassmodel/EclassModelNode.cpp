#include "EclassModelNode.h"

#include <functional>

namespace entity {

EclassModelNode::EclassModelNode(const IEntityClassPtr& eclass) :
	EntityNode(eclass),
    _originKey(std::bind(&EclassModelNode::originChanged, this)),
    _origin(ORIGINKEY_IDENTITY),
    _rotationKey(std::bind(&EclassModelNode::rotationChanged, this)),
    _angleKey(std::bind(&EclassModelNode::angleChanged, this)),
	_angle(AngleKey::IDENTITY),
    _renderOrigin(_origin),
	_localAABB(Vector3(0,0,0), Vector3(1,1,1)) // minimal AABB, is determined by child bounds anyway
{}

EclassModelNode::EclassModelNode(const EclassModelNode& other) :
	EntityNode(other),
	Snappable(other),
    _originKey(std::bind(&EclassModelNode::originChanged, this)),
    _origin(ORIGINKEY_IDENTITY),
    _rotationKey(std::bind(&EclassModelNode::rotationChanged, this)),
    _angleKey(std::bind(&EclassModelNode::angleChanged, this)),
	_angle(AngleKey::IDENTITY),
    _renderOrigin(_origin),
	_localAABB(Vector3(0,0,0), Vector3(1,1,1)) // minimal AABB, is determined by child bounds anyway
{}

EclassModelNode::~EclassModelNode()
{
    removeKeyObserver("origin", _originKey);
    removeKeyObserver("rotation", _rotationObserver);
    removeKeyObserver("angle", _angleObserver);
}

EclassModelNodePtr EclassModelNode::Create(const IEntityClassPtr& eclass)
{
	EclassModelNodePtr instance(new EclassModelNode(eclass));
	instance->construct();

	return instance;
}

void EclassModelNode::construct()
{
	EntityNode::construct();

    _rotationObserver.setCallback(std::bind(&RotationKey::rotationChanged, &_rotationKey, std::placeholders::_1));
	_angleObserver.setCallback(std::bind(&RotationKey::angleChanged, &_rotationKey, std::placeholders::_1));

    _rotation.setIdentity();

    addKeyObserver("angle", _angleObserver);
	addKeyObserver("rotation", _rotationObserver);
    addKeyObserver("origin", _originKey);
}

// Snappable implementation
void EclassModelNode::snapto(float snap)
{
    _originKey.snap(snap);
	_originKey.write(_spawnArgs);
}

const AABB& EclassModelNode::localAABB() const
{
	return _localAABB;
}

void EclassModelNode::onPreRender(const VolumeTest& volume)
{
    EntityNode::onPreRender(volume);

    if (isSelected())
    {
        _renderOrigin.update(_pivotShader);
    }
}

#if 0
void EclassModelNode::renderSolid(IRenderableCollector& collector, const VolumeTest& volume) const
{
	EntityNode::renderSolid(collector, volume);

    if (isSelected())
	{
		_renderOrigin.render(collector, volume, localToWorld());
	}
}

void EclassModelNode::renderWireframe(IRenderableCollector& collector, const VolumeTest& volume) const
{
	EntityNode::renderWireframe(collector, volume);

	if (isSelected())
	{
		_renderOrigin.render(collector, volume, localToWorld());
	}
}
#endif

void EclassModelNode::setRenderSystem(const RenderSystemPtr& renderSystem)
{
	EntityNode::setRenderSystem(renderSystem);

    if (renderSystem)
    {
        _pivotShader = renderSystem->capture("$PIVOT");
    }
    else
    {
        _pivotShader.reset();
    }
}

scene::INodePtr EclassModelNode::clone() const
{
	EclassModelNodePtr node(new EclassModelNode(*this));
	node->construct();
    node->constructClone(*this);

	return node;
}

void EclassModelNode::translate(const Vector3& translation)
{
	_origin += translation;
    _renderOrigin.queueUpdate();
}

void EclassModelNode::rotate(const Quaternion& rotation) 
{
	_rotation.rotate(rotation);
}

void EclassModelNode::_revertTransform()
{
	_origin = _originKey.get();
    _renderOrigin.queueUpdate();
	_rotation = _rotationKey.m_rotation;
}

void EclassModelNode::_freezeTransform()
{
	_originKey.set(_origin);
	_originKey.write(_spawnArgs);

	_rotationKey.m_rotation = _rotation;
	_rotationKey.write(&_spawnArgs, true);
}

void EclassModelNode::_onTransformationChanged()
{
	if (getType() == TRANSFORM_PRIMITIVE)
	{
		_revertTransform();

		translate(getTranslation());
		rotate(getRotation());

		updateTransform();
	}
}

void EclassModelNode::_applyTransformation()
{
	if (getType() == TRANSFORM_PRIMITIVE)
	{
		_revertTransform();

		translate(getTranslation());
		rotate(getRotation());

		_freezeTransform();
	}
}

const Vector3& EclassModelNode::getUntransformedOrigin()
{
    return _originKey.get();
}

void EclassModelNode::updateTransform()
{
    _renderOrigin.queueUpdate();

	localToParent() = Matrix4::getIdentity();
	localToParent().translateBy(_origin);

	localToParent().multiplyBy(_rotation.getMatrix4());

	EntityNode::transformChanged();
}

void EclassModelNode::originChanged()
{
	_origin = _originKey.get();
	updateTransform();
}

void EclassModelNode::rotationChanged()
{
	_rotation = _rotationKey.m_rotation;
	updateTransform();
}

void EclassModelNode::angleChanged()
{
	_angle = _angleKey.getValue();
	updateTransform();
}

void EclassModelNode::onSelectionStatusChange(bool changeGroupStatus)
{
    EntityNode::onSelectionStatusChange(changeGroupStatus);

    if (isSelected())
    {
        _renderOrigin.queueUpdate();
    }
    else
    {
        _renderOrigin.clear();
    }
}

} // namespace entity
