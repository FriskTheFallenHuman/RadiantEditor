#pragma once

#include "inode.h"
#include "imap.h"
#include "imapformat.h"
#include "inamespace.h"
#include "imapresource.h"
#include "iscenegraph.h"
#include "icommandsystem.h"
#include "imodule.h"
#include "math/Vector3.h"

#include "model/export/ScaledModelExporter.h"
#include "model/export/ModelScalePreserver.h"
#include "MapPositionManager.h"
#include "messages/ApplicationShutdownRequest.h"

#include <sigc++/signal.h>
#include "time/StopWatch.h"

class TextInputStream;

namespace map
{

class ModelScalePreserver;
class DiffStatus;

/// Main class representing the current map
class Map :
	public IMap,
	public scene::Graph::Observer
{
	// The map name
	std::string _mapName;

	// The name of the last "save copy as" filename
	std::string _lastCopyMapName;

	sigc::signal<void> _mapNameChangedSignal;
	sigc::signal<void> _mapModifiedChangedSignal;

	// Pointer to the resource for this map
	IMapResourcePtr _resource;

	bool _modified;

	scene::INodePtr _worldSpawnNode; // "classname" "worldspawn" !

	bool _saveInProgress;

	// A local helper object, observing the radiant module
	ScaledModelExporter _scaledModelExporter;
	std::unique_ptr<MapPositionManager> _mapPositionManager;
	std::unique_ptr<ModelScalePreserver> _modelScalePreserver;

    // Map save timer, for displaying "changes from last n minutes will be lost"
    // messages
    util::StopWatch _mapSaveTimer;

	MapEventSignal _mapEvent;

	std::size_t _shutdownListener;

private:
    std::string getSaveConfirmationText() const;

public:
	Map();

	MapEventSignal signal_mapEvent() const override;
	const scene::INodePtr& getWorldspawn() override;
	const scene::INodePtr& findOrInsertWorldspawn() override;
	scene::IMapRootNodePtr getRoot() override;

	// RegisterableModule implementation
	const std::string& getName() const override;
	const StringSet& getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

	// Gets called when a node is removed from the scenegraph
	void onSceneNodeErase(const scene::INodePtr& node) override;

	/** greebo: Returns true if the map has not been named yet.
	 */
	bool isUnnamed() const;

	/** greebo: Updates the name of the map (and triggers an update
	 * 			of the mainframe window title)
	 */
	void setMapName(const std::string& newName);

	/** greebo: Returns the name of this map
	 */
	std::string getMapName() const override;

	sigc::signal<void>& signal_mapNameChanged() override;

	/**
	 * greebo: Saves the current map, doesn't ask for any filenames,
	 * so this has to be done before this step.
	 *
	 * It's possible to pass a mapformat to be used for saving. If the map
	 * format argument is omitted, the format corresponding to the current
	 * game type is used.
	 *
	 * @returns: TRUE if the save was successful, FALSE if an error occurred.
	 */
	bool save(const MapFormatPtr& mapFormat = MapFormatPtr());

	/**
	 * greebo: Asks the user for a new filename and saves the map if
	 * a valid filename was specified.
	 *
	 * @returns: TRUE, if the user entered a valid filename and the map was
	 * saved correctly. Returns FALSE if no valid filename was entered or
	 * an error occurred.
	 */
	bool saveAs();

	/**
	 * greebo: Saves a copy of the current map (asks for filename using
	 * a dialog window).
	 */
	void saveCopyAs();

	/** greebo: Saves the current selection to the target <filename>.
	 */
	void saveSelected(const std::string& filename, const MapFormatPtr& mapFormat = MapFormatPtr());

	// Loads the map from the given filename
    void load(const std::string& filename);

	/** greebo: Imports the contents from the given filename.
	 *
	 * @returns: true on success.
	 */
	bool import(const std::string& filename);

	/** 
	 * greebo: Exports the current map directly to the given filename.
	 * This skips any "modified" or "unnamed" checks, it just dumps
	 * the current scenegraph content to the file.
	 */
	void saveDirect(const std::string& filename, const MapFormatPtr& mapFormat = MapFormatPtr());

	void rename(const std::string& filename);

	void exportSelected(std::ostream& out) override;
	void exportSelected(std::ostream& out, const MapFormatPtr& format) override;

	// free all map elements, reinitialize the structures that depend on them
	void freeMap();

	/** greebo: Returns true if the map has unsaved changes.
	 */
	bool isModified() const override;

	// Sets the modified status of this map
	void setModified(bool modifiedFlag) override;

	sigc::signal<void>& signal_modifiedChanged() override;

	// greebo: Creates a new, empty map file.
	void createNewMap() override;

	IMapExporter::Ptr createMapExporter(IMapWriter& writer,
		const scene::IMapRootNodePtr& root, std::ostream& mapStream) override;

	// Accessor methods for the worldspawn node
	void setWorldspawn(const scene::INodePtr& node);

	/** greebo: Returns the map format for this map
	 */
	MapFormatPtr getFormat();

	/** greebo: Focus the XYViews and the Camera to the given point/angle.
	 */
	static void focusViews(const Vector3& point, const Vector3& angles);

	/** greebo: Registers the commands with the EventManager.
	 */
	void registerCommands();

	// Static command targets for connection to the EventManager
	static void exportSelection(const cmd::ArgumentList& args);
	static void newMap(const cmd::ArgumentList& args);
	static void openMap(const cmd::ArgumentList& args);
	static void openMapFromArchive(const cmd::ArgumentList& args);
	static void importMap(const cmd::ArgumentList& args);
	void saveMapCmd(const cmd::ArgumentList& args);
	static void saveMapAs(const cmd::ArgumentList& args);
	static void exportMap(const cmd::ArgumentList& args);

	/** greebo: Queries a filename from the user and saves a copy
	 *          of the current map to the specified filename.
	 */
	static void saveMapCopyAs(const cmd::ArgumentList& args);

	/** greebo: Asks the user for the .pfb file and exports the file/selection
	 */
	static void saveSelectedAsPrefab(const cmd::ArgumentList& args);

private:
	/** 
	 * greebo: Asks the user if the current changes should be saved.
	 *
	 * @returns: true, if the user gave clearance (map was saved, had no
	 * changes to be saved, etc.), false, if the user hit "cancel".
	 */
	bool askForSave(const std::string& title);

	void handleShutdownRequest(radiant::ApplicationShutdownRequest& request);

	/** greebo: Loads a prefab and translates it to the given target coordinates
	 */
	void loadPrefabAt(const cmd::ArgumentList& args);

	/**
	 * greebo: Tries to locate the worldspawn in the global scenegraph and 
	 * stores it into the local member variable.
	 * Returns the node that was found (can be an empty ptr).
	 */
	scene::INodePtr findWorldspawn();

	// Creates a fresh worldspawn node and inserts it into the root scene node
	scene::INodePtr createWorldspawn();

    // Defines a map location
    struct MapLocation
    {
        std::string path;
        bool isArchive;
        std::string archiveRelativePath;
    };
    void loadMapResourceFromLocation(const MapLocation& location);

	void loadMapResourceFromPath(const std::string& path);
	void loadMapResourceFromArchive(const std::string& archive, const std::string& archiveRelativePath);

	void emitMapEvent(MapEvent ev);

	void clearMapResource();

}; // class Map

} // namespace map

// Accessor function for the map
// Function body defined in MapModules.cpp
map::Map& GlobalMap();
