#include "RadiantTest.h"

#include "modelskin.h"
#include "scenelib.h"
#include "algorithm/Entity.h"
#include "algorithm/Scene.h"
#include "testutil/TemporaryFile.h"

namespace test
{

using ModelSkinTest = RadiantTest;

TEST_F(ModelSkinTest, FindSkins)
{
    // All of these declarations need to be parsed and present
    auto tileSkin = GlobalModelSkinCache().findSkin("tile_skin");
    EXPECT_EQ(tileSkin->getDeclName(), "tile_skin");
    EXPECT_EQ(tileSkin->getDeclFilePath(), "skins/test_skins.skin");
    EXPECT_EQ(tileSkin->getRemap("textures/atest/a"), "textures/numbers/10");

    auto separatedTileSkin = GlobalModelSkinCache().findSkin("separated_tile_skin");
    EXPECT_EQ(separatedTileSkin->getDeclName(), "separated_tile_skin");
    EXPECT_EQ(separatedTileSkin->getDeclFilePath(), "skins/test_skins.skin");
    EXPECT_EQ(separatedTileSkin->getRemap("material"), "textures/numbers/11");

    auto skinWithStrangeCasing = GlobalModelSkinCache().findSkin("skin_with_strange_casing");
    EXPECT_EQ(skinWithStrangeCasing->getDeclName(), "skin_with_strange_casing");
    EXPECT_EQ(skinWithStrangeCasing->getDeclFilePath(), "skins/test_skins.skin");
    EXPECT_EQ(skinWithStrangeCasing->getRemap("material"), "textures/numbers/11");

    auto ivyOnesided = GlobalModelSkinCache().findSkin("ivy_onesided");
    EXPECT_EQ(ivyOnesided->getDeclName(), "ivy_onesided");
    EXPECT_EQ(ivyOnesided->getDeclFilePath(), "skins/selection_test.skin");
    EXPECT_EQ(ivyOnesided->getRemap("textures/darkmod/decals/vegetation/ivy_mixed_pieces"), 
        "textures/darkmod/decals/vegetation/ivy_mixed_pieces_onesided");
}

TEST_F(ModelSkinTest, FindSkinsIsCaseInsensitive)
{
    // This is a different spelling than the one used in the decl file
    auto tileSkin = GlobalModelSkinCache().findSkin("tILE_skiN");

    EXPECT_NE(tileSkin->getDeclName(), "tILE_skiN") << "Name should not actually be the same as the one in the request";
    EXPECT_EQ(tileSkin->getDeclFilePath(), "skins/test_skins.skin");
    EXPECT_EQ(tileSkin->getRemap("textures/atest/a"), "textures/numbers/10");
}

inline bool containsRemap(const std::vector<decl::ISkin::Remapping>& remaps, 
    const std::string& original, const std::string& replacement)
{
    for (const auto& remap : remaps)
    {
        if (remap.Original == original && remap.Replacement == replacement)
        {
            return true;
        }
    }

    return false;
}

TEST_F(ModelSkinTest, GetAllRemaps)
{
    auto skin = GlobalModelSkinCache().findSkin("skin_with_wildcard");

    const auto& remaps = skin->getAllRemappings();

    EXPECT_EQ(remaps.size(), 2);
    EXPECT_TRUE(containsRemap(remaps, "textures/common/caulk", "textures/common/shadowcaulk"));
    EXPECT_TRUE(containsRemap(remaps, "*", "textures/common/nodraw"));
}

TEST_F(ModelSkinTest, GetModels)
{
    // Skin without any models listed
    auto skin = GlobalModelSkinCache().findSkin("skin_with_wildcard");
    EXPECT_EQ(skin->getModels().size(), 0);

    // Skin with 2 models
    skin = GlobalModelSkinCache().findSkin("separated_tile_skin");
    const auto& models = skin->getModels();
    EXPECT_EQ(models.size(), 2);
    EXPECT_EQ(models.count("models/ase/separated_tiles.ase"), 1);
    EXPECT_EQ(models.count("models/ase/separated_tiles22.ase"), 1);
}

TEST_F(ModelSkinTest, GetRemap)
{
    auto tileSkin = GlobalModelSkinCache().findSkin("tile_skin2");

    EXPECT_EQ(tileSkin->getRemap("textures/atest/a"), "textures/numbers/12");
    EXPECT_EQ(tileSkin->getRemap("any_other_texture"), "") << "Missing remap should return an empty string";
}

TEST_F(ModelSkinTest, GetRemapUsingWildcard)
{
    auto skin = GlobalModelSkinCache().findSkin("invisible");

    // Check the skin contains what need in this test
    EXPECT_NE(skin->getBlockSyntax().contents.find("*   textures/common/nodraw"), std::string::npos);

    EXPECT_EQ(skin->getRemap("textures/atest/a"), "textures/common/nodraw") << "Skin should always return nodraw";
    EXPECT_EQ(skin->getRemap("any_other_texture"), "textures/common/nodraw") << "Skin should always return nodraw";
}

TEST_F(ModelSkinTest, GetRemapIsRespectingDeclarationOrder)
{
    auto skin = GlobalModelSkinCache().findSkin("skin_with_wildcard");

    EXPECT_EQ(skin->getRemap("textures/common/caulk"), "textures/common/shadowcaulk") << "Skin should respond to specific material first";
    EXPECT_EQ(skin->getRemap("any_other_texture"), "textures/common/nodraw") << "Skin should return nodraw for the rest";
}

inline void expectSkinIsListed(const StringList& skins, const std::string& expectedSkin)
{
    EXPECT_NE(std::find(skins.begin(), skins.end(), expectedSkin), skins.end())
        << "Couldn't find the expected skin " << expectedSkin << " in the list";
}

TEST_F(ModelSkinTest, FindMatchingSkins)
{
    auto separatedSkins = GlobalModelSkinCache().getSkinsForModel("models/ase/separated_tiles.ase");
    EXPECT_EQ(separatedSkins.size(), 1);
    EXPECT_EQ(separatedSkins.at(0), "separated_tile_skin");

    auto tileSkins = GlobalModelSkinCache().getSkinsForModel("models/ase/tiles.ase");
    EXPECT_EQ(tileSkins.size(), 2);
    expectSkinIsListed(tileSkins, "tile_skin");
    expectSkinIsListed(tileSkins, "tile_skin2");
}

TEST_F(ModelSkinTest, GetAllSkins)
{
    auto allSkins = GlobalModelSkinCache().getAllSkins();
    expectSkinIsListed(allSkins, "tile_skin");
    expectSkinIsListed(allSkins, "tile_skin2");
    expectSkinIsListed(allSkins, "separated_tile_skin");
    expectSkinIsListed(allSkins, "skin_with_strange_casing");
    expectSkinIsListed(allSkins, "ivy_onesided");
}

TEST_F(ModelSkinTest, ReloadDeclsRefreshesModels)
{
    // Create a temporary file holding a new skin
    TemporaryFile tempFile(_context.getTestProjectPath() + "skins/_skin_decl_test.skin");
    tempFile.setContents(R"(
skin temporary_skin
{
    model               models/ase/tiles.ase
    textures/atest/a    textures/common/caulk
}
)");

    // Load that new declaration
    GlobalDeclarationManager().reloadDeclarations();

    EXPECT_TRUE(GlobalModelSkinCache().findSkin("temporary_skin"));

    // Create a model and insert it into the scene
    auto funcStaticClass = GlobalEntityClassManager().findClass("func_static");
    auto funcStatic = GlobalEntityModule().createEntity(funcStaticClass);
    scene::addNodeToContainer(funcStatic, GlobalMapModule().getRoot());

    // Set model and skin spawnargs
    funcStatic->getEntity().setKeyValue("model", "models/ase/tiles.ase");
    funcStatic->getEntity().setKeyValue("skin", "temporary_skin");

    // Find the child model node
    auto model = algorithm::findChildModel(funcStatic);
    EXPECT_EQ(model->getIModel().getModelPath(), "models/ase/tiles.ase");

    // Check the contents of the materials list
    auto activeMaterials = model->getIModel().getActiveMaterials();
    EXPECT_EQ(activeMaterials.size(), 1);
    EXPECT_EQ(activeMaterials.at(0), "textures/common/caulk");

    // Change the remap to noclip
    tempFile.setContents(R"(
skin temporary_skin
{
    model               models/ase/tiles.ase
    textures/atest/a    textures/common/noclip
}
)");

    GlobalDeclarationManager().reloadDeclarations();

    activeMaterials = model->getIModel().getActiveMaterials();
    EXPECT_EQ(activeMaterials.size(), 1);
    EXPECT_EQ(activeMaterials.at(0), "textures/common/noclip");
}

// A slight variant of the above ReloadDeclsRefreshesModels: in this test the
// skin body is changed manually (similar to what happens during reloadDecls)
// and the declsReloaded signal is fired manually by the test code.
// This is to hide the dominant entityDefs-reloaded signal that is making
// the above test green without even without any skin signal listening code
TEST_F(ModelSkinTest, ReloadDeclsRefreshesModelsUsingSignal)
{
    // Create a model and insert it into the scene
    auto funcStaticClass = GlobalEntityClassManager().findClass("func_static");
    auto funcStatic = GlobalEntityModule().createEntity(funcStaticClass);
    scene::addNodeToContainer(funcStatic, GlobalMapModule().getRoot());

    // Set model and skin spawnargs
    funcStatic->getEntity().setKeyValue("model", "models/ase/tiles.ase");
    funcStatic->getEntity().setKeyValue("skin", "tile_skin");

    // Find the child model node
    auto model = algorithm::findChildModel(funcStatic);
    EXPECT_EQ(model->getIModel().getModelPath(), "models/ase/tiles.ase");

    // Check the contents of the materials list
    auto activeMaterials = model->getIModel().getActiveMaterials();
    EXPECT_EQ(activeMaterials.size(), 1);
    EXPECT_EQ(activeMaterials.at(0), "textures/numbers/10");

    // Inject a new block syntax into the existing skin declaration
    auto skin = GlobalModelSkinCache().findSkin("tile_skin");
    decl::DeclarationBlockSyntax syntax;

    syntax.name = skin->getDeclName();
    syntax.typeName = "skin";
    syntax.contents = R"(
    model               models/ase/tiles.ase
    textures/atest/a    textures/common/noclip
)";
    skin->setBlockSyntax(syntax);

    // Create an artificial skins-reloaded event
    GlobalDeclarationManager().signal_DeclsReloaded(decl::Type::Skin).emit();

    activeMaterials = model->getIModel().getActiveMaterials();
    EXPECT_EQ(activeMaterials.size(), 1);
    EXPECT_EQ(activeMaterials.at(0), "textures/common/noclip");
}

}
