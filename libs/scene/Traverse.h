#pragma once

#include "inode.h"
class IPatchNode;
typedef std::shared_ptr<IPatchNode> IPatchNodePtr;
class IFace;

namespace scene 
{

/** greebo: Traverses the entire scenegraph (used as entry point/TraverseFunc for map saving)
 */
void traverse(const scene::INodePtr& root, scene::NodeVisitor& nodeExporter);

/** greebo: Traverses only the selected items
 */
void traverseSelected(const scene::INodePtr& root, scene::NodeVisitor& nodeExporter);

/**
 * Visit each visible face in the global scene graph. Both the brush and the face itself
 * must not be hidden or filtered out to be visited.
 */
void foreachVisibleFace(const std::function<void(IFace&)>& functor);

/**
 * Visit each visible patch node in the global scene graph.
 */
void foreachVisiblePatch(const std::function<void(const IPatchNodePtr&)>& functor);

/** stgatilov: Returns traverser, which traverses only specified set of items (and their ancestors/descendants)
 */
std::function<void(const scene::INodePtr&, scene::NodeVisitor&)> traverseSubset(const std::set<scene::INode*>& nodes);

} // namespace
