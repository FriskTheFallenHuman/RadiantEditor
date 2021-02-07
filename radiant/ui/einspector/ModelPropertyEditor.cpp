#include "ModelPropertyEditor.h"
#include "PropertyEditorFactory.h"

#include "ui/modelselector/ModelSelector.h"
#include "ui/particles/ParticlesChooser.h"

#include "i18n.h"
#include "ientity.h"
#include "iselection.h"
#include "scenelib.h"
#include "wxutil/dialog/MessageBox.h"

#include <wx/panel.h>
#include <wx/button.h>
#include "wxutil/Bitmap.h"
#include <wx/sizer.h>

#include "SkinChooser.h"

namespace ui
{

ModelPropertyEditor::ModelPropertyEditor()
{}

// Main constructor
ModelPropertyEditor::ModelPropertyEditor(wxWindow* parent, Entity* entity,
									     const std::string& name,
									     const std::string& options)
: PropertyEditor(entity),
  _key(name)
{
	// Construct the main widget (will be managed by the base class)
	wxPanel* mainVBox = new wxPanel(parent, wxID_ANY);
	mainVBox->SetSizer(new wxBoxSizer(wxHORIZONTAL));

	// Register the main widget in the base class
	setMainWidget(mainVBox);

	// Browse button for models
	wxButton* browseButton = new wxButton(mainVBox, wxID_ANY, _("Choose model..."));
	browseButton->SetBitmap(PropertyEditorFactory::getBitmapFor("model"));
	browseButton->Bind(wxEVT_BUTTON, &ModelPropertyEditor::_onModelButton, this);

	wxButton* skinButton = new wxButton(mainVBox, wxID_ANY, _("Choose skin..."));
	skinButton->SetBitmap(PropertyEditorFactory::getBitmapFor("skin"));
	skinButton->Bind(wxEVT_BUTTON, &ModelPropertyEditor::_onSkinButton, this);

	// Browse button for particles
	wxButton* particleButton = new wxButton(mainVBox, wxID_ANY, _("Choose particle..."));
	particleButton->SetBitmap(wxutil::GetLocalBitmap("particle16.png"));
	particleButton->Bind(wxEVT_BUTTON, &ModelPropertyEditor::_onParticleButton, this);

	// The panel will use the entire height of the editor frame in the entity inspector
	// use vertical centering to position it in the middle
	mainVBox->GetSizer()->Add(browseButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	mainVBox->GetSizer()->Add(skinButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	mainVBox->GetSizer()->Add(particleButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
}

void ModelPropertyEditor::_onModelButton(wxCommandEvent& ev)
{
	// Use the ModelSelector to choose a model
	ModelSelectorResult result = ModelSelector::chooseModel(
		_entity->getKeyValue(_key), false, false // pass the current model, don't show options or skins
	);

    UndoableCommand cmd("setModelProperty");

    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        Entity* entity = Node_getEntity(node);
        std::string prevModel = entity->getKeyValue(_key);
        std::string name = entity->getKeyValue("name");

        bool wasBrushBasedModel = prevModel == name;

        if (!result.model.empty())
        {
            bool willBeBrushBasedModel = result.model == name;

            // Check if any brushes should be removed, but inform the user about this
            if (!willBeBrushBasedModel && wasBrushBasedModel && hasChildPrimitives(node))
            {
                // Warn the user and proceed
                wxutil::Messagebox::Show(_("Warning: "),
                    _("Changing this entity's model to the selected value will\nremove all child primitives from it:\n") + name,
                    IDialog::MessageType::MESSAGE_WARNING);

                scene::NodeRemover walker;
                node->traverseChildren(walker);
            }

            // Save the model key now
            entity->setKeyValue(_key, result.model);
        }
    });
}

void ModelPropertyEditor::_onParticleButton(wxCommandEvent& ev)
{
	// Invoke ParticlesChooser
    std::string currentSelection = _entity->getKeyValue(_key);
	std::string particle = ParticlesChooser::ChooseParticle(currentSelection);

	if (!particle.empty())
	{
		setKeyValue(_key, particle);
	}
}

void ModelPropertyEditor::_onSkinButton(wxCommandEvent& ev)
{
	// Display the SkinChooser to get a skin from the user
	std::string modelName = _entity->getKeyValue("model");
	std::string prevSkin = _entity->getKeyValue("skin");
	std::string skin = SkinChooser::chooseSkin(modelName, prevSkin);

	if (skin != prevSkin)
	{
		// Apply the key to the entity
		setKeyValue("skin", skin);
	}
}

}
