#pragma once

#include "ilightnode.h"

#include "Light.h"
#include "dragplanes.h"
#include "../VertexInstance.h"
#include "../EntityNode.h"

namespace entity
{

class LightNode;
typedef std::shared_ptr<LightNode> LightNodePtr;

/// Scenegraph node representing a light
class LightNode :
    public EntityNode,
    public ILightNode,
    public Snappable,
    public ComponentSelectionTestable,
    public ComponentEditable,
    public ComponentSnappable,
    public PlaneSelectable,
    public OpenGLRenderable
{
	Light _light;

	// The (draggable) light center instance
	VertexInstance _lightCenterInstance;

	VertexInstance _lightTargetInstance;
	VertexInstanceRelative _lightRightInstance;
	VertexInstanceRelative _lightUpInstance;
	VertexInstance _lightStartInstance;
	VertexInstance _lightEndInstance;

	// dragplanes for lightresizing using mousedrag
    selection::DragPlanes _dragPlanes;

	// Renderable components of this light
	RenderLightRadiiBox _renderableRadius;
    RenderLightProjection _renderableFrustum;

	// a temporary variable for calculating the AABB of all (selected) components
	mutable AABB m_aabb_component;

public:
	LightNode(const IEntityClassPtr& eclass);

private:
	LightNode(const LightNode& other);

public:
	static LightNodePtr Create(const IEntityClassPtr& eclass);

	// RenderEntity implementation
	virtual float getShaderParm(int parmNum) const override;

	// Bounded implementation
	virtual const AABB& localAABB() const override;

	// override scene::Inode methods to deselect the child components
	virtual void onInsertIntoScene(scene::IMapRootNode& root) override;
	virtual void onRemoveFromScene(scene::IMapRootNode& root) override;

	// Snappable implementation
	virtual void snapto(float snap) override;

	/** greebo: Returns the AABB of the small diamond representation.
	 *	(use this to select the light against an AABB selectiontest like CompleteTall or similar).
	 */
	AABB getSelectAABB() const override;

	/*greebo: This is a callback function that gets connected in the constructor
	* Don't know exactly what it does, but it seems to notify the shader cache that the light has moved or
	* something like that.*/
	void lightChanged();

	/* greebo: This snaps the components to the grid.
	 *
	 * Note: if none are selected, ALL the components are snapped to the grid (I hope this is intentional)
	 * This function can only be called in Selection::eVertex mode, so I assume that the user wants all components
	 * to be snapped.
	 *
	 * If one or more components is/are selected, ONLY those are snapped to the grid.
	 */
	void snapComponents(float snap) override;

	// PlaneSelectable implementation
	void selectPlanes(Selector& selector, SelectionTest& test, const PlaneCallback& selectedPlaneCallback) override;
	void selectReversedPlanes(Selector& selector, const SelectedPlanes& selectedPlanes) override;

	// Test the light volume for selection, this just passes the call on to the contained Light class
	void testSelect(Selector& selector, SelectionTest& test) override;

	// greebo: Returns true if drag planes or the light center is selected (both are components)
	bool isSelectedComponents() const override;
	// greebo: Selects/deselects all components, depending on the chosen componentmode
	void setSelectedComponents(bool select, SelectionSystem::EComponentMode mode) override;
	void invertSelectedComponents(SelectionSystem::EComponentMode mode) override;
	void testSelectComponents(Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode) override;

	/**
	 * greebo: This returns the AABB of all the selectable vertices. This method
	 * distinguishes between projected and point lights and stretches the AABB accordingly.
	 */
	// ComponentEditable implementation
	const AABB& getSelectedComponentsBounds() const override;

	scene::INodePtr clone() const override;

	void selectedChangedComponent(const ISelectable& selectable);

	// Renderable implementation
	void renderSolid(RenderableCollector& collector, const VolumeTest& volume) const override;
	void renderWireframe(RenderableCollector& collector, const VolumeTest& volume) const override;
	void setRenderSystem(const RenderSystemPtr& renderSystem) override;
	void renderComponents(RenderableCollector& collector, const VolumeTest& volume) const override;

    // OpenGLRenderable implementation
    void render(const RenderInfo& info) const;

	const Matrix4& rotation() const;

    // Returns the original "origin" value
    const Vector3& getUntransformedOrigin() override;

protected:
	// Gets called by the Transformable implementation whenever
	// scale, rotation or translation is changed.
    void _onTransformationChanged() override;

	// Called by the Transformable implementation before freezing
	// or when reverting transformations.
    void _applyTransformation() override;

	// Override EntityNode::construct()
	void construct() override;

private:
    void renderInactiveComponents(RenderableCollector& collector, const VolumeTest& volume, const bool selected) const;
    void evaluateTransform();

    // Render the light volume including bounds and origin
    void renderLightVolume(RenderableCollector& collector,
                           const Matrix4& localToWorld, bool selected) const;

    // Update the bounds of the renderable radius box
    void updateRenderableRadius() const;
}; // class LightNode

} // namespace entity
