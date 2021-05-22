#include "BrushNode.h"

#include "ivolumetest.h"
#include "ifilter.h"
#include "iradiant.h"
#include "icounter.h"
#include "iclipper.h"
#include "ientity.h"
#include "math/Frustum.h"
#include "math/Hash.h"
#include <functional>

// Constructor
BrushNode::BrushNode() :
	scene::SelectableNode(),
	m_brush(*this),
	_selectedPoints(GL_POINTS),
	_faceCentroidPointsCulled(GL_POINTS),
	m_viewChanged(false),
	_renderableComponentsNeedUpdate(true),
    _untransformedOriginChanged(true)
{
	m_brush.attach(*this); // BrushObserver
}

// Copy Constructor
BrushNode::BrushNode(const BrushNode& other) :
	scene::SelectableNode(other),
	scene::Cloneable(other),
	Snappable(other),
	IBrushNode(other),
	BrushObserver(other),
	SelectionTestable(other),
	ComponentSelectionTestable(other),
	ComponentEditable(other),
	ComponentSnappable(other),
	PlaneSelectable(other),
	LitObject(other),
	Transformable(other),
	m_brush(*this, other.m_brush),
	_selectedPoints(GL_POINTS),
	_faceCentroidPointsCulled(GL_POINTS),
	m_viewChanged(false),
	_renderableComponentsNeedUpdate(true),
    _untransformedOriginChanged(true)
{
	m_brush.attach(*this); // BrushObserver
}

BrushNode::~BrushNode()
{
	m_brush.detach(*this); // BrushObserver
}

scene::INode::Type BrushNode::getNodeType() const
{
	return Type::Brush;
}

const AABB& BrushNode::localAABB() const {
	return m_brush.localAABB();
}

std::size_t BrushNode::getFingerprint()
{
    constexpr std::size_t SignificantDigits = scene::SignificantFingerprintDoubleDigits;

    if (m_brush.getNumFaces() == 0)
    {
        return 0; // empty brushes produce a zero fingerprint
    }

    auto hash = static_cast<std::size_t>(m_brush.getDetailFlag() + 1);

    math::combineHash(hash, m_brush.getNumFaces());

    // Combine all face plane equations
    for (const auto& face : m_brush)
    {
        // Plane equation
        math::combineHash(hash, math::hashVector3(face->getPlane3().normal(), SignificantDigits));
        math::combineHash(hash, math::hashDouble(face->getPlane3().dist(), SignificantDigits));

        // Material Name
        math::combineHash(hash, std::hash<std::string>()(face->getShader()));

        // Texture Matrix
        auto texdef = face->getTexDefMatrix();
        math::combineHash(hash, math::hashDouble(texdef.xx(), SignificantDigits));
        math::combineHash(hash, math::hashDouble(texdef.yx(), SignificantDigits));
        math::combineHash(hash, math::hashDouble(texdef.tx(), SignificantDigits));
        math::combineHash(hash, math::hashDouble(texdef.xy(), SignificantDigits));
        math::combineHash(hash, math::hashDouble(texdef.yy(), SignificantDigits));
        math::combineHash(hash, math::hashDouble(texdef.ty(), SignificantDigits));
    }

    return hash;
}

// Snappable implementation
void BrushNode::snapto(float snap) {
	m_brush.snapto(snap);
}

void BrushNode::snapComponents(float snap) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->snapComponents(snap);
	}
}

void BrushNode::testSelect(Selector& selector, SelectionTest& test)
{
    // BeginMesh(true): Always treat brush faces twosided when in orthoview
	test.BeginMesh(localToWorld(), !test.getVolume().fill());

	SelectionIntersection best;
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i)
	{
		if (i->faceIsVisible())
		{
			i->testSelect(test, best);
		}
	}

	if (best.isValid()) {
		selector.addIntersection(best);
	}
}

bool BrushNode::isSelectedComponents() const {
	for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		if (i->selectedComponents()) {
			return true;
		}
	}
	return false;
}

void BrushNode::setSelectedComponents(bool select, SelectionSystem::EComponentMode mode) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->setSelected(mode, select);
	}
}

