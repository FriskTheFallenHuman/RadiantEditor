#include "RadiantTest.h"

#include "ishaders.h"
#include <algorithm>
#include "string/split.h"
#include "string/case_conv.h"
#include "string/trim.h"
#include "string/join.h"

namespace test
{

using MaterialsTest = RadiantTest;

constexpr double TestEpsilon = 0.0001;

TEST_F(MaterialsTest, MaterialFileInfo)
{
    auto& materialManager = GlobalMaterialManager();

    // Expect our example material definitions in the ShaderLibrary
    EXPECT_TRUE(materialManager.materialExists("textures/orbweaver/drain_grille"));
    EXPECT_TRUE(materialManager.materialExists("models/md5/chars/nobles/noblewoman/noblebottom"));
    EXPECT_TRUE(materialManager.materialExists("tdm_spider_black"));

    // ShaderDefinitions should contain their source file infos
    const auto& drainGrille = materialManager.getMaterial("textures/orbweaver/drain_grille");
    EXPECT_EQ(drainGrille->getShaderFileInfo().name, "example.mtr");
    EXPECT_EQ(drainGrille->getShaderFileInfo().visibility, vfs::Visibility::NORMAL);

    const auto& nobleTop = materialManager.getMaterial("models/md5/chars/nobles/noblewoman/nobletop");
    EXPECT_EQ(nobleTop->getShaderFileInfo().name, "tdm_ai_nobles.mtr");
    EXPECT_EQ(nobleTop->getShaderFileInfo().visibility, vfs::Visibility::NORMAL);

    // Visibility should be parsed from assets.lst
    const auto& hiddenTex = materialManager.getMaterial("textures/orbweaver/drain_grille_h");
    EXPECT_EQ(hiddenTex->getShaderFileInfo().name, "hidden.mtr");
    EXPECT_EQ(hiddenTex->getShaderFileInfo().visibility, vfs::Visibility::HIDDEN);

    // assets.lst visibility applies to the MTR file, and should propagate to
    // all shaders within it
    const auto& hiddenTex2 = materialManager.getMaterial("textures/darkmod/another_white");
    EXPECT_EQ(hiddenTex2->getShaderFileInfo().name, "hidden.mtr");
    EXPECT_EQ(hiddenTex2->getShaderFileInfo().visibility, vfs::Visibility::HIDDEN);
}

TEST_F(MaterialsTest, MaterialParser)
{
    auto& materialManager = GlobalMaterialManager();

    // All of these materials need to be present
    // variant3 lacks whitespace between its name and {, which caused trouble in #4900
    EXPECT_TRUE(materialManager.materialExists("textures/parsing_test/variant1"));
    EXPECT_TRUE(materialManager.materialExists("textures/parsing_test/variant2"));
    EXPECT_TRUE(materialManager.materialExists("textures/parsing_test/variant3"));
}

TEST_F(MaterialsTest, EnumerateMaterialLayers)
{
    auto material = GlobalMaterialManager().getMaterial("tdm_spider_black");
    EXPECT_TRUE(material);

    // Get a list of all layers in the material
    auto layers = material->getAllLayers();
    EXPECT_EQ(layers.size(), 5);

    // First layer is the bump map in this particular material
    EXPECT_EQ(layers.at(0)->getType(), IShaderLayer::BUMP);
    EXPECT_EQ(layers.at(0)->getMapImageFilename(),
              "models/md5/chars/monsters/spider/spider_local");

    // Second layer is the diffuse map
    EXPECT_EQ(layers.at(1)->getType(), IShaderLayer::DIFFUSE);
    EXPECT_EQ(layers.at(1)->getMapImageFilename(),
              "models/md5/chars/monsters/spider_black");

    // Third layer is the specular map
    EXPECT_EQ(layers.at(2)->getType(), IShaderLayer::SPECULAR);
    EXPECT_EQ(layers.at(2)->getMapImageFilename(),
              "models/md5/chars/monsters/spider_s");

    // Fourth layer is the additive "ambient method" stage
    EXPECT_EQ(layers.at(3)->getType(), IShaderLayer::BLEND);
    EXPECT_EQ(layers.at(3)->getMapImageFilename(),
              "models/md5/chars/monsters/spider_black");
    BlendFunc bf4 = layers.at(3)->getBlendFunc();
    EXPECT_EQ(bf4.src, GL_ONE);
    EXPECT_EQ(bf4.dest, GL_ONE);

    // Fifth layer is another additive stage with a VFP
    EXPECT_EQ(layers.at(4)->getType(), IShaderLayer::BLEND);
    EXPECT_EQ(layers.at(4)->getNumFragmentMaps(), 4);
    BlendFunc bf5 = layers.at(4)->getBlendFunc();
    EXPECT_EQ(bf5.src, GL_ONE);
    EXPECT_EQ(bf5.dest, GL_ONE);
}

TEST_F(MaterialsTest, IdentifyAmbientLight)
{
    auto ambLight = GlobalMaterialManager().getMaterial("lights/ambientLight");
    ASSERT_TRUE(ambLight);
    EXPECT_TRUE(ambLight->isAmbientLight());

    auto pointLight = GlobalMaterialManager().getMaterial("lights/defaultPointLight");
    ASSERT_TRUE(pointLight);
    EXPECT_FALSE(pointLight->isAmbientLight());

    auto nonLight = GlobalMaterialManager().getMaterial("tdm_spider_black");
    ASSERT_TRUE(nonLight);
    EXPECT_FALSE(nonLight->isAmbientLight());
}

TEST_F(MaterialsTest, MaterialTableLookup)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/expressions/sinTableLookup");

