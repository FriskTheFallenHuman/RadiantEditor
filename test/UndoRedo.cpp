#include "RadiantTest.h"

#include "iundo.h"
#include "ibrush.h"
#include "ieclass.h"
#include "ientity.h"
#include "imap.h"
#include "icommandsystem.h"
#include "math/Matrix4.h"
#include "algorithm/Scene.h"
#include "scenelib.h"

namespace test
{

using UndoTest = RadiantTest;

TEST_F(UndoTest, BrushCreation)
{
    std::string mapPath = "maps/simple_brushes.map";
    GlobalCommandSystem().executeCommand("OpenMap", mapPath);

    auto material = "textures/numbers/19";
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    EXPECT_FALSE(algorithm::findFirstBrushWithMaterial(worldspawn, material)) << "Test map already has a brush with that material";

    auto brushNode = GlobalBrushCreator().createBrush();
    auto& brush = *Node_getIBrush(brushNode);

    auto translation = Matrix4::getTranslation({ 20, 3, -7 });
    brush.addFace(Plane3(+1, 0, 0, 64).transform(translation));
    brush.addFace(Plane3(-1, 0, 0, 64).transform(translation));
    brush.addFace(Plane3(0, +1, 0, 64).transform(translation));
    brush.addFace(Plane3(0, -1, 0, 64).transform(translation));
    brush.addFace(Plane3(0, 0, +1, 64).transform(translation));
    brush.addFace(Plane3(0, 0, -1, 64).transform(translation));

    brush.setShader("textures/numbers/19");
    brush.evaluateBRep();
    
    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        scene::addNodeToContainer(brushNode, worldspawn);
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";
    EXPECT_TRUE(algorithm::findFirstBrushWithMaterial(worldspawn, material)) << "Could not locate the brush";

    // Brush should be gone now
    GlobalUndoSystem().undo();
    EXPECT_FALSE(brushNode->inScene());
    EXPECT_FALSE(algorithm::findFirstBrushWithMaterial(worldspawn, material)) << "Brush should be gone after undo";

    // Redo, brush should be back again
    GlobalUndoSystem().redo();
    EXPECT_TRUE(brushNode->inScene());
    EXPECT_TRUE(algorithm::findFirstBrushWithMaterial(worldspawn, material)) << "Could not locate the brush after redo";
}

namespace
{

constexpr const char* InitialTestKeyValue = "0";

inline scene::INodePtr setupTestEntity()
{
    auto entity = GlobalEntityModule().createEntity(GlobalEntityClassManager().findClass("light"));
    scene::addNodeToContainer(entity, GlobalMapModule().getRoot());
    Node_getEntity(entity)->setKeyValue("test", InitialTestKeyValue);

    return entity;
}

}

TEST_F(UndoTest, AddEntityKeyValue)
{
    auto entity = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("newkey", "ljdaslk");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("newkey"), "");

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("newkey"), "ljdaslk");
}

TEST_F(UndoTest, ChangeEntityKeyValue)
{
    auto entity = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "1");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), InitialTestKeyValue);

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1");
}

TEST_F(UndoTest, RemoveEntityKeyValue)
{
    auto entity = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), InitialTestKeyValue);

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "");
}

TEST_F(UndoTest, MultipleChangesInSingleOperation)
{
    auto entity = setupTestEntity();
    auto entity2 = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "1");
        Node_getEntity(entity)->setKeyValue("test1", "value1");

        Node_getEntity(entity2)->setKeyValue("test", "2");
        Node_getEntity(entity2)->setKeyValue("test2", "value2");
        Node_getEntity(entity2)->setKeyValue("test3", "value3");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();
    Node_getEntity(entity)->setKeyValue("test", InitialTestKeyValue);
    Node_getEntity(entity)->setKeyValue("test1", "");

    Node_getEntity(entity2)->setKeyValue("test", InitialTestKeyValue);
    Node_getEntity(entity2)->setKeyValue("test2", "");
    Node_getEntity(entity2)->setKeyValue("test3", "");

    GlobalUndoSystem().redo();
    Node_getEntity(entity)->setKeyValue("test", "1");
    Node_getEntity(entity)->setKeyValue("test1", "value1");

    Node_getEntity(entity2)->setKeyValue("test", "2");
    Node_getEntity(entity2)->setKeyValue("test2", "value2");
    Node_getEntity(entity2)->setKeyValue("test3", "value3");
}

