#pragma once

#include "module/ApplicationContextBase.h"
#include "os/fs.h"
#include "os/dir.h"
#include "debugging/debugging.h"

namespace radiant
{

/**
 * Application context implementation used in DarkRadiant's unit tests
 */
class TestContext :
	public radiant::ApplicationContextBase
{
private:
	std::string _settingsFolder;

public:
	TestContext()
	{
		// Set up the temporary settings folder
		auto settingsFolder = os::getTemporaryPath() / "dr_temp_settings";

		_settingsFolder = os::standardPathWithSlash(settingsFolder.string());

		os::removeDirectory(_settingsFolder);
		os::makeDirectory(_settingsFolder);

		setErrorHandlingFunction([&](const std::string& title, const std::string& message)
		{
			std::cerr << "Fatal error " << title << "\n" << message << std::endl;
			DEBUGGER_BREAKPOINT();
		});
	}

	~TestContext()
	{
		if (!_settingsFolder.empty())
		{
			os::removeDirectory(_settingsFolder);
		}
	}

    // Returns the path to the test/resources/tdm/ folder shipped with the DR sources
    virtual std::string getTestProjectPath() const
    {
        return getTestResourcePath() + "tdm/";
    }

	virtual std::string getTestResourcePath() const
	{
#if defined(POSIX) 
	#if defined(TESTRESOURCEDIR) && !defined(ENABLE_RELOCATION)
		fs::path testResourcePath(TESTRESOURCEDIR);
	#else
		// make check will compile the test binary to $top_builddir/test/.libs/
		fs::path testResourcePath = getApplicationPath();
		testResourcePath /= "../../test/resources/";
	#endif
#else
		fs::path testResourcePath = getApplicationPath();
		testResourcePath /= "../test/resources/";
#endif
		return os::standardPathWithSlash(testResourcePath.string());
	}

	std::string getSettingsPath() const override
	{
		return _settingsFolder;
	}

	std::vector<std::string> getLibraryPaths() const override
	{
		auto libBasePath = os::standardPathWithSlash(getLibraryBasePath());

		// Don't load modules from the plugins/ folder, as these
		// are relying on a working UI. For the test environment
		// we are only interested in non-UI modules, for now at least.
		return
		{
			libBasePath + MODULES_DIR,
		};
	}
};

}