    auto stage = material->getAllLayers().front();

    // Set time to 5008 seconds, this is the value I happened to run into when debugging this in the engine
    stage->evaluateExpressions(5008);

    EXPECT_FLOAT_EQ(stage->getAlphaTest(), -0.00502608204f);

    material = GlobalMaterialManager().getMaterial("textures/parsertest/expressions/cosTableLookup");

    stage = material->getAllLayers().front();
    stage->evaluateExpressions(1000);

    EXPECT_FLOAT_EQ(stage->getAlphaTest(), 0.999998093f);
}

TEST_F(MaterialsTest, MaterialRotationEvaluation)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/expressions/rotationCalculation");

    auto& stage = material->getAllLayers().front();

    // Set time to 5008 seconds, this is the value I happened to run into when debugging this in the engine
    stage->evaluateExpressions(5008);

    auto expectedMatrix = Matrix4::byRows(
        0.999998033,    0.000158024632, 0,  0,
        -0.000158024632, 0.999998033, 0, 0,
        0,              0,              1,  0,
        0,              0,              0,  1
    );
    EXPECT_TRUE(stage->getTextureTransform().isEqual(expectedMatrix, TestEpsilon)) << "Expected:\n " << expectedMatrix << " but got:\n  " << stage->getTextureTransform();
}

TEST_F(MaterialsTest, MaterialParserPolygonOffset)
{
    auto& materialManager = GlobalMaterialManager();

    auto polygonOffset1 = materialManager.getMaterial("textures/parsertest/polygonOffset1");

    EXPECT_TRUE(polygonOffset1->getMaterialFlags() & Material::FLAG_POLYGONOFFSET);
    EXPECT_EQ(polygonOffset1->getPolygonOffset(), 1.0f) << "Default value of polygonOffset should be 1.0";

    auto polygonOffset2 = materialManager.getMaterial("textures/parsertest/polygonOffset2");

    EXPECT_TRUE(polygonOffset2->getMaterialFlags() & Material::FLAG_POLYGONOFFSET);
    EXPECT_EQ(polygonOffset2->getPolygonOffset(), 13.0f);

    auto polygonOffset3 = materialManager.getMaterial("textures/parsertest/polygonOffset3");

    EXPECT_TRUE(polygonOffset3->getMaterialFlags() & Material::FLAG_POLYGONOFFSET);
    EXPECT_EQ(polygonOffset3->getPolygonOffset(), -3.0f);
}
TEST_F(MaterialsTest, MaterialParserSortRequest)
{
    auto& materialManager = GlobalMaterialManager();

    auto material = materialManager.getMaterial("textures/parsertest/sortPredefined_none");
    EXPECT_FALSE(material->getParseFlags() & Material::PF_HasSortDefined);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_subview");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_SUBVIEW);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_opaque");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_OPAQUE);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_decal");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_DECAL);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_far");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_FAR);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_medium");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_MEDIUM);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_close");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_CLOSE);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_almostnearest");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_ALMOST_NEAREST);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_nearest");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_NEAREST);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_afterfog");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_AFTER_FOG);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_postprocess");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_POST_PROCESS);

    material = materialManager.getMaterial("textures/parsertest/sortPredefined_portalsky");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_EQ(material->getSortRequest(), Material::SORT_PORTAL_SKY);
    
    material = materialManager.getMaterial("textures/parsertest/sortPredefined_decal_macro");
    EXPECT_FALSE(material->getParseFlags() & Material::PF_HasSortDefined); // sort is not explicitly set
    EXPECT_EQ(material->getSortRequest(), Material::SORT_DECAL);

    material = materialManager.getMaterial("textures/parsertest/sort_custom");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_FLOAT_EQ(material->getSortRequest(), 34.56f);

    material = materialManager.getMaterial("textures/parsertest/sort_custom2");
    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSortDefined);
    EXPECT_FLOAT_EQ(material->getSortRequest(), 34.56f);

    // Update the sort request
    material->setSortRequest(78.45f);
    EXPECT_FLOAT_EQ(material->getSortRequest(), 78.45f);
}

