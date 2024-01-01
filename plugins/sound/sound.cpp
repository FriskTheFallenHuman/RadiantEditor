#include "SoundManager.h"

#include "imodule.h"

extern "C" void RADIANTEDITOR_DLLEXPORT RegisterModule(IModuleRegistry& registry)
{
	module::performDefaultInitialisation(registry);

	registry.registerModule(std::make_shared<sound::SoundManager>());
}
