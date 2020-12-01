#include "GLFont.h"

#include "itextstream.h"
#include "igl.h"
#include "imodule.h"
#include <iostream>

namespace gl
{

GLFont::GLFont(Style style, unsigned int size) :
	_pixelHeight(0),
	_ftglFont(nullptr)
{
    // Load the locally-provided TTF font file
	std::string fontpath = module::GlobalModuleRegistry()
                           .getApplicationContext()
                           .getRuntimeDataPath()
                           + "ui/fonts/";

	fontpath += style == FONT_SANS ? "FreeSans.ttf" : "FreeMono.ttf";

	_ftglFont = FTGL::ftglCreatePixmapFont(fontpath.c_str());

	if (_ftglFont)
	{
        FTGL::ftglSetFontFaceSize(_ftglFont, size, 0);
		_pixelHeight = static_cast<int>(FTGL::ftglGetFontLineHeight(_ftglFont));
	}
	else
	{
		rError() << "Failed to create FTGLPixmapFont" << std::endl;
	}
}

GLFont::~GLFont()
{
	if (_ftglFont)
	{
		FTGL::ftglDestroyFont(_ftglFont);
		_ftglFont = nullptr;
	}
}

} // namespace