TEST_F(MaterialsTest, MaterialParserAmbientRimColour)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/withAmbientRimColor");

    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasAmbientRimColour);
    // no further support for ambientRimColor at this point
}

TEST_F(MaterialsTest, MaterialParserSpectrum)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/nospectrum");

    EXPECT_FALSE(material->getParseFlags() & Material::PF_HasSpectrum);
    EXPECT_EQ(material->getSpectrum(), 0);

    material = GlobalMaterialManager().getMaterial("textures/parsertest/spectrumMinus45");

    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSpectrum);
    EXPECT_EQ(material->getSpectrum(), -45);

    material = GlobalMaterialManager().getMaterial("textures/parsertest/spectrumPositive100");

    EXPECT_TRUE(material->getParseFlags() & Material::PF_HasSpectrum);
    EXPECT_EQ(material->getSpectrum(), 100);
}

inline void checkRenderBumpArguments(const std::string& materialName, const std::string& keyword)
{
    auto material = GlobalMaterialManager().getMaterial(materialName);

    auto fullDefinition = material->getDefinition();
    std::vector<std::string> lines;
    string::split(lines, fullDefinition, "\n", true);

    std::string keywordLower = string::to_lower_copy(keyword);
    auto renderBumpLine = std::find_if(lines.begin(), lines.end(),
        [&](const std::string& line) { return string::to_lower_copy(line).find(keywordLower) != std::string::npos; });

    EXPECT_NE(renderBumpLine, lines.end());

    std::string line = *renderBumpLine;
    auto args = line.substr(string::to_lower_copy(line).find(keywordLower) + keywordLower.size());

    string::trim(args);

    if (keyword == "renderbump")
    {
        EXPECT_EQ(material->getRenderBumpArguments(), args);
    }
    else
    {
        EXPECT_EQ(material->getRenderBumpFlatArguments(), args);
    }
}

TEST_F(MaterialsTest, MaterialParserRenderbump)
{
    checkRenderBumpArguments("textures/parsertest/renderBump1", "renderbump");
    checkRenderBumpArguments("textures/parsertest/renderBump2", "renderbump");
    checkRenderBumpArguments("textures/parsertest/renderBump3", "renderbump");
    checkRenderBumpArguments("textures/parsertest/renderBump4", "renderbump");
    checkRenderBumpArguments("textures/parsertest/renderBump5", "renderbump");
    checkRenderBumpArguments("textures/parsertest/renderBump6", "renderbump");
}

TEST_F(MaterialsTest, MaterialParserRenderbumpFlat)
{
    checkRenderBumpArguments("textures/parsertest/renderBumpFlat1", "renderbumpflat");
    checkRenderBumpArguments("textures/parsertest/renderBumpFlat2", "renderbumpflat");
}

