#pragma once

#include <map>
#include "ientity.h"
#include <memory>

#include <sigc++/connection.h>
#include <sigc++/trackable.h>
#include <wx/panel.h>

class ISelectable;
class Entity;
class wxStaticText;
class wxScrolledWindow;
class wxSizer;

namespace ui
{

class AIEditingPanel;
typedef std::shared_ptr<AIEditingPanel> AIEditingPanelPtr;

class SpawnargLinkedCheckbox;
class SpawnargLinkedSpinButton;

class AIEditingPanel : 
	public wxPanel,
	public Entity::Observer,
	public sigc::trackable
{
private:
	sigc::connection _selectionChangedSignal;

	wxScrolledWindow* _mainPanel;

	bool _queueUpdate;

	typedef std::map<std::string, SpawnargLinkedCheckbox*> CheckboxMap;
	CheckboxMap _checkboxes;

	typedef std::map<std::string, SpawnargLinkedSpinButton*> SpinButtonMap;
	SpinButtonMap _spinButtons;

	typedef std::map<std::string, wxStaticText*> LabelMap;
	LabelMap _labels;

	Entity* _entity;

	sigc::connection _undoHandler;
	sigc::connection _redoHandler;

public:
	AIEditingPanel(wxWindow* parent);
	~AIEditingPanel() override;

	void onKeyInsert(const std::string& key, EntityKeyValue& value) override;
    void onKeyChange(const std::string& key, const std::string& val) override;
	void onKeyErase(const std::string& key, EntityKeyValue& value) override;

	void postUndo();
	void postRedo();

protected:
	void onPaint(wxPaintEvent& ev);
	void onBrowseButton(wxCommandEvent& ev, const std::string& key);

private:
	void constructWidgets();
	wxSizer* createSpinButtonHbox(SpawnargLinkedSpinButton* spinButton);
	wxStaticText* createSectionLabel(const std::string& text);
	void createChooserRow(wxSizer* table, const std::string& rowLabel, 
									  const std::string& buttonLabel, const std::string& buttonIcon,
									  const std::string& key);

	void onSelectionChanged(const ISelectable& selectable);

	void rescanSelection();

	Entity* getEntityFromSelection();
	void updateWidgetsFromSelection();
	void updatePanelSensitivity();
};

} // namespace
