#pragma once

#include "PropertyEditor.h"

namespace ui
{

/**
 * Property Editor for the "skin" key of models. This contains a text entry box
 * with a browse button that displays a SkinChooser, which is a dialog that
 * allows the selection of both matching and generic skins to apply to the given
 * model.
 */
class SkinPropertyEditor : 
	public PropertyEditor
{
private:
	// Keyvalue to set
	std::string _key;

private:

	void onBrowseButtonClick() override;

public:

	// Main constructor
    SkinPropertyEditor(wxWindow* parent, IEntitySelection& entities,
        const std::string& name, const std::string& options);

    static Ptr CreateNew(wxWindow* parent, IEntitySelection& entities,
                  const std::string& name, const std::string& options)
    {
        return std::make_shared<SkinPropertyEditor>(parent, entities, name, options);
    }
};

class SkinChooserDialogWrapper :
    public IPropertyEditorDialog
{
public:
    std::string runDialog(Entity* entity, const std::string& key) override;
};

} // namespace