void BrushNode::invertSelectedComponents(SelectionSystem::EComponentMode mode)
{
	// Component mode, invert the component selection
	switch (mode)
	{
	case SelectionSystem::eVertex:
		for (brush::VertexInstance& vertex : m_vertexInstances)
		{
			vertex.invertSelected();
		}
		break;
	case SelectionSystem::eEdge:
		for (EdgeInstance& edge : m_edgeInstances)
		{
			edge.invertSelected();
		}
		break;
	case SelectionSystem::eFace:
		for (FaceInstance& face : m_faceInstances)
		{
			face.invertSelected();
		}
		break;
	case SelectionSystem::eDefault:
		break;
	} // switch
}

void BrushNode::testSelectComponents(Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode) {
	test.BeginMesh(localToWorld());

	switch (mode) {
		case SelectionSystem::eVertex: {
				for (VertexInstances::iterator i = m_vertexInstances.begin(); i != m_vertexInstances.end(); ++i) {
					i->testSelect(selector, test);
				}
			}
		break;
		case SelectionSystem::eEdge: {
				for (EdgeInstances::iterator i = m_edgeInstances.begin(); i != m_edgeInstances.end(); ++i) {
					i->testSelect(selector, test);
				}
			}
			break;
		case SelectionSystem::eFace: {
				if (test.getVolume().fill()) {
					for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
						i->testSelect(selector, test);
					}
				}
				else {
					for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
						i->testSelect_centroid(selector, test);
					}
				}
			}
			break;
		default:
			break;
	}
}

const AABB& BrushNode::getSelectedComponentsBounds() const {
	m_aabb_component = AABB();

	for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->iterate_selected(m_aabb_component);
	}

	return m_aabb_component;
}

void BrushNode::selectPlanes(Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback) {
	test.BeginMesh(localToWorld());

	PlanePointer brushPlanes[brush::c_brush_maxFaces];
	PlanesIterator j = brushPlanes;

	for (Brush::const_iterator i = m_brush.begin(); i != m_brush.end(); ++i) {
		*j++ = &(*i)->plane3();
	}

	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->selectPlane(selector, Line(test.getNear(), test.getFar()), brushPlanes, j, selectedPlaneCallback);
	}
}

void BrushNode::selectReversedPlanes(Selector& selector, const SelectedPlanes& selectedPlanes) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->selectReversedPlane(selector, selectedPlanes);
	}
}

void BrushNode::selectedChangedComponent(const ISelectable& selectable)
{
	_renderableComponentsNeedUpdate = true;

	GlobalSelectionSystem().onComponentSelection(SelectableNode::getSelf(), selectable);
}

// IBrushNode implementation
Brush& BrushNode::getBrush() {
	return m_brush;
}

IBrush& BrushNode::getIBrush() {
	return m_brush;
}

void BrushNode::translate(const Vector3& translation)
{
	m_brush.translate(translation);
}

scene::INodePtr BrushNode::clone() const {
	return std::make_shared<BrushNode>(*this);
}

void BrushNode::onInsertIntoScene(scene::IMapRootNode& root)
{
    m_brush.connectUndoSystem(root.getUndoChangeTracker());
	GlobalCounters().getCounter(counterBrushes).increment();

    // Update the origin information needed for transformations
    _untransformedOriginChanged = true;

	SelectableNode::onInsertIntoScene(root);
}

void BrushNode::onRemoveFromScene(scene::IMapRootNode& root)
{
	// De-select this node
	setSelected(false);

	// De-select all child components as well
	setSelectedComponents(false, SelectionSystem::eVertex);
	setSelectedComponents(false, SelectionSystem::eEdge);
	setSelectedComponents(false, SelectionSystem::eFace);

	GlobalCounters().getCounter(counterBrushes).decrement();
    m_brush.disconnectUndoSystem(root.getUndoChangeTracker());

	SelectableNode::onRemoveFromScene(root);
}

void BrushNode::clear() {
	m_faceInstances.clear();
}

void BrushNode::reserve(std::size_t size) {
	m_faceInstances.reserve(size);
}

void BrushNode::push_back(Face& face) {
	m_faceInstances.push_back(FaceInstance(face, std::bind(&BrushNode::selectedChangedComponent, this, std::placeholders::_1)));
    _untransformedOriginChanged = true;
}

void BrushNode::pop_back() {
	ASSERT_MESSAGE(!m_faceInstances.empty(), "erasing invalid element");
	m_faceInstances.pop_back();
    _untransformedOriginChanged = true;
}

void BrushNode::erase(std::size_t index) {
	ASSERT_MESSAGE(index < m_faceInstances.size(), "erasing invalid element");
	m_faceInstances.erase(m_faceInstances.begin() + index);
}
void BrushNode::connectivityChanged() {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->connectivityChanged();
	}
}

