#include "Shader.h"

#include "i18n.h"
#include "iselection.h"
#include "iscenegraph.h"
#include "itextstream.h"
#include "iselectiontest.h"
#include "igroupnode.h"
#include "selectionlib.h"
#include "registry/registry.h"
#include "messages/TextureChanged.h"
#include "command/ExecutionFailure.h"
#include "command/ExecutionNotPossible.h"
#include "string/string.h"
#include "brush/FaceInstance.h"
#include "brush/BrushVisit.h"
#include "brush/TextureProjection.h"
#include "patch/PatchNode.h"
#include "selection/algorithm/Primitives.h"
#include "selection/shaderclipboard/ShaderClipboard.h"
#include "selection/shaderclipboard/ClosestTexturableFinder.h"
#include "scene/Traverse.h"

#include "string/case_conv.h"

namespace selection
{

namespace algorithm
{

void applyShaderToSelection(const std::string& shaderName)
{
	UndoableCommand undo("setShader");

	GlobalSelectionSystem().foreachFace([&] (IFace& face) { face.setShader(shaderName); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.setShader(shaderName); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void applyShaderToSelectionCmd(const cmd::ArgumentList& args)
{
	if (args.size() != 1 || args[0].getString().empty())
	{
		rMessage() << "SetShaderOnSelection <shadername>" << std::endl;
		return;
	}

	applyShaderToSelection(args[0].getString());
}

/** greebo: Applies the shader from the clipboard's face to the given <target> face
 */
void applyClipboardFaceToFace(Face& target)
{
	// Get a reference to the source Texturable in the clipboard
	Texturable& source = ShaderClipboard::Instance().getSource();

	target.applyShaderFromFace(*(source.face));
}

/** greebo: Applies the shader from the clipboard's patch to the given <target> face
 */
void applyClipboardPatchToFace(Face& target)
{
	// Get a reference to the source Texturable in the clipboard
	Texturable& source = ShaderClipboard::Instance().getSource();

	// Apply a default projection to the face
	TextureProjection projection;

	// Copy just the shader name, the rest is default value
	target.setShader(source.patch->getShader());
	target.SetTexdef(projection);

    // To fix the extremely small scale we get when applying a default TextureProjection
    target.applyDefaultTextureScale();
}

// Function may leak a cmd::ExecutionFailure if the source/target combination doesn't work out
void applyClipboardToTexturable(Texturable& target, bool projected, bool entireBrush)
{
	// Get a reference to the source Texturable in the clipboard
	Texturable& source = ShaderClipboard::Instance().getSource();

	// Check the basic conditions
	if (!target.empty() && !source.empty())
    {
		// Do we have a Face to copy from?
		if (source.isFace())
        {
			if (target.isFace() && entireBrush)
            {
				// Copy Face >> Whole Brush
				for (Brush::const_iterator i = target.brush->begin();
					 i != target.brush->end();
					 i++)
				{
					applyClipboardFaceToFace(*(*i));
				}
			}
			else if (target.isFace() && !entireBrush) 
            {
				// Copy Face >> Face
				applyClipboardFaceToFace(*target.face);
			}
			else if (target.isPatch() && !entireBrush)
            {
				// Copy Face >> Patch

				// Set the shader name first
				target.patch->setShader(source.face->getShader());

				// Either paste the texture projected or naturally
				if (projected) 
                {
					target.patch->pasteTextureProjected(source.face);
				}
				else 
                {
					target.patch->pasteTextureNatural(source.face);
				}
			}
		}
		else if (source.isPatch())
        {
			// Source holds a patch
			if (target.isFace() && entireBrush) 
            {
				// Copy patch >> whole brush
				for (Brush::const_iterator i = target.brush->begin();
					 i != target.brush->end();
					 i++)
				{
					applyClipboardPatchToFace(*(*i));
				}
			}
			else if (target.isFace() && !entireBrush)
            {
				// Copy patch >> face
				applyClipboardPatchToFace(*target.face);
			}
			else if (target.isPatch() && !entireBrush) 
            {
				// Copy patch >> patch
				target.patch->setShader(source.patch->getShader());
				target.patch->pasteTextureNatural(*source.patch);
			}
		}
		else if (source.isShader())
        {
			if (target.isFace() && entireBrush)
            {
				// Copy patch >> whole brush
				for (Brush::const_iterator i = target.brush->begin();
					 i != target.brush->end();
					 i++)
				{
					(*i)->setShader(source.getShader());
				}
			}
			else if (target.isFace() && !entireBrush)
            {
				target.face->setShader(source.getShader());
			}
			else if (target.isPatch() && !entireBrush) 
            {
				target.patch->setShader(source.getShader());
			}
		}
	}
}

void pasteShader(SelectionTest& test, bool projected, bool entireBrush)
{
	// Construct the command string
	std::string command("pasteShader");
	command += (projected ? "Projected" : "Natural");
	command += (entireBrush ? "ToBrush" : "");

	UndoableCommand undo(command);

	// Initialise an empty Texturable structure
	Texturable target;

	// Find a suitable target Texturable
	ClosestTexturableFinder finder(test, target);
	GlobalSceneGraph().root()->traverseChildren(finder);

	if (target.isPatch() && entireBrush)
    {
		throw cmd::ExecutionFailure(
			_("Can't paste shader to entire brush.\nTarget is not a brush."));
	}
	else
    {
		// Pass the call to the algorithm function taking care of all the IFs
		applyClipboardToTexturable(target, projected, entireBrush);
	}

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void pasteTextureCoords(SelectionTest& test)
{
	UndoableCommand undo("pasteTextureCoordinates");

	// Initialise an empty Texturable structure
	Texturable target;

	// Find a suitable target Texturable
	ClosestTexturableFinder finder(test, target);
	GlobalSceneGraph().root()->traverseChildren(finder);

	// Get a reference to the source Texturable in the clipboard
	Texturable& source = ShaderClipboard::Instance().getSource();

	// Check the basic conditions
	if (target.isPatch() && source.isPatch())
    {
		// Check if the dimensions match, emit an error otherwise
		if (target.patch->getWidth() == source.patch->getWidth() &&
			target.patch->getHeight() == source.patch->getHeight())
		{
			target.patch->pasteTextureCoordinates(source.patch);
		}
		else
        {
            throw cmd::ExecutionFailure(
				_("Can't paste Texture Coordinates.\nTarget patch dimensions must match."));
		}
	}
	else
    {
		if (source.isPatch())
        {
			// Nothing to do, this works for patches only
			throw cmd::ExecutionFailure(_("Can't paste Texture Coordinates from patches to faces."));
		}
		else
        {
			// Nothing to do, this works for patches only
			throw cmd::ExecutionFailure(_("Can't paste Texture Coordinates from faces."));
		}
	}

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void pasteShaderName(SelectionTest& test) 
{
	// Initialise an empty Texturable structure
	Texturable target;

	// Find a suitable target Texturable
	ClosestTexturableFinder finder(test, target);
	GlobalSceneGraph().root()->traverseChildren(finder);

	if (target.empty())
	{
		return;
	}

	UndoableCommand undo("pasteTextureName");

	// Get a reference to the source Texturable in the clipboard
	Texturable& source = ShaderClipboard::Instance().getSource();

	if (target.isPatch())
	{
		target.patch->setShader(source.getShader());
	}
	else if (target.isFace())
	{
		target.face->setShader(source.getShader());
	}

	SceneChangeNotify();

	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void pickShaderFromSelection(const cmd::ArgumentList& args)
{
	GlobalShaderClipboard().clear();

	const SelectionInfo& info = GlobalSelectionSystem().getSelectionInfo();

	// Check for a single patch
	if (info.totalCount == 1 && info.patchCount == 1)
	{
		try
		{
			Patch& sourcePatch = getLastSelectedPatch();
			ShaderClipboard::Instance().setSource(sourcePatch);
		}
		catch (const InvalidSelectionException&)
		{
			throw cmd::ExecutionNotPossible(_("Can't copy Shader. Couldn't retrieve patch."));
		}
	}
	else if (selectedFaceCount() == 1)
	{
		try
		{
			Face& sourceFace = getLastSelectedFace();
			ShaderClipboard::Instance().setSource(sourceFace);
		}
		catch (const InvalidSelectionException&)
		{
			throw cmd::ExecutionNotPossible(_("Can't copy Shader. Couldn't retrieve face."));
		}
	}
	else
	{
		// Nothing to do, this works for patches only
		throw cmd::ExecutionNotPossible(_("Can't copy Shader. Please select a single face or patch."));
	}
}

/** greebo: This applies the clipboard to the visited faces/patches.
 */
class ClipboardShaderApplicator
{
	bool _natural;
public:
	ClipboardShaderApplicator(bool natural = false) :
		_natural(natural)
	{}

	void operator()(Patch& patch)
	{
        Texturable target;
        target.patch = &patch;
        target.node = patch.getPatchNode().shared_from_this();

        // Apply the shader (projected, not to the entire brush)
        applyClipboardToTexturable(target, !_natural, false);
	}

	void operator()(IFace& face)
	{
		Texturable target;
		target.face = &face;
		target.node = face.getBrush().getBrushNode().shared_from_this();

		// Apply the shader (projected, not to the entire brush)
		applyClipboardToTexturable(target, !_natural, false);
	}
};

void pasteShaderToSelection(const cmd::ArgumentList& args)
{
	if (ShaderClipboard::Instance().getSource().empty())
	{
		return;
	}

	// Start a new command
	UndoableCommand command("pasteShaderToSelection");

	ClipboardShaderApplicator applicator;
	GlobalSelectionSystem().foreachFace(applicator);
	GlobalSelectionSystem().foreachPatch(applicator);

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void pasteShaderNaturalToSelection(const cmd::ArgumentList& args)
{
	if (ShaderClipboard::Instance().getSource().empty())
	{
		return;
	}

	// Start a new command
	UndoableCommand command("pasteShaderNaturalToSelection");

	// greebo: Construct a visitor and traverse the selection
	ClipboardShaderApplicator applicator(true); // true == paste natural
	GlobalSelectionSystem().foreachFace(applicator);
	GlobalSelectionSystem().foreachPatch(applicator);

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

TextureProjection getSelectedTextureProjection()
{
	TextureProjection returnValue;

	if (selectedFaceCount() == 1)
	{
		// Get the last selected face instance from the global
		FaceInstance& faceInstance = *FaceInstance::Selection().back();
		faceInstance.getFace().GetTexdef(returnValue);
	}

	return returnValue;
}

Vector2 getSelectedFaceShaderSize()
{
	Vector2 returnValue(0,0);

	if (selectedFaceCount() == 1)
	{
		// Get the last selected face instance from the global
		FaceInstance& faceInstance = *FaceInstance::Selection().back();

		returnValue[0] = faceInstance.getFace().getFaceShader().getWidth();
		returnValue[1] = faceInstance.getFace().getFaceShader().getHeight();
	}

	return returnValue;
}

void fitTexture(const float repeatS, const float repeatT)
{
	UndoableCommand command("fitTexture");

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.fitTexture(repeatS, repeatT); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.SetTextureRepeat(repeatS, repeatT); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void fitTextureCmd(const cmd::ArgumentList& args)
{
	if (args.size() != 2)
	{
		rWarning() << "Usage: FitTexture <repeatU> <repeatV>" << std::endl;
		return;
	}

	fitTexture(args[0].getDouble(), args[1].getDouble())
}

void flipTexture(unsigned int flipAxis)
{
	UndoableCommand undo("flipTexture");

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.flipTexture(flipAxis); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.FlipTexture(flipAxis); });

	SceneChangeNotify();
}

void flipTextureS(const cmd::ArgumentList& args)
{
	flipTexture(0);
}

void flipTextureT(const cmd::ArgumentList& args)
{
	flipTexture(1);
}

void naturalTexture(const cmd::ArgumentList& args)
{
	UndoableCommand undo("naturalTexture");

    // Construct the "naturally" scaled Texdef structure
    TexDef shiftScaleRotation;

    float naturalScale = registry::getValue<float>("user/ui/textures/defaultTextureScale");

    shiftScaleRotation._scale[0] = naturalScale;
    shiftScaleRotation._scale[1] = naturalScale;

	// Patches
	GlobalSelectionSystem().foreachPatch(
        [] (Patch& patch) { patch.NaturalTexture(); }
    );
	GlobalSelectionSystem().foreachFace(
        [&] (Face& face) { face.setTexdef(shiftScaleRotation); }
    );

	SceneChangeNotify();

	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void applyTexDefToFaces(TexDef& texDef)
{
    UndoableCommand undo("textureDefinitionSetSelected");

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.setTexdef(texDef); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void shiftTexture(const Vector2& shift) 
{
	std::string command("shiftTexture: ");
	command += "s=" + string::to_string(shift[0]) + ", t=" + string::to_string(shift[1]);

	UndoableCommand undo(command);

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.shiftTexdefByPixels(shift[0], shift[1]); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.TranslateTexture(shift[0], shift[1]); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void scaleTexture(const Vector2& scale)
{
	std::string command("scaleTexture: ");
	command += "sScale=" + string::to_string(scale[0]) + ", tScale=" + string::to_string(scale[1]);

	UndoableCommand undo(command);

	// Prepare the according patch scale value (they are relatively scaled)
	Vector2 patchScale;

	// We need to have 1.05 for a +0.05 scale
	// and a 1/1.05 for a -0.05 scale
	for (int i = 0; i < 2; i++)
	{
		if (scale[i] >= 0.0f)
		{
			patchScale[i] = 1.0f + scale[i];
		}
		else
		{
			patchScale[i] = 1/(1.0f + fabs(scale[i]));
		}
	}

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.scaleTexdef(scale[0], scale[1]); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.ScaleTexture(patchScale[0], patchScale[1]); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void rotateTexture(const float angle)
{
	std::string command("rotateTexture: ");
	command += "angle=" + string::to_string(angle);

	UndoableCommand undo(command);

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.rotateTexdef(angle); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.RotateTexture(angle); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void shiftTextureLeft() {
	shiftTexture(Vector2(-registry::getValue<float>("user/ui/textures/surfaceInspector/hShiftStep"), 0.0f));
}

void shiftTextureRight() {
	shiftTexture(Vector2(registry::getValue<float>("user/ui/textures/surfaceInspector/hShiftStep"), 0.0f));
}

void shiftTextureUp() {
	shiftTexture(Vector2(0.0f, registry::getValue<float>("user/ui/textures/surfaceInspector/vShiftStep")));
}

void shiftTextureDown() {
	shiftTexture(Vector2(0.0f, -registry::getValue<float>("user/ui/textures/surfaceInspector/vShiftStep")));
}

void scaleTextureLeft() {
	scaleTexture(Vector2(-registry::getValue<float>("user/ui/textures/surfaceInspector/hScaleStep"), 0.0f));
}

void scaleTextureRight() {
	scaleTexture(Vector2(registry::getValue<float>("user/ui/textures/surfaceInspector/hScaleStep"), 0.0f));
}

void scaleTextureUp() {
	scaleTexture(Vector2(0.0f, registry::getValue<float>("user/ui/textures/surfaceInspector/vScaleStep")));
}

void scaleTextureDown() {
	scaleTexture(Vector2(0.0f, -registry::getValue<float>("user/ui/textures/surfaceInspector/vScaleStep")));
}

void rotateTextureClock() {
	rotateTexture(fabs(registry::getValue<float>("user/ui/textures/surfaceInspector/rotStep")));
}

void rotateTextureCounter() {
	rotateTexture(-fabs(registry::getValue<float>("user/ui/textures/surfaceInspector/rotStep")));
}

void rotateTexture(const cmd::ArgumentList& args) {
	if (args.size() != 1) {
		rMessage() << "Usage: TexRotate [+1|-1]" << std::endl;
		return;
	}

	if (args[0].getInt() > 0) {
		// Clockwise
		rotateTextureClock();
	}
	else {
		// Counter-Clockwise
		rotateTextureCounter();
	}
}

void scaleTexture(const cmd::ArgumentList& args) {
	if (args.size() != 1) {
		rMessage() << "Usage: TexScale 's t'" << std::endl;
		rMessage() << "       TexScale [up|down|left|right]" << std::endl;
		rMessage() << "Example: TexScale '0.05 0' performs"
			<< " a 105% scale in the s direction." << std::endl;
		rMessage() << "Example: TexScale up performs"
			<< " a vertical scale using the step value defined in the Surface Inspector."
			<< std::endl;
		return;
	}

	std::string arg = string::to_lower_copy(args[0].getString());

	if (arg == "up") {
		scaleTextureUp();
	}
	else if (arg == "down") {
		scaleTextureDown();
	}
	if (arg == "left") {
		scaleTextureLeft();
	}
	if (arg == "right") {
		scaleTextureRight();
	}
	else {
		// No special argument, retrieve the Vector2 argument and pass the call
		scaleTexture(args[0].getVector2());
	}
}

void shiftTextureCmd(const cmd::ArgumentList& args) {
	if (args.size() != 1) {
		rMessage() << "Usage: TexShift 's t'" << std::endl
			 << "       TexShift [up|down|left|right]" << std::endl
			 << "[up|down|left|right| takes the step values "
			 << "from the Surface Inspector." << std::endl;
		return;
	}

	std::string arg = string::to_lower_copy(args[0].getString());

	if (arg == "up") {
		shiftTextureUp();
	}
	else if (arg == "down") {
		shiftTextureDown();
	}
	if (arg == "left") {
		shiftTextureLeft();
	}
	if (arg == "right") {
		shiftTextureRight();
	}
	else {
		// No special argument, retrieve the Vector2 argument and pass the call
		shiftTexture(args[0].getVector2());
	}
}

void alignTexture(EAlignType align)
{
	std::string command("alignTexture: ");
	command += "edge=";

	switch (align)
	{
	case ALIGN_TOP:
		command += "top";
		break;
	case ALIGN_BOTTOM:
		command += "bottom";
		break;
	case ALIGN_LEFT:
		command += "left";
		break;
	case ALIGN_RIGHT:
		command += "right";
		break;
	};

	UndoableCommand undo(command);

	GlobalSelectionSystem().foreachFace([&] (Face& face) { face.alignTexture(align); });
	GlobalSelectionSystem().foreachPatch([&] (Patch& patch) { patch.alignTexture(align); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

void alignTextureCmd(const cmd::ArgumentList& args)
{
	if (args.size() != 1)
	{
		rMessage() << "Usage: TexAlign [top|bottom|left|right]" << std::endl;
		return;
	}

	std::string arg = string::to_lower_copy(args[0].getString());

	if (arg == "top")
	{
		alignTexture(ALIGN_TOP);
	}
	else if (arg == "bottom")
	{
		alignTexture(ALIGN_BOTTOM);
	}
	if (arg == "left")
	{
		alignTexture(ALIGN_LEFT);
	}
	if (arg == "right")
	{
		alignTexture(ALIGN_RIGHT);
	}
	else
	{
		rMessage() << "Usage: TexAlign [top|bottom|left|right]" << std::endl;
	}
}

void normaliseTexture(const cmd::ArgumentList& args)
{
	UndoableCommand undo("normaliseTexture");

	GlobalSelectionSystem().foreachFace([] (IFace& face) { face.normaliseTexture(); });
	GlobalSelectionSystem().foreachPatch([] (IPatch& patch) { patch.normaliseTexture(); });

	SceneChangeNotify();
	// Update the Texture Tools
	radiant::TextureChangedMessage::Send();
}

class ByShaderSelector :
	public scene::NodeVisitor
{
private:
	std::string _shaderName;

	bool _select;

public:
	ByShaderSelector(const std::string& shaderName, bool select = true) :
		_shaderName(shaderName),
		_select(select)
	{}

	bool pre(const scene::INodePtr& node)
	{
		Brush* brush = Node_getBrush(node);

		if (brush != NULL)
		{
			if (brush->hasShader(_shaderName))
			{
				Node_setSelected(node, _select);
			}

			return false; // don't traverse primitives
		}

		Patch* patch = Node_getPatch(node);

		if (patch != NULL)
		{
			if (patch->getShader() == _shaderName)
			{
				Node_setSelected(node, _select);
			}

			return false; // don't traverse primitives
		}

		return true;
	}
};

void selectItemsByShader(const std::string& shaderName)
{
	ByShaderSelector selector(shaderName, true);
	GlobalSceneGraph().root()->traverseChildren(selector);
}

void deselectItemsByShader(const std::string& shaderName)
{
	ByShaderSelector selector(shaderName, false);
	GlobalSceneGraph().root()->traverseChildren(selector);
}

void selectItemsByShaderCmd(const cmd::ArgumentList& args)
{
	if (args.size() < 1)
	{
		rMessage() << "Usage: SelectItemsByShader <SHADERNAME>" << std::endl;
		return;
	}

	selectItemsByShader(args[0].getString());
}

void deselectItemsByShaderCmd(const cmd::ArgumentList& args)
{
	if (args.size() < 1)
	{
		rMessage() << "Usage: DeselectItemsByShader <SHADERNAME>" << std::endl;
		return;
	}

	deselectItemsByShader(args[0].getString());
}

} // namespace algorithm

} // namespace selection