TEST_F(MaterialsTest, MaterialParserDeform)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/deform1");
    EXPECT_EQ(material->getDeformType(), Material::DEFORM_FLARE);
    EXPECT_EQ(material->getDeformDeclName(), "");
    EXPECT_EQ(material->getDeformExpression(0)->getExpressionString(), "1.5");
    EXPECT_FALSE(material->getDeformExpression(1));
    EXPECT_FALSE(material->getDeformExpression(2));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/deform2");
    EXPECT_EQ(material->getDeformType(), Material::DEFORM_EXPAND);
    EXPECT_EQ(material->getDeformDeclName(), "");
    EXPECT_EQ(material->getDeformExpression(0)->getExpressionString(), "(0.1 * deformtesttable[time * (0.3 + time)] - global3)");
    EXPECT_FALSE(material->getDeformExpression(1));
    EXPECT_FALSE(material->getDeformExpression(2));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/deform3");
    EXPECT_EQ(material->getDeformType(), Material::DEFORM_MOVE);
    EXPECT_EQ(material->getDeformDeclName(), "");
    EXPECT_EQ(material->getDeformExpression(0)->getExpressionString(), "(1.7 + time + 4.0 - global3)");
    EXPECT_FALSE(material->getDeformExpression(1));
    EXPECT_FALSE(material->getDeformExpression(2));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/deform4");
    EXPECT_EQ(material->getDeformType(), Material::DEFORM_TURBULENT);
    EXPECT_EQ(material->getDeformDeclName(), "deformtesttable");
    EXPECT_EQ(material->getDeformExpression(0)->getExpressionString(), "time * 2.0");
    EXPECT_EQ(material->getDeformExpression(1)->getExpressionString(), "(parm11 - 4.0)");
    EXPECT_EQ(material->getDeformExpression(2)->getExpressionString(), "-1.0 * global5");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/deform5");
    EXPECT_EQ(material->getDeformType(), Material::DEFORM_PARTICLE);
    EXPECT_EQ(material->getDeformDeclName(), "testparticle");
    EXPECT_FALSE(material->getDeformExpression(0));
    EXPECT_FALSE(material->getDeformExpression(1));
    EXPECT_FALSE(material->getDeformExpression(2));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/deform6");
    EXPECT_EQ(material->getDeformType(), Material::DEFORM_PARTICLE2);
    EXPECT_EQ(material->getDeformDeclName(), "testparticle");
    EXPECT_FALSE(material->getDeformExpression(0));
    EXPECT_FALSE(material->getDeformExpression(1));
    EXPECT_FALSE(material->getDeformExpression(2));
}

TEST_F(MaterialsTest, MaterialParserStageNotransform)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/notransform");
    auto stage = material->getAllLayers().front();

    EXPECT_EQ(stage->getTransformations().size(), 0);
    EXPECT_TRUE(stage->getTextureTransform() == Matrix4::getIdentity());
}

TEST_F(MaterialsTest, MaterialParserStageTranslate)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/translation1");
    auto stage = material->getAllLayers().front();

    EXPECT_EQ(stage->getTransformations().size(), 1);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "3.0");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "parm3 + 5.0");

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::getTranslation(Vector3(3.0, 5.0, 0)), TestEpsilon));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/translation2");
    stage = material->getAllLayers().front();

    EXPECT_EQ(stage->getTransformations().size(), 1);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "time");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "0.5");

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::getTranslation(Vector3(1.0, 0.5, 0)), TestEpsilon));
}

TEST_F(MaterialsTest, MaterialParserStageRotate)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/rotate1");
    auto stage = material->getAllLayers().front();

    EXPECT_EQ(stage->getTransformations().size(), 1);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Rotate);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "0.03");
    EXPECT_FALSE(stage->getTransformations().at(0).expression2);

    // sintable and costable lookups are [0..1], translate them to [0..2pi]
    auto cosValue = cos(0.03 * 2 * c_pi);
    auto sinValue = sin(0.03 * 2 * c_pi);

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::byRows(
        cosValue, -sinValue, 0, (-0.5*cosValue + 0.5*sinValue) + 0.5,
        sinValue,  cosValue, 0, (-0.5*sinValue - 0.5*cosValue) + 0.5,
        0, 0, 1, 0,
        0, 0, 0, 1
    ), TestEpsilon));
}

TEST_F(MaterialsTest, MaterialParserStageScale)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/scale1");
    auto stage = material->getAllLayers().front();

    EXPECT_EQ(stage->getTransformations().size(), 1);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Scale);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "4.0");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "time * 3.0");

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::byRows(
        4, 0, 0, 0,
        0, 3, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    ), TestEpsilon));
}

