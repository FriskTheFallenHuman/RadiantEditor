#pragma once

#include "imap.h"
#include "ishaderclipboard.h"
#include "Texturable.h"
#include <sigc++/signal.h>
#include <sigc++/trackable.h>

namespace selection 
{

class ShaderClipboard :
	public IShaderClipboard,
	public sigc::trackable
{
private:
	// The source and target Texturables
	Texturable _source;

	bool _updatesDisabled;

    sigc::signal<void> _signalSourceChanged;

public:
	ShaderClipboard();

	/** greebo: Sets the source patch to the given <sourcePatch>
	 */
	void setSource(Patch& sourcePatch);

	/** greebo: Sets the source face to the given <sourceFace>
	 */
	void setSource(Face& sourceFace);

	/** greebo: Sets the source face to the given <shader>
	 */
	void setSource(std::string shader);

	/** greebo: Retrieves the current source Texturable
	 */
	Texturable& getSource();

    /**
     * Is emitted when the shader source changes.
     */
    sigc::signal<void> signal_sourceChanged() const;

	/** greebo: Clears both the source and target texturables.
	 * 			Call this as soon as the objects might be deleted
	 * 			or the map is changed.
	 */
	void clear();

	// IShaderClipboard implementation

	void pickFromSelectionTest(SelectionTest& test) override;
	void pasteShader(SelectionTest& test, PasteMode mode, bool pasteToAllFaces) override;
	void pasteTextureCoords(SelectionTest& test) override;
	void pasteMaterialName(SelectionTest& test) override;

private:
	// UndoSystem callbacks
	void onUndoRedoOperation();

	void onMapEvent(IMap::MapEvent ev);

	/** greebo: Updates the shader information in the status bar.
	 */
	void updateStatusText();

	/** greebo: Sets the media browser / texwindow focus to the
	 * 			new source shader.
	 */
	void updateMediaBrowsers();

	/** greebo: Retrieves the best texturable object from the
	 * 			given SelectionTest.
	 */
	Texturable getTexturable(SelectionTest& test);
};

} // namespace selection
