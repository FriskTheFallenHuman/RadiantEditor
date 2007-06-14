#include "CurveEditInstance.h"

#include "CurveControlPointFunctors.h"

namespace entity {

CurveEditInstance::CurveEditInstance(ControlPoints& controlPointsTransformed, //  The working set
  			const ControlPoints& controlPoints,	// the unchanged reference control points 
  			const SelectionChangeCallback& selectionChanged) :
    _selectionChanged(selectionChanged),
    _controlPointsTransformed(controlPointsTransformed),
    _controlPoints(controlPoints),
    m_controlsRender(GL_POINTS),
    m_selectedRender(GL_POINTS)
  {
  }

void CurveEditInstance::testSelect(Selector& selector, SelectionTest& test) {
    ASSERT_MESSAGE(_controlPointsTransformed.size() == _selectables.size(), "curve instance mismatch");
    ControlPoints::const_iterator p = _controlPointsTransformed.begin();
    for(Selectables::iterator i = _selectables.begin(); i != _selectables.end(); ++i, ++p)
    {
    	SelectionIntersection best;
		test.TestPoint(*p, best);
		if (best.valid()) {
			Selector_add(selector, *i, best);
		}
    }
}

bool CurveEditInstance::isSelected() const {
    for(Selectables::const_iterator i = _selectables.begin(); i != _selectables.end(); ++i)
    {
      if((*i).isSelected())
      {
        return true;
      }
    }
    return false;
}

void CurveEditInstance::setSelected(bool selected) {
    for(Selectables::iterator i = _selectables.begin(); i != _selectables.end(); ++i)
    {
      (*i).setSelected(selected);
    }
}

	// Local helper, TODO!
	inline void ControlPoints_write(ControlPoints& controlPoints, const std::string& key, Entity& entity) {
		std::string value;
		if (!controlPoints.empty()) {
			value = intToStr(controlPoints.size()) + " (";
			for (ControlPoints::const_iterator i = controlPoints.begin(); i != controlPoints.end(); ++i) {
				value += " " + floatToStr(i->x()) + " " + floatToStr(i->y()) + " " + floatToStr(i->z()) + " ";
			}
			value += ")";
		}
		entity.setKeyValue(key, value);
	}

void CurveEditInstance::write(const char* key, Entity& entity) {
	ControlPoints_write(_controlPointsTransformed, key, entity);
}

void CurveEditInstance::transform(const Matrix4& matrix, bool selectedOnly) {
	ControlPointTransformator transformator(matrix);
	
  	if (selectedOnly) {
    	forEachSelected(transformator);
  	}
  	else {
  		forEach(transformator);
  	}
}

void CurveEditInstance::snapto(float snap) {
	ControlPointSnapper snapper(snap);
    forEachSelected(snapper);
}

void CurveEditInstance::updateSelected() const {
    m_selectedRender.clear();
    ControlPointAdder adder(m_selectedRender, colour_selected);
    forEachSelected(adder);
}
  
void CurveEditInstance::renderComponents(Renderer& renderer, 
	const VolumeTest& volume, const Matrix4& localToWorld) const
{
    renderer.SetState(StaticShaders::instance().controlsShader, Renderer::eWireframeOnly);
    renderer.SetState(StaticShaders::instance().controlsShader, Renderer::eFullMaterials);
    renderer.addRenderable(m_controlsRender, localToWorld);
}

void CurveEditInstance::renderComponentsSelected(Renderer& renderer, 
	const VolumeTest& volume, const Matrix4& localToWorld) const
{
    updateSelected();
    if(!m_selectedRender.empty())
    {
      renderer.Highlight(Renderer::ePrimitive, false);
      renderer.SetState(StaticShaders::instance().selectedShader, Renderer::eWireframeOnly);
      renderer.SetState(StaticShaders::instance().selectedShader, Renderer::eFullMaterials);
      renderer.addRenderable(m_selectedRender, localToWorld);
    }
}

void CurveEditInstance::curveChanged() {
    _selectables.resize(_controlPointsTransformed.size(), _selectionChanged);

    m_controlsRender.clear();
    m_controlsRender.reserve(_controlPointsTransformed.size());
    ControlPointAdder adder(m_controlsRender);
    forEach(adder);

    m_selectedRender.reserve(_controlPointsTransformed.size());
}

void CurveEditInstance::forEachSelected(ControlPointFunctor& functor) {
	ASSERT_MESSAGE(_controlPointsTransformed.size() == _selectables.size(), "curve instance mismatch");
	ControlPoints::iterator transformed = _controlPointsTransformed.begin();
	ControlPoints::const_iterator original = _controlPoints.begin();

	for (Selectables::iterator i = _selectables.begin(); 
		 i != _selectables.end(); 
		 ++i, ++transformed, ++original)
	{
		if (i->isSelected()) {
    		functor(*transformed, *original);
  		}
	}
}

void CurveEditInstance::forEachSelected(ControlPointConstFunctor& functor) const {
	ASSERT_MESSAGE(_controlPointsTransformed.size() == _selectables.size(), "curve instance mismatch");
	ControlPoints::const_iterator transformed = _controlPointsTransformed.begin();
	ControlPoints::const_iterator original = _controlPoints.begin();

	for (Selectables::const_iterator i = _selectables.begin(); 
		 i != _selectables.end(); 
		 ++i, ++transformed, ++original)
	{
		if (i->isSelected()) {
    		functor(*transformed, *original);
  		}
	}
}

void CurveEditInstance::forEach(ControlPointFunctor& functor) {
	ControlPoints::const_iterator original = _controlPoints.begin();
	for (ControlPoints::iterator i = _controlPointsTransformed.begin(); 
    	i != _controlPointsTransformed.end(); 
    	++i, ++original)
    {
		functor(*i, *original);
    }
}

} // namespace entity