void BrushNode::edge_clear() {
	m_edgeInstances.clear();
}
void BrushNode::edge_push_back(SelectableEdge& edge) {
	m_edgeInstances.push_back(EdgeInstance(m_faceInstances, edge));
}

void BrushNode::vertex_clear() {
	m_vertexInstances.clear();
}
void BrushNode::vertex_push_back(SelectableVertex& vertex) {
	m_vertexInstances.push_back(brush::VertexInstance(m_faceInstances, vertex));
}

void BrushNode::DEBUG_verify() {
	ASSERT_MESSAGE(m_faceInstances.size() == m_brush.DEBUG_size(), "FATAL: mismatch");
}

bool BrushNode::intersectsLight(const RendererLight& light) const {
	return light.lightAABB().intersects(worldAABB());
}

void BrushNode::renderComponents(RenderableCollector& collector, const VolumeTest& volume) const
{
	m_brush.evaluateBRep();

	const Matrix4& l2w = localToWorld();

	if (volume.fill() && GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace)
	{
		evaluateViewDependent(volume, l2w);
		collector.addRenderable(*m_brush.m_state_point, _faceCentroidPointsCulled, l2w);
	}
	else
	{
		m_brush.renderComponents(GlobalSelectionSystem().ComponentMode(), collector, volume, l2w);
	}
}

void BrushNode::renderSolid(RenderableCollector& collector, const VolumeTest& volume) const
{
	m_brush.evaluateBRep();

	renderClipPlane(collector, volume);

	renderSolid(collector, volume, localToWorld());
}

void BrushNode::renderWireframe(RenderableCollector& collector, const VolumeTest& volume) const
{
	m_brush.evaluateBRep();

	renderClipPlane(collector, volume);

	renderWireframe(collector, volume, localToWorld());
}

void BrushNode::setRenderSystem(const RenderSystemPtr& renderSystem)
{
	SelectableNode::setRenderSystem(renderSystem);

	if (renderSystem)
	{
		m_state_selpoint = renderSystem->capture("$SELPOINT");
	}
	else
	{
		m_state_selpoint.reset();
	}

	m_brush.setRenderSystem(renderSystem);
	m_clipPlane.setRenderSystem(renderSystem);
}

void BrushNode::renderClipPlane(RenderableCollector& collector, const VolumeTest& volume) const
{
	if (GlobalClipper().clipMode() && isSelected())
	{
		m_clipPlane.render(collector, volume, localToWorld());
	}
}

void BrushNode::viewChanged() const {
	m_viewChanged = true;
}

std::size_t BrushNode::getHighlightFlags()
{
	if (!isSelected()) return Highlight::NoHighlight;

	return isGroupMember() ? (Highlight::Selected | Highlight::GroupMember) : Highlight::Selected;
}

void BrushNode::evaluateViewDependent(const VolumeTest& volume, const Matrix4& localToWorld) const
{
	if (!m_viewChanged) return;

	m_viewChanged = false;

	// Array of booleans to indicate which faces are visible
	static bool faces_visible[brush::c_brush_maxFaces];

	// Will hold the indices of all visible faces (from the current viewpoint)
	static std::size_t visibleFaceIndices[brush::c_brush_maxFaces];

	std::size_t numVisibleFaces(0);
	bool* j = faces_visible;
	bool forceVisible = isForcedVisible();

	// Iterator to an index of a visible face
	std::size_t* visibleFaceIter = visibleFaceIndices;
	std::size_t curFaceIndex = 0;

	for (FaceInstances::const_iterator i = m_faceInstances.begin();
		 i != m_faceInstances.end();
		 ++i, ++j, ++curFaceIndex)
	{
		// Check if face is filtered before adding to visibility matrix
		// greebo: Removed localToWorld transformation here, brushes don't have a non-identity l2w
        // Don't cull backfacing planes to make those faces visible in orthoview (#5465)
		if (forceVisible || i->faceIsVisible())
		{
			*j = true;

			// Store the index of this visible face in the array
			*visibleFaceIter++ = curFaceIndex;
			numVisibleFaces++;
		}
		else
		{
			*j = false;
		}
	}

	m_brush.update_wireframe(m_render_wireframe, faces_visible);
	m_brush.update_faces_wireframe(_faceCentroidPointsCulled, visibleFaceIndices, numVisibleFaces);
}

