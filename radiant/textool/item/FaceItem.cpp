#include "FaceItem.h"

#include "igl.h"
#include "iselectiontest.h"
#include "math/FloatTools.h"

#include "FaceVertexItem.h"

namespace textool
{

FaceItem::FaceItem(IFace& sourceFace) :
	_sourceFace(sourceFace),
	_winding(sourceFace.getWinding())
{
	// Allocate a vertex item for each winding vertex
	for (auto& vertex : _winding)
	{
		_children.emplace_back(new FaceVertexItem(_sourceFace, vertex, *this));
	}
}

AABB FaceItem::getExtents()
{
	AABB returnValue;

	for (const auto& vertex : _winding)
	{
		returnValue.includePoint(Vector3(vertex.texcoord[0], vertex.texcoord[1], 0));
	}

	return returnValue;
}

void FaceItem::render()
{
	glEnable(GL_BLEND);
	glBlendColor(0,0,0, 0.3f);
	glBlendFunc(GL_CONSTANT_ALPHA_EXT, GL_ONE_MINUS_CONSTANT_ALPHA_EXT);

	if (_selected) {
		glColor3f(1, 0.5f, 0);
	}
	else {
		glColor3f(0.8f, 0.8f, 0.8f);
	}

	glBegin(GL_TRIANGLE_FAN);

	for (const auto& vertex : _winding)
	{
		glVertex2d(vertex.texcoord[0], vertex.texcoord[1]);
	}

	glEnd();
	glDisable(GL_BLEND);

	glPointSize(5);
	glBegin(GL_POINTS);
	/*for (Winding::const_iterator i = _winding.begin(); i != _winding.end(); ++i)
	{
		glVertex2f(i->texcoord[0], i->texcoord[1]);
	}*/

	glColor3f(1, 1, 1);
	Vector2 centroid = getCentroid();
	glVertex2d(centroid[0], centroid[1]);

	glEnd();

    glDisable(GL_BLEND);

	// Now invoke the default render method (calls render() on all children)
	TexToolItem::render();
}

void FaceItem::transform(const Matrix4& matrix) {
	// Pick the translation components from the matrix and apply the translation
	Vector2 translation(matrix.tx(), matrix.ty());

	// Invert the s-translation, shiftTexDef does it inversely for some reason.
	translation[0] *= -1;

	// Shift the texdef accordingly
	_sourceFace.shiftTexdef(translation[0], translation[1]);
}

void FaceItem::transformSelected(const Matrix4& matrix)
{
    // If this object is selected, transform <self>
	if (_selected)
    {
		transform(matrix);
	}
	else 
    {
		// Object is not selected, propagate the call to the children
        // Special behaviour for faces: don't propagate the call to 
        // more than one selected item
		for (std::size_t i = 0; i < _children.size(); i++) 
        {
            if (_children[i]->isSelected())
            {
                _children[i]->transformSelected(matrix);
                break;
            }
		}
	}
	update();
}

Vector2 FaceItem::getCentroid() const {
	Vector2 texCentroid;

	for (const auto& vertex : _winding)
	{
		texCentroid += vertex.texcoord;
	}

	// Take the average value of all the winding texcoords to retrieve the centroid
	texCentroid /= _winding.size();

	return texCentroid;
}

bool FaceItem::testSelect(const Rectangle& rectangle)
{
	Vector2 texCentroid;

	for (const auto& vertex : _winding)
	{
		/*if (rectangle.contains(i->texcoord))
		{
			return true;
		}*/

		// Otherwise, just continue summing up the texcoords for the centroid check
		texCentroid += vertex.texcoord;
	}

	// Take the average value of all the winding texcoords to retrieve the centroid
	texCentroid /= _winding.size();

	return rectangle.contains(texCentroid);
}

void FaceItem::testSelect(Selector& selector, SelectionTest& test)
{
    // Arrange the UV coordinates in a Vector3 array for testing
    std::vector<Vector3> uvs;
    uvs.reserve(_winding.size());

    for (const auto& vertex : _winding)
    {
        uvs.emplace_back(vertex.texcoord.x(), vertex.texcoord.y(), 0);
    }

    test.BeginMesh(Matrix4::getIdentity(), true);

    SelectionIntersection best;
    test.TestPolygon(VertexPointer(uvs.data(), sizeof(Vector3)), uvs.size(), best);

    if (best.isValid())
    {
        Selector_add(selector, *this);
    }
}

void FaceItem::snapSelectedToGrid(float grid)
{
	if (_selected)
	{
		Vector2 centroid = getCentroid();

		Vector2 snapped(
			float_snapped(centroid[0], grid),
			float_snapped(centroid[1], grid)
		);

		Vector3 translation;
		translation[0] = snapped[0] - centroid[0];
		translation[1] = snapped[1] - centroid[1];

		Matrix4 matrix = Matrix4::getTranslation(translation);

		// Do the transformation
		transform(matrix);
	}

	// Let the base class call the method on our children
	TexToolItem::snapSelectedToGrid(grid);
}

void FaceItem::flipSelected(const int& axis) {
	if (_selected) {
		_sourceFace.flipTexture(axis);
	}
}

} // namespace TexTool
