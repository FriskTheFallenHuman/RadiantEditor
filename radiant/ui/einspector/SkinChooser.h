#ifndef SKINCHOOSER_H_
#define SKINCHOOSER_H_

#include <gtk/gtkwidget.h>
#include <gtk/gtktreestore.h>

#include <string>

namespace ui
{

/** Dialog to allow selection of skins for a model entity. Skins are grouped
 * into two toplevel categories - matching skins which are associated with the
 * model, and all skins available.
 */
class SkinChooser
{
	// Main dialog widget
	GtkWidget* _widget;
	
	// Tree store and view
	GtkWidget* _treeView;
	GtkTreeStore* _treeStore;
	
	// The model name to use for skin matching
	std::string _model;

private:

	// Constructor creates GTK widgets
	SkinChooser();
	
	// Widget creation functions
	GtkWidget* createTreeView();
	GtkWidget* createButtons();
	
	// Show the dialog and block until selection is made
	std::string showAndBlock(const std::string& model);
	
	// Populate the tree with skins
	void populateSkins();
	
public:

	/** Display the dialog and return the skin chosen by the user, or an empty
	 * string if no selection was made. This static method maintains a singleton
	 * instance of the dialog, and enters are recursive GTK main loop during
	 * skin selection.
	 * 
	 * @param model
	 * The full VFS path of the model for which matching skins should be found.
	 */
	static std::string chooseSkin(const std::string& model);

};

}

#endif /*SKINCHOOSER_H_*/