TEST_F(UndoTest, NodeOutsideScene)
{
    auto entity = setupTestEntity();
    
    scene::removeNodeFromParent(entity);

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";
    EXPECT_FALSE(entity->inScene()) << "Node has been removed from its parent, should be outside the scene";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "1");
    }

    // The map should not care about this change
    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map should not be modified after this change";

    GlobalUndoSystem().undo();
    // Node should still be unaffected by this
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1");

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1");
}

TEST_F(UndoTest, SequentialOperations)
{
    auto entity = setupTestEntity();
    auto entity2 = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "1");
    }
    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test1", "value1");
    }
    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity2)->setKeyValue("test", "2");
    }
    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity2)->setKeyValue("test2", "value2");
    }
    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity2)->setKeyValue("test3", "value3");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    // Roll back one change after the other

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), "value2"); // unchanged
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), "2"); // unchanged
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // unchanged
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // unchanged

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), "2"); // unchanged
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // unchanged
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // unchanged

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), InitialTestKeyValue); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // unchanged
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // unchanged

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), InitialTestKeyValue); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // unchanged

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), InitialTestKeyValue); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), InitialTestKeyValue); // undone

    // Then redo all of these one after one

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), InitialTestKeyValue); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // redone

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), InitialTestKeyValue); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // redone

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), "2"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // redone

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), "value2"); // redone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), "2"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // redone

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test3"), "value3"); // redone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), "value2"); // redone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), "2"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // redone
}

TEST_F(UndoTest, NestedOperations)
{
    auto entity = setupTestEntity();
    auto entity2 = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    // A nested set of operations, should be undoable as one single operation
    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "1");

        {
            UndoableCommand cmd("testOperation");
            Node_getEntity(entity)->setKeyValue("test1", "value1");
        }
        {
            UndoableCommand cmd("testOperation");
            Node_getEntity(entity2)->setKeyValue("test", "2");

            {
                UndoableCommand cmd("testOperation");
                Node_getEntity(entity2)->setKeyValue("test2", "value2");
            }
        }

        Node_getEntity(entity)->setKeyValue("last", "one");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("last"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), InitialTestKeyValue); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), ""); // undone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), InitialTestKeyValue); // undone

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("last"), "one"); // redone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test2"), "value2"); // redone
    EXPECT_EQ(Node_getEntity(entity2)->getKeyValue("test"), "2"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test1"), "value1"); // redone
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "1"); // redone
}

// Changing a key value multiple times within a single operation is treated correctly
TEST_F(UndoTest, MultipleKeyValueChangeInSingleOperation)
{
    auto entity = setupTestEntity();

    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    {
        UndoableCommand cmd("testOperation");
        Node_getEntity(entity)->setKeyValue("test", "1");
        Node_getEntity(entity)->setKeyValue("test", "2");
        Node_getEntity(entity)->setKeyValue("test", "3");
        Node_getEntity(entity)->setKeyValue("test", "4");
    }

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), InitialTestKeyValue);

    GlobalUndoSystem().redo();
    EXPECT_EQ(Node_getEntity(entity)->getKeyValue("test"), "4");
}

