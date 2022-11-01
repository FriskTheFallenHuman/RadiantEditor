#include "DragManipulator.h"

#include "selection/SelectionPool.h"
#include "selection/SelectionTestWalkers.h"
#include "selection/algorithm/Planes.h"
#include "selection/SingleItemSelector.h"
#include "selection/BestSelector.h"

#include "registry/registry.h"

namespace selection
{

const std::string RKEY_TRANSIENT_COMPONENT_SELECTION = "user/ui/transientComponentSelection";

DragManipulator::DragManipulator(ManipulationPivot& pivot, ISceneSelectionTesterFactory& factory) :
    _pivot(pivot),
    _testerFactory(factory),
    _freeResizeComponent(_resizeTranslatable),
    _resizeModeActive(false),
    _freeDragComponent(_dragTranslatable),
    _dragTranslatable(SelectionTranslator::TranslationCallback())
{}

DragManipulator::Type DragManipulator::getType() const
{
	return Drag;
}

DragManipulator::Component* DragManipulator::getActiveComponent() 
{
    return _dragSelectable.isSelected() ? &_freeDragComponent : &_freeResizeComponent;
}

void DragManipulator::testSelect(SelectionTest& test, const Matrix4& pivot2world)
{
	_resizeModeActive = false;

    // No drag manipulation in merge mode
    if (GlobalSelectionSystem().getSelectionMode() == SelectionMode::MergeAction) return;

    SelectionPool selector;

	switch (GlobalSelectionSystem().getSelectionMode())
	{
	case SelectionMode::Primitive:
		testSelectPrimitiveMode(test.getVolume(), test, selector);
		break;
	case SelectionMode::GroupPart:
		testSelectGroupPartMode(test.getVolume(), test, selector);
		break;
	case SelectionMode::Entity:
		testSelectEntityMode(test.getVolume(), test, selector);
		break;
	case SelectionMode::Component:
		testSelectComponentMode(test.getVolume(), test, selector);
		break;
	};

	for (SelectionPool::const_iterator i = selector.begin(); i != selector.end(); ++i)
	{
		i->second->setSelected(true);
	}
}

void DragManipulator::testSelectPrimitiveMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
	SingleItemSelector itemSelector;

	// First try to select entities (including func_* groups)
	EntitySelector selectionTester(itemSelector, test);
	GlobalSceneGraph().foreachVisibleNodeInVolume(view, selectionTester);

	if (itemSelector.hasValidSelectable())
	{
		// Found a selectable entity
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
		return;
	}
	
	// Find all worldspawn primitives
	PrimitiveSelector primitiveTester(itemSelector, test);
	GlobalSceneGraph().foreachVisibleNodeInVolume(view, primitiveTester);

	if (itemSelector.hasValidSelectable())
	{
		// Found a selectable primitive
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
		return;
	}

	// Entities and worldspawn primitives failed, so check for group children too
	// Find all group child primitives that are selectable
	GroupChildPrimitiveSelector childPrimitiveTester(itemSelector, test);
	GlobalSceneGraph().foreachVisibleNodeInVolume(view, childPrimitiveTester);

	if (itemSelector.hasValidSelectable())
	{
		// Found a selectable group child primitive
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
		return;
	}

	// all direct hits failed, check for drag-selectable faces
	_resizeModeActive = algorithm::testSelectPlanes(selector, test);
}

void DragManipulator::testSelectGroupPartMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
	// Find all primitives that are selectable
	SingleItemSelector itemSelector;

    auto tester = _testerFactory.createSceneSelectionTester(SelectionMode::GroupPart);
    tester->testSelectScene(view, test, itemSelector);

	if (itemSelector.hasValidSelectable())
	{
		// Found a selectable primitive
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
		return;
	}

	// Check for selectable faces
	_resizeModeActive = algorithm::testSelectPlanes(selector, test);
}

void DragManipulator::testSelectEntityMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
	// Create a boolean selection pool (can have exactly one selectable or none)
	SingleItemSelector itemSelector;

    auto tester = _testerFactory.createSceneSelectionTester(SelectionMode::Entity);
    tester->testSelectScene(view, test, itemSelector);

	// Check, if an entity could be found
	if (itemSelector.hasValidSelectable())
	{
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
	}
}

void DragManipulator::testSelectComponentMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
	BestSelector bestSelector;

	ComponentSelector selectionTester(bestSelector, test, GlobalSelectionSystem().ComponentMode());
	GlobalSelectionSystem().foreachSelected(selectionTester);

	bool transientComponentSelection = registry::getValue<bool>(RKEY_TRANSIENT_COMPONENT_SELECTION);

	for (ISelectable* selectable : bestSelector.getBestSelectables())
	{
		// greebo: For transient component selection, clicking an unselected
		// component will deselect all previously selected components beforehand
		if (transientComponentSelection && !selectable->isSelected())
		{
			GlobalSelectionSystem().setSelectedAllComponents(false);
		}

		selector.addSelectable(SelectionIntersection(0, 0), selectable);
		_dragSelectable.setSelected(true);
	}
}

void DragManipulator::setSelected(bool select) 
{
    _resizeModeActive = select;
    _dragSelectable.setSelected(select);
}

bool DragManipulator::isSelected() const
{
	return _resizeModeActive || _dragSelectable.isSelected();
}

}
