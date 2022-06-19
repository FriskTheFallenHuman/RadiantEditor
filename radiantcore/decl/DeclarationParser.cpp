#include "DeclarationParser.h"

#include "DeclarationManager.h"

namespace decl
{

inline DeclarationBlockSyntax createBlock(const parser::BlockTokeniser::Block& block, 
    const vfs::FileInfo& fileInfo, const std::string& modName)
{
    auto spacePos = block.name.find(' ');

    DeclarationBlockSyntax syntax;

    syntax.typeName = spacePos != std::string::npos ? block.name.substr(0, spacePos) : std::string();
    syntax.name = spacePos != std::string::npos ? block.name.substr(spacePos + 1) : block.name;
    syntax.contents = block.contents;
    syntax.modName = modName;
    syntax.fileInfo = fileInfo;

    return syntax;
}

DeclarationParser::DeclarationParser(DeclarationManager& owner, Type declType, 
    const std::string& baseDir, const std::string& extension,
    const std::map<std::string, IDeclarationParser::Ptr>& parsersByTypename) :
    ThreadedDeclParser<void>(declType, baseDir, extension, 1),
    _owner(owner),
    _defaultDeclType(declType),
    _parsersByTypename(parsersByTypename)
{
    _defaultTypeParser = getParserByType(declType);

    if (!_defaultTypeParser) throw std::invalid_argument("No parser has been associated to the default type " + getTypeName(declType));
}

void DeclarationParser::parse(std::istream& stream, const vfs::FileInfo& fileInfo, const std::string& modDir)
{
    // Cut the incoming stream into declaration blocks
    parser::BasicDefBlockTokeniser<std::istream> tokeniser(stream);

    while (tokeniser.hasMoreBlocks())
    {
        auto block = tokeniser.nextBlock();

        // Convert the incoming block to a DeclarationBlockSyntax
        auto blockSyntax = createBlock(block, fileInfo, modDir);

        auto spacePos = block.name.find(' ');

        if (spacePos == std::string::npos)
        {
            // No type specified, use the default type parser
            parseBlock(*_defaultTypeParser, blockSyntax);
            continue;
        }

        // Locate a parser capable of handling that block
        auto parser = _parsersByTypename.find(block.name.substr(0, spacePos));

        if (parser != _parsersByTypename.end())
        {
            parseBlock(*parser->second, blockSyntax);
            continue;
        }

        // Unknown block type, move to buffer
        _unrecognisedBlocks.emplace_back(std::move(blockSyntax));
    }
}

void DeclarationParser::onFinishParsing()
{
    // Submit all parsed declarations to the decl manager
    _owner.onParserFinished(_defaultDeclType, std::move(_parsedDecls), std::move(_unrecognisedBlocks));
}

void DeclarationParser::parseBlock(IDeclarationParser& parser, const DeclarationBlockSyntax& block)
{
    auto declaration =  parser.parseFromBlock(block);

    auto& declMap = _parsedDecls.try_emplace(parser.getDeclType(), NamedDeclarations()).first->second;

    // Insert into map, emit a warning on duplicates
    DeclarationManager::InsertDeclaration(declMap, std::move(declaration));
}

IDeclarationParser::Ptr DeclarationParser::getParserByType(Type declType) const
{
    // Get the default type parser
    for (const auto& pair : _parsersByTypename)
    {
        if (pair.second->getDeclType() == declType)
        {
            return pair.second;
        }
    }

    return {};
}

}
