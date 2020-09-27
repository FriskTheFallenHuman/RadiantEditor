#pragma once

#include "imodule.h"
#include <vector>

namespace radiant
{

class ApplicationContextBase :
	public IApplicationContext
{
private:
	// The app + home paths
	std::string _appPath;
	std::string _homePath;
	std::string _settingsPath;
	std::string _cachePath;
	std::string _bitmapsPath;

	// The path where the .map files are stored
	std::string _mapsPath;

	// Command line arguments
	std::vector<std::string> _cmdLineArgs;

	// A function pointer to a global error handler, used for ASSERT_MESSAGE
	ErrorHandlingFunction _errorHandler;

protected:
	static const std::string PLUGINS_DIR; ///< name of plugins directory
	static const std::string MODULES_DIR; ///< name of modules directory

public:
	/**
	 * Initialises the context with the arguments given to main().
	 */
	virtual void initialise(int argc, char* argv[]);

    /* ApplicationContext implementation */
    virtual std::string getApplicationPath() const override;
    virtual std::vector<std::string> getLibraryPaths() const override;
    virtual std::string getRuntimeDataPath() const override;
    virtual std::string getHTMLPath() const override;
    virtual std::string getSettingsPath() const override;
	virtual std::string getCacheDataPath() const override;
    virtual std::string getBitmapsPath() const override;
    const ArgumentList& getCmdLineArgs() const override;

    const ErrorHandlingFunction& getErrorHandlingFunction() const override;

protected:
	void setErrorHandlingFunction(const ErrorHandlingFunction& function);

	// The platform-specific path where any library folders like modules/ and plugins/ are stored
	std::string getLibraryBasePath() const;

private:
	// Sets up the bitmap path and settings path
	void initPaths();

	// Initialises the arguments
	void initArgs(int argc, char* argv[]);
};

} // namespace radiant
