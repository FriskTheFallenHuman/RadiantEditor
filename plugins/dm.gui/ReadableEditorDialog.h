#ifndef _READABLE_EDITOR_DIALOG_H_
#define _READABLE_EDITOR_DIALOG_H_

#include "icommandsystem.h"

#include "gtkutil/window/BlockingTransientWindow.h"
#include "gui/GuiView.h"
#include <map>

#include "XDataLoader.h"

class Entity;

namespace ui
{


class ReadableEditorDialog :
	public gtkutil::BlockingTransientWindow
{
public:
	enum Result
	{
		RESULT_OK,
		RESULT_CANCEL,
		NUM_RESULTS,
	};

private:
	gui::GuiViewPtr _guiView;

	// A container for storing enumerated widgets
	std::map<int, GtkWidget*> _widgets;

	Result _result;

	// The entity we're working with
	Entity* _entity;

	// The XData-definition of the entity
	readable::XDataPtr _xData;

	// The filename of the XData definition
	std::string _xdFilename;

public:
	// Pass the working entity to the constructor
	ReadableEditorDialog(Entity* entity);

	static void RunDialog(const cmd::ArgumentList& args);

protected:
	virtual void _postShow();

private:
	void save();

	void initControlsFromEntity();

	GtkWidget* createEditPane();
	GtkWidget* createButtonPanel();

	static void onCancel(GtkWidget* widget, ReadableEditorDialog* self);
	static void onSave(GtkWidget* widget, ReadableEditorDialog* self);
	static void onBrowseXd(GtkWidget* widget, ReadableEditorDialog* self);
	static void onPrevPage(GtkWidget* widget, ReadableEditorDialog* self);
	static void onNextPage(GtkWidget* widget, ReadableEditorDialog* self);
	static void onBrowseGui(GtkWidget* widget, ReadableEditorDialog* self);
};

} // namespace ui

#endif /* _READABLE_EDITOR_DIALOG_H_ */