TEST_F(MaterialsTest, MaterialParserStageCenterScale)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/centerscale1");
    auto stage = material->getAllLayers().front();

    EXPECT_EQ(stage->getTransformations().size(), 1);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::CenterScale);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "4.0");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "time * 3.0");

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::byRows(
        4, 0, 0, 0.5 - 0.5 * 4,
        0, 3, 0, 0.5 - 0.5 * 3,
        0, 0, 1, 0,
        0, 0, 0, 1
    ), TestEpsilon));
}

TEST_F(MaterialsTest, MaterialParserStageShear)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/shear1");
    auto stage = material->getAllLayers().front();
    EXPECT_EQ(stage->getTransformations().size(), 1);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Shear);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "global3 + 5.0");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "4.0");

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::byRows(
        1,  5,  0,  -0.5 * 5,
        4,  1,  0,  -0.5 * 4,
        0,  0,  1,   0,
        0,  0,  0,   1
    ), TestEpsilon));
}

TEST_F(MaterialsTest, MaterialParserStageTransforms)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/combined1");

    auto stage = material->getAllLayers().front();
    EXPECT_EQ(stage->getTransformations().size(), 2);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "time");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "0.5");
    EXPECT_EQ(stage->getTransformations().at(1).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(1).expression1->getExpressionString(), "0.7");
    EXPECT_EQ(stage->getTransformations().at(1).expression2->getExpressionString(), "0.5");

    stage->evaluateExpressions(1000);
    EXPECT_TRUE(stage->getTextureTransform().isEqual(Matrix4::getTranslation(Vector3(1, 0.5, 0) + Vector3(0.7, 0.5, 0)), TestEpsilon));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/combined2");

    stage = material->getAllLayers().front();
    EXPECT_EQ(stage->getTransformations().size(), 3);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "time");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "0.5");
    EXPECT_EQ(stage->getTransformations().at(1).type, IShaderLayer::TransformType::Scale);
    EXPECT_EQ(stage->getTransformations().at(1).expression1->getExpressionString(), "0.6");
    EXPECT_EQ(stage->getTransformations().at(1).expression2->getExpressionString(), "0.2");
    EXPECT_EQ(stage->getTransformations().at(2).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(2).expression1->getExpressionString(), "0.7");
    EXPECT_EQ(stage->getTransformations().at(2).expression2->getExpressionString(), "0.5");

    stage->evaluateExpressions(1000);
    auto combinedMatrix = Matrix4::getTranslation(Vector3(1, 0.5, 0));
    combinedMatrix.premultiplyBy(Matrix4::getScale(Vector3(0.6, 0.2, 1)));
    combinedMatrix.premultiplyBy(Matrix4::getTranslation(Vector3(0.7, 0.5, 0)));
    EXPECT_TRUE(stage->getTextureTransform().isEqual(combinedMatrix, TestEpsilon));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/transform/combined3");

    stage = material->getAllLayers().front();
    EXPECT_EQ(stage->getTransformations().size(), 6);
    EXPECT_EQ(stage->getTransformations().at(0).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(0).expression1->getExpressionString(), "time");
    EXPECT_EQ(stage->getTransformations().at(0).expression2->getExpressionString(), "0.5");
    EXPECT_EQ(stage->getTransformations().at(1).type, IShaderLayer::TransformType::Shear);
    EXPECT_EQ(stage->getTransformations().at(1).expression1->getExpressionString(), "0.9");
    EXPECT_EQ(stage->getTransformations().at(1).expression2->getExpressionString(), "0.8");
    EXPECT_EQ(stage->getTransformations().at(2).type, IShaderLayer::TransformType::Rotate);
    EXPECT_EQ(stage->getTransformations().at(2).expression1->getExpressionString(), "0.22");
    EXPECT_FALSE(stage->getTransformations().at(2).expression2);
    EXPECT_EQ(stage->getTransformations().at(3).type, IShaderLayer::TransformType::CenterScale);
    EXPECT_EQ(stage->getTransformations().at(3).expression1->getExpressionString(), "0.2");
    EXPECT_EQ(stage->getTransformations().at(3).expression2->getExpressionString(), "0.1");
    EXPECT_EQ(stage->getTransformations().at(4).type, IShaderLayer::TransformType::Scale);
    EXPECT_EQ(stage->getTransformations().at(4).expression1->getExpressionString(), "0.5");
    EXPECT_EQ(stage->getTransformations().at(4).expression2->getExpressionString(), "0.4");
    EXPECT_EQ(stage->getTransformations().at(5).type, IShaderLayer::TransformType::Translate);
    EXPECT_EQ(stage->getTransformations().at(5).expression1->getExpressionString(), "1.0");
    EXPECT_EQ(stage->getTransformations().at(5).expression2->getExpressionString(), "1.0");

    auto time = 750;
    stage->evaluateExpressions(time);
    auto timeSecs = time / 1000.0;

    combinedMatrix = Matrix4::getTranslation(Vector3(timeSecs, 0.5, 0));

    auto shear = Matrix4::byColumns(1, 0.8, 0, 0, 
                                    0.9, 1.0, 0, 0, 
                                    0, 0, 1, 0, 
                                   -0.5*0.9, -0.5*0.8, 0, 1);
    combinedMatrix.premultiplyBy(shear);

    // sintable and costable lookups are [0..1], translate them to [0..2pi]
    auto cosValue = cos(0.22 * 2 * c_pi);
    auto sinValue = sin(0.22 * 2 * c_pi);

    auto rotate = Matrix4::byRows(cosValue, -sinValue, 0, (-0.5*cosValue+0.5*sinValue) + 0.5,
        sinValue, cosValue, 0, (-0.5*sinValue-0.5*cosValue) + 0.5,
        0, 0, 1, 0,
        0, 0, 0, 1);
    combinedMatrix.premultiplyBy(rotate);

    auto centerScale = Matrix4::byColumns(0.2, 0, 0, 0,
        0, 0.1, 0, 0,
        0, 0, 1, 0,
        0.5 - 0.5*0.2, 0.5 - 0.5*0.1, 0, 1);
    combinedMatrix.premultiplyBy(centerScale);
    combinedMatrix.premultiplyBy(Matrix4::getScale(Vector3(0.5, 0.4, 1)));
    combinedMatrix.premultiplyBy(Matrix4::getTranslation(Vector3(1, 1, 0)));

    EXPECT_TRUE(stage->getTextureTransform().isEqual(combinedMatrix, TestEpsilon));
}

