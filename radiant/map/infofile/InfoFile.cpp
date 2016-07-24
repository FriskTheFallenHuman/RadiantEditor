#include "InfoFile.h"

#include <limits>
#include "itextstream.h"
#include "imapinfofile.h"
#include "string/convert.h"

#include "i18n.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

namespace map
{

const char* const InfoFile::HEADER_SEQUENCE = "DarkRadiant Map Information File Version";
const char* const InfoFile::NODE_TO_LAYER_MAPPING = "NodeToLayerMapping";
const char* const InfoFile::LAYER = "Layer";
const char* const InfoFile::LAYERS = "Layers";
const char* const InfoFile::NODE = "Node";

// Pass the input stream to the constructor
InfoFile::InfoFile(std::istream& infoStream) :
	_tok(infoStream),
	_isValid(true)
{
	_standardLayerList.insert(0);
}

const InfoFile::LayerNameMap& InfoFile::getLayerNames() const {
	return _layerNames;
}

std::size_t InfoFile::getLayerMappingCount() const {
	return _layerMappings.size();
}

const scene::LayerList& InfoFile::getNextLayerMapping() {
	// Check if we have a valid infofile
	if (!_isValid) {
		return _standardLayerList;
	}

	// Check if the node index is out of bounds
	if (_layerMappingIterator == _layerMappings.end()) {
		return _standardLayerList;
	}

	// Return the current list and increase the iterator afterwards
	return *(_layerMappingIterator++);
}

void InfoFile::parse()
{
	GlobalMapInfoFileManager().foreachModule([](IMapInfoFileModule& module)
	{
		module.onInfoFileLoadStart();
	});

	// parse the header
	try
	{
		std::vector<std::string> parts;
		boost::algorithm::split(parts, HEADER_SEQUENCE, boost::algorithm::is_any_of(" "));

		// Parse the string "DarkRadiant Map Information File Version"
		for (std::size_t i = 0; i < parts.size(); i++)
		{
			_tok.assertNextToken(parts[i]);
		}

		float version = boost::lexical_cast<float>(_tok.nextToken());

		if (version != MAP_INFO_VERSION) 
		{
			_isValid = false;
			throw parser::ParseException(_("Map Info File Version invalid"));
		}
	}
	catch (parser::ParseException& e)
	{
        rError() << "[InfoFile] Unable to parse info file header: " << e.what() << std::endl;
		_isValid = false;
        return;
    }
    catch (boost::bad_lexical_cast& e)
	{
        rError() << "[InfoFile] Unable to parse info file version: " << e.what() << std::endl;
		_isValid = false;
        return;
    }

	// The opening brace of the master block
	_tok.assertNextToken("{");

	parseInfoFileBody();

	// Set the layer mapping iterator to the beginning
	_layerMappingIterator = _layerMappings.begin();
}

void InfoFile::parseInfoFileBody()
{
	while (_tok.hasMoreTokens())
	{
		std::string token = _tok.nextToken();

		bool blockParsed = false;

		// Send each block to the modules that are able to load it
		GlobalMapInfoFileManager().foreachModule([&](IMapInfoFileModule& module)
		{
			if (!blockParsed && module.canParseBlock(token))
			{
				module.parseBlock(token, _tok);
				blockParsed = true;
			}
		});

		if (blockParsed)
		{
			continue; // block was processed by a module
		}

		if (token == LAYERS)
		{
			parseLayerNames();
			continue;
		}

		if (token == NODE_TO_LAYER_MAPPING)
		{
			parseNodeToLayerMapping();
			continue;
		}

		if (token == "}")
		{
			break;
		}

		// Unknown token, try to ignore that block
		rWarning() << "Unknown keyword " << token << " encountered, will try to ignore this block." << std::endl;

		// We can only ignore a block if there is a block beginning curly brace
		_tok.assertNextToken("{");

		// Ignore the block
		int depth = 1;

		while (_tok.hasMoreTokens() && depth > 0)
		{
			std::string token2 = _tok.nextToken();

			if (token2 == "{") 
			{
				depth++;
			}
			else if (token2 == "}") 
			{
				depth--;
			}
		}
	}
}

void InfoFile::parseLayerNames()
{
	// The opening brace
	_tok.assertNextToken("{");

	while (_tok.hasMoreTokens()) {
		std::string token = _tok.nextToken();

		if (token == LAYER) {
			// Get the ID
			std::string layerIDStr = _tok.nextToken();
			int layerID = string::convert<int>(layerIDStr);

			_tok.assertNextToken("{");

			// Assemble the name
			std::string name;

			token = _tok.nextToken();
			while (token != "}") {
				name += token;
				token = _tok.nextToken();
			}

			rMessage() << "[InfoFile]: Parsed layer #"
				<< layerID << " with name " << name << std::endl;

			_layerNames.insert(LayerNameMap::value_type(layerID, name));

			continue;
		}

		if (token == "}") {
			break;
		}
	}
}

void InfoFile::parseNodeToLayerMapping()
{
	// The opening brace
	_tok.assertNextToken("{");

	while (_tok.hasMoreTokens()) {
		std::string token = _tok.nextToken();

		if (token == NODE) {
			_tok.assertNextToken("{");

			// Create a new LayerList
			_layerMappings.push_back(scene::LayerList());

			while (_tok.hasMoreTokens()) {
				std::string nodeToken = _tok.nextToken();

				if (nodeToken == "}") {
					break;
				}

				// Add the ID to the list
				_layerMappings.back().insert(string::convert<int>(nodeToken));
			}
		}

		if (token == "}") {
			break;
		}
	}
}

} // namespace map
