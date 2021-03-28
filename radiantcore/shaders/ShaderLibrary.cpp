#include "ShaderLibrary.h"

#include <iostream>
#include <utility>
#include "iimage.h"
#include "itextstream.h"
#include "ShaderTemplate.h"

namespace shaders 
{

// Insert into the definitions map, if not already present
bool ShaderLibrary::addDefinition(const std::string& name,
								  const ShaderDefinition& def)
{
	std::pair<ShaderDefinitionMap::iterator, bool> result = _definitions.insert(
		ShaderDefinitionMap::value_type(name, def)
	);

	return result.second;
}

ShaderDefinition& ShaderLibrary::getDefinition(const std::string& name)
{
	// Try to lookup the named definition
	ShaderDefinitionMap::iterator i = _definitions.find(name);

	if (i != _definitions.end())
    {
		// Return the definition
		return i->second;
	}

	// The shader definition hasn't been found, let's check if the name
	// refers to a file in the VFS
	ImagePtr img = GlobalImageLoader().imageFromVFS(name);

	if (img)
	{
		// Create a new template with this name
		ShaderTemplatePtr shaderTemplate(new ShaderTemplate(name, ""));

		// Add a diffuse layer to that template, using the given texture path
		MapExpressionPtr imgExpr(new ImageExpression(name));
		shaderTemplate->addLayer(IShaderLayer::DIFFUSE, imgExpr);

		// Take this empty shadertemplate and create a ShaderDefinition
		ShaderDefinition def(shaderTemplate,
            vfs::FileInfo("materials/", "_autogenerated_by_darkradiant_.mtr", vfs::Visibility::HIDDEN));

		// Insert the shader definition and set the iterator to it
		i = _definitions.emplace(name, def).first;

		return i->second;
	}
	else
	{
        rWarning() << "ShaderLibrary: definition not found: " << name << std::endl;

		// Create an empty template with this name
		ShaderTemplatePtr shaderTemplate(new ShaderTemplate(name, "\n"
            "\"description\"\t\"This material is missing and has been auto-generated by DarkRadiant\""));

		// Take this empty shadertemplate and create a ShaderDefinition
        // Make the definition VFS-visible to let them show in MediaBrowser (#5475)
		ShaderDefinition def(shaderTemplate,
            vfs::FileInfo("materials/", "_autogenerated_by_darkradiant_.mtr", vfs::Visibility::NORMAL));

		// Insert the shader definition and set the iterator to it
		i = _definitions.emplace(name, def).first;

		return i->second;
	}
}

bool ShaderLibrary::definitionExists(const std::string& name) const
{
	return _definitions.count(name) > 0;
}

void ShaderLibrary::copyDefinition(const std::string& nameOfOriginal, const std::string& nameOfCopy)
{
    // These need to be checked by the caller
    assert(definitionExists(nameOfOriginal));
    assert(!definitionExists(nameOfCopy));

    auto found = _definitions.find(nameOfOriginal);

    auto result = _definitions.emplace(nameOfCopy, found->second);
    result.first->second.file = vfs::FileInfo{"", "", vfs::Visibility::HIDDEN};
}

void ShaderLibrary::renameDefinition(const std::string& oldName, const std::string& newName)
{
    // These need to be checked by the caller
    assert(definitionExists(oldName));
    assert(!definitionExists(newName));

    // Rename in definition table
    auto extracted = _definitions.extract(oldName);
    extracted.key() = newName;

    _definitions.insert(std::move(extracted));

    // Rename in shaders table (if existing)
    if (_shaders.count(oldName) > 0)
    {
        auto extractedShader = _shaders.extract(oldName);
        extractedShader.key() = newName;

        // Insert it under the new name before setting the CShader instance's name
        // the observing OpenGLShader instance will request the material to reconstruct itself
        // If the new name is not present at that point, the library will create a default material.
        auto result = _shaders.insert(std::move(extractedShader));

        // Rename the CShader instance
        result.position->second->setName(newName);
    }
}

void ShaderLibrary::removeDefinition(const std::string& name)
{
    assert(definitionExists(name));

    _definitions.erase(name);
    _shaders.erase(name);
}

ShaderDefinition& ShaderLibrary::getEmptyDefinition()
{
    if (!_emptyDefinition)
    {
        auto shaderTemplate = std::make_shared<ShaderTemplate>("_emptyTemplate", "\n"
            "\"description\"\t\"This material is internal and has no corresponding declaration\"");

        _emptyDefinition = std::make_unique<ShaderDefinition>(shaderTemplate,
            vfs::FileInfo("materials/", "_autogenerated_by_darkradiant_.mtr", vfs::Visibility::HIDDEN));
    }

    return *_emptyDefinition;
}

CShaderPtr ShaderLibrary::findShader(const std::string& name)
{
	// Try to lookup the shader in the active shaders list
	ShaderMap::iterator i = _shaders.find(name);

	if (i != _shaders.end())
    {
		// A shader has been found, return its pointer
		return i->second;
	}
	else
    {
        // No shader has been found, retrieve its definition (may also be a
        // dummy def)
        ShaderDefinition& def = getDefinition(name);

        // Construct a new shader object with this def and insert it into the
        // map
        CShaderPtr shader(new CShader(name, def));

		_shaders[name] = shader;

		return shader;
	}
}

void ShaderLibrary::clear()
{
	_shaders.clear();
	_definitions.clear();
    _tables.clear();
}

std::size_t ShaderLibrary::getNumDefinitions()
{
	return _definitions.size();
}

void ShaderLibrary::foreachShaderName(const ShaderNameCallback& callback)
{
    for (const auto& pair : _definitions)
	{
        if (pair.second.file.visibility == vfs::Visibility::NORMAL)
            callback(pair.first);
	}
}

void ShaderLibrary::foreachShader(const std::function<void(const CShaderPtr&)>& func)
{
	for (const ShaderMap::value_type& pair : _shaders)
	{
        func(pair.second);
	}
}

ITableDefinition::Ptr ShaderLibrary::getTableForName(const std::string& name)
{
    auto i = _tables.find(name);

    return i != _tables.end() ? i->second : ITableDefinition::Ptr();
}

bool ShaderLibrary::addTableDefinition(const TableDefinitionPtr& def)
{
    auto result = _tables.emplace(def->getName(), def);

    return result.second;
}

} // namespace shaders
