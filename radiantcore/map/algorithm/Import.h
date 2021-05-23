#pragma once

#include <istream>
#include <string>
#include <memory>

namespace scene
{
class INode;
typedef std::shared_ptr<INode> INodePtr;
class IMapRootNode;
typedef std::shared_ptr<IMapRootNode> IMapRootNodePtr;
}

namespace map
{

class MapFormat;
typedef std::shared_ptr<MapFormat> MapFormatPtr;

namespace algorithm
{

// Integrates the map graph rooted at \p node into the global scene-graph.
void importMap(const scene::INodePtr& node);

/**
 * Ensures that all names in the foreign root node are adjusted such that
 * they don't conflict with any names in the target root's namespace, while keeping
 * all the links within the imported nodes intact.
 */
void prepareNamesForImport(const scene::IMapRootNodePtr& targetRoot, const scene::INodePtr& foreignRoot);

/**
 * Imports map objects from the given stream, inserting it into the active map.
 * De-selects the current selection before importing, selects the imported objects.
 */
void importFromStream(std::istream& stream);

/**
 * Returns a map format capable of loading the data in the given stream,
 * matching the the given extension. Since more than one map format might
 * be able to load the map data (e.g. .pfb vs .map), the extension gives this
 * method a hint about which one to prefer.
 * Passing an empty extension will consider all available formats.
 * The stream needs to support the seek method.
 * After this call, the stream is guaranteed to be rewound to the beginning
  */
MapFormatPtr determineMapFormat(std::istream& stream, const std::string& type);

/**
 * Returns a map format capable of loading the data in the given stream
 * The stream needs to support the seek method
 * After this call, the stream is guaranteed to be rewound to the beginning
  */
MapFormatPtr determineMapFormat(std::istream& stream);

struct ComparisonResult
{
    using Ptr = std::shared_ptr<ComparisonResult>;
    using Fingerprints = std::map<std::size_t, scene::INodePtr>;
    using FingerprintsByType = std::map<scene::INode::Type, Fingerprints>;

    // Represents a matching node pair
    struct Match
    {
        std::size_t fingerPrint;
        scene::INodePtr sourceNode;
        scene::INodePtr targetNode;
    };

    // The collection of nodes with the same fingerprint value, grouped by type
    std::map<scene::INode::Type, std::list<Match>> equivalentNodes;

    // The collection of nodes with differing fingerprint values, grouped by type
    FingerprintsByType differingNodes;
};

// Runs a comparison of "source" (to be merged) against the "target" (merge target)
ComparisonResult::Ptr compareGraphs(const scene::IMapRootNodePtr& source, const scene::IMapRootNodePtr& target);

}

}