void BrushNode::renderSolid(RenderableCollector& collector,
                            const VolumeTest& volume,
                            const Matrix4& localToWorld) const
{
	assert(_renderEntity); // brushes rendered without parent entity - no way!

	// Check for the override status of this brush
	bool forceVisible = isForcedVisible();

    // Submit the lights and renderable geometry for each face
    for (const FaceInstance& faceInst : m_faceInstances)
    {
		// Skip invisible faces before traversing further
        if (!forceVisible && !faceInst.faceIsVisible()) continue;

        const Face& face = faceInst.getFace();
        if (face.intersectVolume(volume))
        {
            bool highlight = faceInst.selectedComponents();
            if (highlight)
                collector.setHighlightFlag(RenderableCollector::Highlight::Faces, true);

            // greebo: BrushNodes have always an identity l2w, don't do any transforms
            collector.addRenderable(
                *face.getFaceShader().getGLShader(), face.getWinding(),
                Matrix4::getIdentity(), this, _renderEntity
            );

            if (highlight)
                collector.setHighlightFlag(RenderableCollector::Highlight::Faces, false);
        }
    }

	renderSelectedPoints(collector, volume, localToWorld);
}

void BrushNode::renderWireframe(RenderableCollector& collector, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	//renderCommon(collector, volume);

	evaluateViewDependent(volume, localToWorld);

	if (m_render_wireframe.m_size != 0)
	{
		collector.addRenderable(*_renderEntity->getWireShader(), m_render_wireframe, localToWorld);
	}

	renderSelectedPoints(collector, volume, localToWorld);
}

void BrushNode::update_selected() const
{
	if (!_renderableComponentsNeedUpdate) return;

	_renderableComponentsNeedUpdate = false;

	_selectedPoints.clear();

	for (FaceInstances::const_iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		if (i->getFace().contributes()) {
			i->iterate_selected(_selectedPoints);
		}
	}
}

void BrushNode::renderSelectedPoints(RenderableCollector& collector,
                                     const VolumeTest& volume,
                                     const Matrix4& localToWorld) const
{
	m_brush.evaluateBRep();

	update_selected();

	if (!_selectedPoints.empty())
    {
		collector.setHighlightFlag(RenderableCollector::Highlight::Primitives, false);
		collector.addRenderable(*m_state_selpoint, _selectedPoints, localToWorld);
	}
}

void BrushNode::evaluateTransform() {
	Matrix4 matrix(calculateTransform());
	//rMessage() << "matrix: " << matrix << "\n";

	if (getType() == TRANSFORM_PRIMITIVE)
    {
        // If this is a pure translation (no other bits set), call the specialised method
        if (getTransformationType() == Translation)
        {
            // TODO: Take care of texture lock
            for (auto face : m_brush)
            {
                face->translate(getTranslation());
            }
        }
        else
        {
            m_brush.transform(matrix);
        }
	}
	else {
		transformComponents(matrix);
	}
}

bool BrushNode::getIntersection(const Ray& ray, Vector3& intersection)
{
	return m_brush.getIntersection(ray, intersection);
}

void BrushNode::updateFaceVisibility()
{
	// Trigger an update, the brush might not have any faces calculated so far
	m_brush.evaluateBRep();

	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i)
	{
		i->updateFaceVisibility();
	}
}

void BrushNode::transformComponents(const Matrix4& matrix) {
	for (FaceInstances::iterator i = m_faceInstances.begin(); i != m_faceInstances.end(); ++i) {
		i->transformComponents(matrix);
	}
}

void BrushNode::setClipPlane(const Plane3& plane) {
	m_clipPlane.setPlane(m_brush, plane);
}

void BrushNode::forEachFaceInstance(const std::function<void(FaceInstance&)>& functor)
{
	std::for_each(m_faceInstances.begin(), m_faceInstances.end(), functor);
}

const Vector3& BrushNode::getUntransformedOrigin()
{
    if (_untransformedOriginChanged)
    {
        _untransformedOriginChanged = false;
        _untransformedOrigin = worldAABB().getOrigin();
    }

    return _untransformedOrigin;
}

bool BrushNode::facesAreForcedVisible()
{
    return isForcedVisible();
}

void BrushNode::_onTransformationChanged()
{
	m_brush.transformChanged();

	_renderableComponentsNeedUpdate = true;
}

void BrushNode::_applyTransformation()
{
	m_brush.revertTransform();
	evaluateTransform();
	m_brush.freezeTransform();

    _untransformedOriginChanged = true;
}
