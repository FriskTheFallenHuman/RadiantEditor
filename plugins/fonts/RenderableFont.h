#ifndef _RENDERABLE_FONT_H_
#define _RENDERABLE_FONT_H_

#include "irenderable.h"
#include "math/Vector3.h"

#include "FontInfo.h"

namespace fonts
{

class RenderableFont :
	public Renderable
{
private:
	// The current state (used during front end rendering)
	mutable ShaderPtr _curState;

	// The font we're rendering
	FontInfoPtr _font;

public:
	// Construct a renderable for the given font
	RenderableFont(const FontInfoPtr& font);

	// Renderable implementation, adds OpenGLRenderables to the collector
	void renderSolid(RenderableCollector& collector, const VolumeTest& volume) const;
	void renderWireframe(RenderableCollector& collector, const VolumeTest& volume) const;

	// Renders the text at the given position
	void renderText(const std::string& text, const Vector3& pos);

	// Renders the given text in the given rectangle, starting
	// from top left, wrapping the text at the borders.
	void renderText(const std::string& text, 
					const Vector3& areaTopLeft, 
					const Vector3& areaBottomRight);
};

} // namespace fonts

#endif /* _RENDERABLE_FONT_H_ */
