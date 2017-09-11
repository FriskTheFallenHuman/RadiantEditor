#pragma once

#include <list>
#include "iarchive.h"
#include "ifilesystem.h"

#define VFS_MAXDIRS 8

class Doom3FileSystem :
	public VirtualFileSystem
{
private:
	std::string _directories[VFS_MAXDIRS];
	int _numDirectories;
	std::set<std::string> _allowedExtensions;
	std::set<std::string> _allowedExtensionsDir;

	struct ArchiveDescriptor
	{
		std::string name;
		ArchivePtr archive;
		bool is_pakfile;
	};

	typedef std::list<ArchiveDescriptor> ArchiveList;
	ArchiveList _archives;

	typedef std::set<Observer*> ObserverList;
	ObserverList _observers;

public:
	// Constructor
	Doom3FileSystem();

	void initDirectory(const std::string& path) override;
	void initialise() override;
	void shutdown() override;

	int getFileCount(const std::string& filename) override;
	ArchiveFilePtr openFile(const std::string& filename) override;
	ArchiveTextFilePtr openTextFile(const std::string& filename) override;

    ArchiveFilePtr openFileInAbsolutePath(const std::string& filename) override;
    ArchiveTextFilePtr openTextFileInAbsolutePath(const std::string& filename) override;

	// Call the specified callback function for each file matching extension
	// inside basedir.
    void forEachFile(const std::string& basedir, const std::string& extension,
                     const VisitorFunc& visitorFunc, std::size_t depth) override;

    void forEachFileInAbsolutePath(const std::string& path,
                                   const std::string& extension,
                                   const VisitorFunc& visitorFunc,
                                   std::size_t depth = 1) override;

	std::string findFile(const std::string& name) override;
	std::string findRoot(const std::string& name) override;

	void addObserver(Observer& observer) override;
	void removeObserver(Observer& observer) override;

	// RegisterableModule implementation
	const std::string& getName() const override;
	const StringSet& getDependencies() const override;
	void initialiseModule(const ApplicationContext& ctx) override;
	void shutdownModule() override;

private:
	void initPakFile(ArchiveLoader& archiveModule, const std::string& filename);
};
typedef std::shared_ptr<Doom3FileSystem> Doom3FileSystemPtr;
