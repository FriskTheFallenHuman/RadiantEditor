#pragma once

#include "igui.h"
#include "math/Vector2.h"
#include "util/Noncopyable.h"

#include <vector>

namespace gui
{

class GuiRenderer :
	public util::Noncopyable
{
private:
	IGuiPtr _gui;

	Vector2 _areaTopLeft;
	Vector2 _areaBottomRight;

	// Whether invisible windowDefs should be rendered anyway
	bool _ignoreVisibility;

public:
	// Construct a new renderer
	GuiRenderer();

	void setGui(const IGuiPtr& gui);

	void setIgnoreVisibility(bool ignoreVisibility);

	// Sets the visible area to be rendered, in absolute GUI coordinates [0,0..640,480]
	void setVisibleArea(const Vector2& topLeft, const Vector2& bottomRight);

	// Starts rendering the attached GUI
	// The GL context must be set before calling this method
	void render();

private:
	void render(const GuiWindowDefPtr& window);
};

}
