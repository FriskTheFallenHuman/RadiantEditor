#include "DynamicLibraryLoader.h"

#include "itextstream.h"
#include "ModuleRegistry.h"

namespace module {

	namespace {
		// This is the name of the entry point symbol in the module
		const char* const SYMBOL_REGISTER_MODULE = "RegisterModule";

		// Modules have to export a symbol of this type, which gets called during DLL loading
		typedef void (*RegisterModulesFunc)(IModuleRegistry& registry);
	}

DynamicLibraryLoader::DynamicLibraryLoader(DynamicLibraryPtr library,
										   DynamicLibraryList& targetList)
{
	assert(library); // Don't take NULL pointers

	if (!library->failed()) {
		// Library was successfully loaded, lookup the symbol
		DynamicLibrary::FunctionPointer funcPtr(
			library->findSymbol(SYMBOL_REGISTER_MODULE)
		);

		if (funcPtr != NULL) {
			// Brute-force conversion of the pointer to the desired type
			RegisterModulesFunc regFunc = reinterpret_cast<RegisterModulesFunc>(funcPtr);

			// Call the symbol and pass a reference to the ModuleRegistry
            regFunc(ModuleRegistry::Instance());

			// Add the library to the static list (for later reference)
			targetList.push_back(library);
		}
		else {
			// Symbol lookup error
            rConsoleError() << "WARNING: Could not find symbol " << SYMBOL_REGISTER_MODULE
			          << " in module " << library->getName() << ":" << std::endl;
		}
	}
	else {
        rConsoleError() << "WARNING: Failed to load module " << library->getName() << ":" << std::endl;
#ifdef __linux__
        rConsoleError() << dlerror() << std::endl;
#endif
	}
}

} // namespace module
