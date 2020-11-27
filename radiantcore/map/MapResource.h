#pragma once

#include "imapresource.h"
#include "imapformat.h"
#include "imapinfofile.h"
#include "imodel.h"
#include "imap.h"
#include <set>
#include "RootNode.h"
#include "os/fs.h"

namespace map
{

class MapResource :
	public IMapResource,
	public util::Noncopyable
{
private:
    scene::IMapRootNodePtr _mapRoot;

	std::string _path;
	std::string _name;

	// File extension of this resource
	std::string _extension;

public:
	// Constructor
	MapResource(const std::string& name);

	void rename(const std::string& fullPath) override;

	bool load() override;
	void save(const MapFormatPtr& mapFormat = MapFormatPtr()) override;

	const scene::IMapRootNodePtr& getRootNode() override;
    void clear() override;

	// Save the map contents to the given filename using the given MapFormat export module
	// Throws an OperationException if anything prevents successful completion
	static void saveFile(const MapFormat& format, const scene::IMapRootNodePtr& root,
						 const GraphTraversalFunc& traverse, const std::string& filename);

private:
	void mapSave();
	void onMapChanged();

	// Create a backup copy of the map (used before saving)
	bool saveBackup();

	RootNodePtr loadMapNode();
    RootNodePtr loadMapNodeFromStream(std::istream& stream, const std::string& fullPath);

	void connectMap();

	bool loadFile(std::istream& mapStream, const MapFormat& format, 
                  const RootNodePtr& root, const std::string& filename);

	void loadInfoFile(const RootNodePtr& root, const std::string& filename, const NodeIndexMap& nodeMap);
	void loadInfoFileFromStream(std::istream& infoFileStream, const RootNodePtr& root, const NodeIndexMap& nodeMap);

	// Opens a stream for the given path, which might be VFS path or an absolute one. The streamProcessor
	// function is then called with the opened stream. Throws std::runtime_error on stream open failure.
	void openFileStream(const std::string& path, const std::function<void(std::istream&)>& streamProcessor);

    // Returns the extension of the auxiliary info file (including the leading dot character)
    static std::string getInfoFileExtension();

	// Checks if file can be overwritten (throws on failure)
	static void throwIfNotWriteable(const fs::path& path);
};
// Resource pointer types
typedef std::shared_ptr<MapResource> MapResourcePtr;
typedef std::weak_ptr<MapResource> MapResourceWeakPtr;

} // namespace map