TEST_F(MaterialsTest, MaterialParserStageVertexProgram)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram1");
    material->getAllLayers().front()->evaluateExpressions(0);

    EXPECT_EQ(material->getAllLayers().front()->getNumVertexParms(), 1);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(0), Vector4(0, 0, 0, 0)); // all 4 equal
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).index, 0);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[0]->getExpressionString(), "time");
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(0).expressions[1]);
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(0).expressions[2]);
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(0).expressions[3]);

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram2");
    material->getAllLayers().front()->evaluateExpressions(0);

    EXPECT_EQ(material->getAllLayers().front()->getNumVertexParms(), 1);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(0), Vector4(0, 3, 0, 1)); // z=0,w=1 implicitly
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).index, 0);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[0]->getExpressionString(), "time");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[1]->getExpressionString(), "3.0");
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(0).expressions[2]);
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(0).expressions[3]);

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram3");
    material->getAllLayers().front()->evaluateExpressions(0);

    EXPECT_EQ(material->getAllLayers().front()->getNumVertexParms(), 1);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(0), Vector4(0, 3, 0, 1)); // w=1 implicitly
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).index, 0);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[0]->getExpressionString(), "time");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[1]->getExpressionString(), "3.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[2]->getExpressionString(), "global3");
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(0).expressions[3]);

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram4");
    material->getAllLayers().front()->evaluateExpressions(1000); // time = 1 sec

    EXPECT_EQ(material->getAllLayers().front()->getNumVertexParms(), 1);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(0), Vector4(1, 3, 0, 2));
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).index, 0);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[0]->getExpressionString(), "time");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[1]->getExpressionString(), "3.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[2]->getExpressionString(), "global3");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[3]->getExpressionString(), "time * 2.0");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram5");
    material->getAllLayers().front()->evaluateExpressions(2000); // time = 2 secs

    EXPECT_EQ(material->getAllLayers().front()->getNumVertexParms(), 3);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(0), Vector4(2, 3, 0, 4));

    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).index, 0);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[0]->getExpressionString(), "time");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[1]->getExpressionString(), "3.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[2]->getExpressionString(), "global3");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[3]->getExpressionString(), "time * 2.0");

    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(1), Vector4(1, 2, 3, 4));
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(1).index, 1);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(1).expressions[0]->getExpressionString(), "1.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(1).expressions[1]->getExpressionString(), "2.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(1).expressions[2]->getExpressionString(), "3.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(1).expressions[3]->getExpressionString(), "4.0");

    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(2), Vector4(5, 6, 7, 8));
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).index, 2);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[0]->getExpressionString(), "5.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[1]->getExpressionString(), "6.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[2]->getExpressionString(), "7.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[3]->getExpressionString(), "8.0");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram6");
    material->getAllLayers().front()->evaluateExpressions(2000); // time = 2 secs

    EXPECT_EQ(material->getAllLayers().front()->getNumVertexParms(), 3);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(0), Vector4(2, 3, 0, 4));

    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).index, 0);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[0]->getExpressionString(), "time");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[1]->getExpressionString(), "3.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[2]->getExpressionString(), "global3");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(0).expressions[3]->getExpressionString(), "time * 2.0");

    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(1), Vector4(0, 0, 0, 0));
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(1).index, -1); // missing
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(1).expressions[0]);
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(1).expressions[1]);
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(1).expressions[2]);
    EXPECT_FALSE(material->getAllLayers().front()->getVertexParm(1).expressions[3]);

    EXPECT_EQ(material->getAllLayers().front()->getVertexParmValue(2), Vector4(5, 6, 7, 8));
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).index, 2);
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[0]->getExpressionString(), "5.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[1]->getExpressionString(), "6.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[2]->getExpressionString(), "7.0");
    EXPECT_EQ(material->getAllLayers().front()->getVertexParm(2).expressions[3]->getExpressionString(), "8.0");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/vertexProgram7");
    EXPECT_TRUE(material->getAllLayers().empty()); // failure to parse should end up with an empty material
}

