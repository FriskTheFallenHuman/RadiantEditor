#pragma once

#include "scene/Node.h"
#include "RenderableTargetLines.h"

namespace entity
{

class EntityNode;

/**
 * Non-selectable node representing one ore more connection lines between
 * entities, displayed as a line with an arrow in the middle. 
 * It is owned and managed by the EntityNode hosting the corresponding TargetKey.
 * The rationale of inserting these lines as separate node type is to prevent
 * the lines from disappearing from the view when the targeting/targeted entities
 * get culled by the space partitioning system during rendering.
 */
class TargetLineNode :
    public scene::Node
{
private:
    AABB _aabb;

    EntityNode& _owner;

    mutable RenderableTargetLines _targetLines;

public:
    TargetLineNode(EntityNode& owner);

    TargetLineNode(TargetLineNode& other);

    // Returns the type of this node
	Type getNodeType() const override;

    const AABB& localAABB() const override;

    void onPreRender(const VolumeTest& volume) override;
    void renderSolid(IRenderableCollector& collector, const VolumeTest& volumeTest) const override;
    void renderWireframe(IRenderableCollector& collector, const VolumeTest& volumeTest) const override;
    void renderHighlights(IRenderableCollector& collector, const VolumeTest& volumeTest) override;
	std::size_t getHighlightFlags() override;

protected:
    void onVisibilityChanged(bool isVisibleNow) override;

private:
    Vector3 getOwnerPosition() const;
};

}