TEST_F(UndoTest, SceneNodeRemoval)
{
    GlobalCommandSystem().executeCommand("OpenMap", cmd::Argument("maps/entityinspector.map"));

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto speaker_1 = algorithm::getEntityByName(GlobalMapModule().getRoot(), "speaker_1");
    auto func_static = algorithm::getEntityByName(GlobalMapModule().getRoot(), "func_static_1");

    auto childBrush = algorithm::findFirstBrushWithMaterial(func_static, "textures/numbers/4");
    auto worldspawnBrush = algorithm::findFirstBrushWithMaterial(worldspawn, "textures/numbers/1");

    // Check the prerequisities
    EXPECT_FALSE(GlobalMapModule().isModified()) << "Map already modified before the change";

    EXPECT_TRUE(speaker_1);
    EXPECT_TRUE(func_static);
    EXPECT_TRUE(childBrush);
    EXPECT_TRUE(worldspawnBrush);

    EXPECT_TRUE(speaker_1->getParent());
    EXPECT_TRUE(func_static->getParent());
    EXPECT_TRUE(childBrush->getParent());
    EXPECT_TRUE(worldspawnBrush->getParent());

    {
        UndoableCommand cmd("testOperation");
        
        // Remove one of the entities
        scene::removeNodeFromParent(speaker_1);

        // Remove a worldspawn brush
        scene::removeNodeFromParent(worldspawnBrush);

        // Remove a child brush from the func_static
        scene::removeNodeFromParent(childBrush);

        // And top that by removing the func_static itself
        scene::removeNodeFromParent(func_static);
    }

    // All nodes should now be without parent
    EXPECT_FALSE(speaker_1->getParent());
    EXPECT_FALSE(func_static->getParent());
    EXPECT_FALSE(childBrush->getParent());
    EXPECT_FALSE(worldspawnBrush->getParent());

    // Lookup the nodes again, they should be gone
    EXPECT_FALSE(algorithm::getEntityByName(GlobalMapModule().getRoot(), "speaker_1"));
    EXPECT_FALSE(algorithm::getEntityByName(GlobalMapModule().getRoot(), "func_static_1"));
    EXPECT_FALSE(algorithm::findFirstBrushWithMaterial(func_static, "textures/numbers/4"));
    EXPECT_FALSE(algorithm::findFirstBrushWithMaterial(worldspawn, "textures/numbers/1"));

    EXPECT_TRUE(GlobalMapModule().isModified()) << "Map should be modified after this change";

    GlobalUndoSystem().undo();

    // All nodes should have proper parents now
    EXPECT_TRUE(speaker_1->getParent());
    EXPECT_TRUE(func_static->getParent());
    EXPECT_TRUE(childBrush->getParent());
    EXPECT_TRUE(worldspawnBrush->getParent());

    EXPECT_TRUE(speaker_1->inScene());
    EXPECT_TRUE(func_static->inScene());
    EXPECT_TRUE(childBrush->inScene());
    EXPECT_TRUE(worldspawnBrush->inScene());

    // Lookup the nodes, they should be present again
    EXPECT_TRUE(algorithm::getEntityByName(GlobalMapModule().getRoot(), "speaker_1"));
    EXPECT_TRUE(algorithm::getEntityByName(GlobalMapModule().getRoot(), "func_static_1"));
    EXPECT_TRUE(algorithm::findFirstBrushWithMaterial(func_static, "textures/numbers/4"));
    EXPECT_TRUE(algorithm::findFirstBrushWithMaterial(worldspawn, "textures/numbers/1"));

    GlobalUndoSystem().redo();

    // Well, this is weird, parents are not cleared when importing a TraversableNodeSet from
    // an undo memento, this seems to have been introduced in the fix to #2118
    EXPECT_TRUE(speaker_1->getParent());
    EXPECT_TRUE(func_static->getParent());
    EXPECT_TRUE(childBrush->getParent());
    EXPECT_TRUE(worldspawnBrush->getParent());

    // All nodes should be outside the scene
    EXPECT_FALSE(speaker_1->inScene());
    EXPECT_FALSE(func_static->inScene());
    EXPECT_FALSE(childBrush->inScene());
    EXPECT_FALSE(worldspawnBrush->inScene());

    // At least, they should be gone when trying to look them up through the scene
    EXPECT_FALSE(algorithm::getEntityByName(GlobalMapModule().getRoot(), "speaker_1"));
    EXPECT_FALSE(algorithm::getEntityByName(GlobalMapModule().getRoot(), "func_static_1"));
    // Same oddity here, the child brush didn't get its parent reference cleared during redo
    EXPECT_TRUE(algorithm::findFirstBrushWithMaterial(func_static, "textures/numbers/4"));
    EXPECT_FALSE(algorithm::findFirstBrushWithMaterial(worldspawn, "textures/numbers/1"));
}

}