TEST_F(MaterialsTest, MaterialParserStageFragmentProgram)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/program/fragmentProgram1");

    EXPECT_EQ(material->getAllLayers().front()->getFragmentProgram(), "glprogs/test.vfp");
    EXPECT_EQ(material->getAllLayers().front()->getNumFragmentMaps(), 3);
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(0).index, 0);
    EXPECT_EQ(string::join(material->getAllLayers().front()->getFragmentMap(0).options, " "), "cubeMap forceHighQuality alphaZeroClamp");
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(0).map->getExpressionString(), "env/gen1");

    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(1).index, 1);
    EXPECT_EQ(string::join(material->getAllLayers().front()->getFragmentMap(1).options, " "), "");
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(1).map->getExpressionString(), "temp/texture");

    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(2).index, 2);
    EXPECT_EQ(string::join(material->getAllLayers().front()->getFragmentMap(2).options, " "), "cubemap cameracubemap nearest linear clamp noclamp zeroclamp alphazeroclamp forcehighquality uncompressed highquality nopicmip");
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(2).map->getExpressionString(), "temp/optionsftw");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/fragmentProgram2");

    EXPECT_EQ(material->getAllLayers().front()->getFragmentProgram(), "glprogs/test.vfp");
    EXPECT_EQ(material->getAllLayers().front()->getNumFragmentMaps(), 3);
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(0).index, 0);
    EXPECT_EQ(string::join(material->getAllLayers().front()->getFragmentMap(0).options, " "), "");
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(0).map->getExpressionString(), "env/gen1");

    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(1).index, -1); // is missing
    EXPECT_EQ(string::join(material->getAllLayers().front()->getFragmentMap(1).options, " "), "");
    EXPECT_FALSE(material->getAllLayers().front()->getFragmentMap(1).map);

    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(2).index, 2);
    EXPECT_EQ(string::join(material->getAllLayers().front()->getFragmentMap(2).options, " "), "");
    EXPECT_EQ(material->getAllLayers().front()->getFragmentMap(2).map->getExpressionString(), "temp/texture");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/program/fragmentProgram3");
    EXPECT_TRUE(material->getAllLayers().empty()); // failure to parse should end up with an empty material
}

