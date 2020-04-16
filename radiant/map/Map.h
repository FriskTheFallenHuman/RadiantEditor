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

#include "model/ScaledModelExporter.h"
#include "StartupMapLoader.h"
#include "MapPositionManager.h"

#include <sigc++/signal.h>
#include <wx/stopwatch.h>

class TextInputStream;

namespace map
{

/// Main class representing the current map
class Map :
	public IMap,
	public scene::Graph::Observer
{
	// The map name
	std::string _mapName;

	// The name of the last "save copy as" filename
	std::string _lastCopyMapName;

	// Pointer to the resource for this map
	IMapResourcePtr _resource;

	bool m_modified;

	scene::INodePtr _worldSpawnNode; // "classname" "worldspawn" !

	bool _saveInProgress;

	// A local helper object, observing the radiant module
	std::unique_ptr<StartupMapLoader> _startupMapLoader;
	ScaledModelExporter _scaledModelExporter;
	std::unique_ptr<MapPositionManager> _mapPositionManager;

    // Map save timer, for displaying "changes from last n minutes will be lost"
    // messages
    wxStopWatch _mapSaveTimer;

	MapEventSignal _mapEvent;

private:
    std::string getSaveConfirmationText() const;

public:
	Map();

	virtual MapEventSignal signal_mapEvent() const override;
	virtual const scene::INodePtr& getWorldspawn() override;
	virtual const scene::INodePtr& findOrInsertWorldspawn() override;
	virtual scene::IMapRootNodePtr getRoot() override;

	// RegisterableModule implementation
	virtual const std::string& getName() const override;
	virtual const StringSet& getDependencies() const override;
	virtual void initialiseModule(const ApplicationContext& ctx) override;
	virtual void shutdownModule() override;

	// Gets called when a node is removed from the scenegraph
	virtual void onSceneNodeErase(const scene::INodePtr& node) override;

	/** greebo: Returns true if the map has not been named yet.
	 */
	bool isUnnamed() const;

	/** greebo: Updates the name of the map (and triggers an update
	 * 			of the mainframe window title)
	 */
	void setMapName(const std::string& newName);

	/** greebo: Returns the name of this class
	 */
	std::string getMapName() const override;

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
	bool saveCopyAs();

	/** greebo: Saves the current selection to the target <filename>.
	 *
	 * @returns: true on success.
	 */
	bool saveSelected(const std::string& filename, const MapFormatPtr& mapFormat = MapFormatPtr());

	/** greebo: Loads the map from the given filename
	 */
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
	 *
	 * @returns: true on success, false on failure.
	 */
	bool saveDirect(const std::string& filename, const MapFormatPtr& mapFormat = MapFormatPtr());

	/** greebo: Creates a new map file.
	 *
	 * Note: Can't be called "new" as this is a reserved word...
	 */
	void createNew();

	void rename(const std::string& filename);

	void exportSelected(std::ostream& out);
	void exportSelected(std::ostream& out, const MapFormatPtr& format);

	// free all map elements, reinitialize the structures that depend on them
	void freeMap();

	/** greebo: Returns true if the map has unsaved changes.
	 */
	bool isModified() const;

	// Sets the modified status of this map
	void setModified(bool modifiedFlag);

	// Updates the window title of the mainframe
	void updateTitle();

	// Accessor methods for the worldspawn node
	void setWorldspawn(const scene::INodePtr& node);

	/** greebo: Returns the map format for this map
	 */
	MapFormatPtr getFormat();

	/** greebo: Asks the user if the current changes should be saved.
	 *
	 * @returns: true, if the user gave clearance (map was saved, had no
	 * 			 changes to be saved, etc.), false, if the user hit "cancel".
	 */
	bool askForSave(const std::string& title);

	/** greebo: Loads a prefab and translates it to the given target coordinates
	 */
	void loadPrefabAt(const Vector3& targetCoords);

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
	static void importMap(const cmd::ArgumentList& args);
	static void saveMap(const cmd::ArgumentList& args);
	static void saveMapAs(const cmd::ArgumentList& args);
	static void exportMap(const cmd::ArgumentList& args);

	/** greebo: Queries a filename from the user and saves a copy
	 *          of the current map to the specified filename.
	 */
	static void saveMapCopyAs(const cmd::ArgumentList& args);

	/** greebo: Asks the user for the .pfb file and imports/exports the file/selection
	 */
	static void loadPrefab(const cmd::ArgumentList& args);
	static void saveSelectedAsPrefab(const cmd::ArgumentList& args);

private:
	/**
	 * greebo: Tries to locate the worldspawn in the global scenegraph and 
	 * stores it into the local member variable.
	 * Returns the node that was found (can be an empty ptr).
	 */
	scene::INodePtr findWorldspawn();

	// Creates a fresh worldspawn node and inserts it into the root scene node
	scene::INodePtr createWorldspawn();

	void loadMapResourceFromPath(const std::string& path);

	void emitMapEvent(MapEvent ev);

}; // class Map

} // namespace map

// Accessor function for the map
// Function body defined in MapModules.cpp
map::Map& GlobalMap();
