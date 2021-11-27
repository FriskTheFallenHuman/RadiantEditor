#pragma once

#include "irender.h"
#include "iregistry.h"
#include "iselection.h"
#include "iselectiontest.h"
#include "math/Vector3.h"
#include "ObservedSelectable.h"
#include "EntitySettings.h"

class VertexInstance :
	public OpenGLRenderable,
	public ISelectable
{
protected:
	Vector3& _vertex;

	// The Selectable
	selection::ObservedSelectable _selectable;

	Vector3 _colour;

	// Shader to use for the point
	ShaderPtr _shader;

public:
	// Construct the instance with the given <vertex> coordinates and connect the selectionChangeCallback
	VertexInstance(Vector3& vertex, const SelectionChangedSlot& observer) :
		_vertex(vertex),
		_selectable(observer),
		_colour(entity::EntitySettings::InstancePtr()->getLightVertexColour(LightEditVertexType::Deselected))
	{}

	void setRenderSystem(const RenderSystemPtr& renderSystem)
	{
		if (renderSystem)
		{
			_shader = renderSystem->capture("$BIGPOINT");
		}
		else
		{
			_shader.reset();
		}
	}

	void setVertex(const Vector3& vertex) {
		_vertex = vertex;
	}

	virtual const Vector3 getVertex() const {
		return _vertex;
	}

	// Return the Shader for rendering
  	const ShaderPtr& getShader() const {
  		return _shader;
  	}

	void setSelected(bool select) {
		_selectable.setSelected(select);
		// Change the colour according to the selection
		_colour = entity::EntitySettings::InstancePtr()->getLightVertexColour(
			select ? LightEditVertexType::Selected : LightEditVertexType::Deselected
		);
	}

	bool isSelected() const {
		return _selectable.isSelected();
	}

	void invertSelected() {
		setSelected(!isSelected());
	}

	virtual void testSelect(Selector& selector, SelectionTest& test) {
		SelectionIntersection best;
		test.TestPoint(_vertex, best);

		if (best.isValid()) {
			// Add the selectable to the given selector > this should trigger the callbacks
			Selector_add(selector, *this, best);
		}
	}

	// Front-end render function
	void render(IRenderableCollector& collector,
                const VolumeTest& volume,
                const Matrix4& localToWorld) const
    {
		collector.setHighlightFlag(IRenderableCollector::Highlight::Primitives, false);
		collector.setHighlightFlag(IRenderableCollector::Highlight::Faces, false);
		collector.addRenderable(*_shader, *this, localToWorld);
	}

	// GL render function (backend)
  	virtual void render(const RenderInfo& info) const {
		// Draw the center point
	    glBegin(GL_POINTS);
	    glColor3dv(_colour);
	    glVertex3dv(_vertex);
	    glEnd();
	}
}; // class VertexInstance

/* This is the vertexinstance class for the light_right and light_up vertex, as they
 * are calculated relatively to the light_target, which in turn is relative to the light origin
 */
class VertexInstanceRelative : public VertexInstance {
	Vector3& _origin;
public:
	// Construct the instance with the given <vertex> coordinates and connect the selectionChangeCallback
	VertexInstanceRelative(Vector3& relativeToOrigin, Vector3& origin, const SelectionChangedSlot& observer)
		: VertexInstance(relativeToOrigin, observer),
		  _origin(origin)
	{}

	const Vector3 getVertex() const {
		return _origin + _vertex;
	}

	void testSelect(Selector& selector, SelectionTest& test) {
		SelectionIntersection best;
		Vector3 testVertex = _origin + _vertex;
		test.TestPoint(testVertex, best);

		if (best.isValid()) {
			// Add the selectable to the given selector > this should trigger the callbacks
			Selector_add(selector, *this, best);
		}
	}
}; // class VertexInstanceRelative