TEST_F(MaterialsTest, MaterialParserGuiSurf)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/guisurf1");
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_GUISURF);
    EXPECT_FALSE(material->getSurfaceFlags() & (Material::SURF_ENTITYGUI| Material::SURF_ENTITYGUI2| Material::SURF_ENTITYGUI3));
    EXPECT_EQ(material->getGuiSurfArgument(), "guis/lvlmaps/genericmap.gui");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/guisurf2");
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_GUISURF);
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_ENTITYGUI);
    EXPECT_FALSE(material->getSurfaceFlags() & (Material::SURF_ENTITYGUI2 | Material::SURF_ENTITYGUI3));
    EXPECT_EQ(material->getGuiSurfArgument(), "");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/guisurf3");
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_GUISURF);
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_ENTITYGUI2);
    EXPECT_FALSE(material->getSurfaceFlags() & (Material::SURF_ENTITYGUI | Material::SURF_ENTITYGUI3));
    EXPECT_EQ(material->getGuiSurfArgument(), "");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/guisurf4");
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_GUISURF);
    EXPECT_TRUE(material->getSurfaceFlags() & Material::SURF_ENTITYGUI3);
    EXPECT_FALSE(material->getSurfaceFlags() & (Material::SURF_ENTITYGUI | Material::SURF_ENTITYGUI2));
    EXPECT_EQ(material->getGuiSurfArgument(), "");
}

TEST_F(MaterialsTest, MaterialParserRgbaExpressions)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr1");

    auto diffuse = material->getAllLayers().front();
    auto time = 10;
    auto timeSecs = time / 1000.0f; // 0.01
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs*3, 1, 1, 1));
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 3.0");
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_GREEN));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_BLUE));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr2");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(1, timeSecs*3, 1, 1));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_RED));
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_GREEN)->getExpressionString(), "time * 3.0");
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_BLUE));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr3");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(1, 1, timeSecs * 3, 1));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_RED));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_GREEN));
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_BLUE)->getExpressionString(), "time * 3.0");
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr4");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(1, 1, 1, timeSecs * 3));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_RED));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_GREEN));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_BLUE));
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA)->getExpressionString(), "time * 3.0");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr5");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs * 3, timeSecs * 3, timeSecs * 3, 1));
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_GREEN)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_BLUE)->getExpressionString(), "time * 3.0");
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr6");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs * 3, timeSecs * 3, timeSecs * 3, timeSecs * 3));
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_GREEN)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_BLUE)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA)->getExpressionString(), "time * 3.0");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr7");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs * 4, 1, 1, 1)); // second red expression overrules first
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 4.0");
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_GREEN));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_BLUE));
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr8");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs * 4, timeSecs * 3, timeSecs * 3, 1)); // red overrules rgb
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 4.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_GREEN)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_BLUE)->getExpressionString(), "time * 3.0");
    EXPECT_FALSE(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA));

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr9");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs * 3, timeSecs * 4, timeSecs * 3, timeSecs * 3)); // green overrules rgba
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_GREEN)->getExpressionString(), "time * 4.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_BLUE)->getExpressionString(), "time * 3.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA)->getExpressionString(), "time * 3.0");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/colourexpr10");

    diffuse = material->getAllLayers().front();
    diffuse->evaluateExpressions(time);

    EXPECT_TRUE(diffuse->getColour() == Colour4(timeSecs * 4, timeSecs * 6, timeSecs * 5, timeSecs * 7)); // rgba is overridden
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_RED)->getExpressionString(), "time * 4.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_GREEN)->getExpressionString(), "time * 6.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_BLUE)->getExpressionString(), "time * 5.0");
    EXPECT_EQ(diffuse->getColourExpression(IShaderLayer::COMP_ALPHA)->getExpressionString(), "time * 7.0");
}

TEST_F(MaterialsTest, MaterialParserLightfallOff)
{
    auto material = GlobalMaterialManager().getMaterial("textures/parsertest/lights/lightfalloff1");

    EXPECT_EQ(material->getLightFalloffCubeMapType(), IShaderLayer::MapType::Map);
    EXPECT_EQ(material->getLightFalloffExpression()->getExpressionString(), "makeIntensity(lights/squarelight1a.tga)");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/lights/lightfalloff2");

    EXPECT_EQ(material->getLightFalloffCubeMapType(), IShaderLayer::MapType::CameraCubeMap);
    EXPECT_EQ(material->getLightFalloffExpression()->getExpressionString(), "lights/squarelight1a");

    material = GlobalMaterialManager().getMaterial("textures/parsertest/lights/lightfalloff3");

    // Second lightFallOff declaration overrides the first one in the material
    EXPECT_EQ(material->getLightFalloffCubeMapType(), IShaderLayer::MapType::CameraCubeMap);
    EXPECT_EQ(material->getLightFalloffExpression()->getExpressionString(), "lights/squarelight1a");
}

}
