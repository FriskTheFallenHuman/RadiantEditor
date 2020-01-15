#include "Quake4MapReader.h"

#include "i18n.h"
#include "itextstream.h"

#include "mapdoom3/primitiveparsers/BrushDef.h"
#include "mapdoom3/primitiveparsers/BrushDef3.h"
#include "mapdoom3/primitiveparsers/PatchDef2.h"
#include "mapdoom3/primitiveparsers/PatchDef3.h"

#include <fmt/format.h>

namespace map
{

Quake4MapReader::Quake4MapReader(IMapImportFilter& importFilter) :
		Doom3MapReader(importFilter)
{}

void Quake4MapReader::initPrimitiveParsers()
{
	if (_primitiveParsers.empty())
	{
		addPrimitiveParser(PrimitiveParserPtr(new BrushDefParser));
		addPrimitiveParser(PrimitiveParserPtr(new BrushDef3ParserQuake4));
		addPrimitiveParser(PrimitiveParserPtr(new PatchDef2Parser));
		addPrimitiveParser(PrimitiveParserPtr(new PatchDef3Parser));
	}
}

void Quake4MapReader::parseMapVersion(parser::DefTokeniser& tok)
{
	// Parse the map version
	float version = 0;

	try
	{
		tok.assertNextToken("Version");
		version = std::stof(tok.nextToken());
	}
	catch (parser::ParseException& e)
	{
		// failed => quit
		rError() << "[mapdoom3] Unable to parse map version: "
			<< e.what() << std::endl;

		throw FailureException(_("Unable to parse map version (parse exception)."));
	}
	catch (std::invalid_argument& e)
	{
		rError() << "[mapdoom3] Unable to parse map version: "
			<< e.what() << std::endl;

		throw FailureException(_("Could not recognise map version number format."));
	}

	float requiredVersion = MAP_VERSION_Q4;

	// Check we have the correct version for this module
	if (version != requiredVersion)
	{
		std::string errMsg = fmt::format(_("Incorrect map version: required {0:f}, found {1:f}"), requiredVersion, version);

		rError() << errMsg << std::endl;

		throw FailureException(errMsg);
	}
}

} // namespace
