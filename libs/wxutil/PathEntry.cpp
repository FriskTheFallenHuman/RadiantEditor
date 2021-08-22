#include "PathEntry.h"

#include "imodule.h"
#include "i18n.h"

#include <wx/sizer.h>
#include <wx/bmpbuttn.h>
#include <wx/textctrl.h>
#include <wx/bitmap.h>

#include "FileChooser.h"
#include "DirChooser.h"
#include "os/path.h"

namespace wxutil
{

PathEntry::PathEntry(wxWindow* parent, const std::string& fileType, bool open) :
	PathEntry(parent, false, open, fileType, std::string())
{}

PathEntry::PathEntry(wxWindow* parent, bool foldersOnly) :
	PathEntry(parent, foldersOnly, true, std::string(), std::string())
{}

PathEntry::PathEntry(wxWindow* parent, const char* fileType, bool open) :
	PathEntry(parent, std::string(fileType), open, std::string())
{}

PathEntry::PathEntry(wxWindow* parent, const std::string& fileType, bool open, const std::string& defaultExt) :
	PathEntry(parent, false, open, fileType, defaultExt)
{}

PathEntry::PathEntry(wxWindow* parent, bool foldersOnly, bool open, 
					 const std::string& fileType, const std::string& defaultExt) :
	wxPanel(parent, wxID_ANY),
	_fileType(fileType),
	_defaultExt(defaultExt),
	_open(open),
	_askForOverwrite(true)
{
	SetSizer(new wxBoxSizer(wxHORIZONTAL));

	// path entry
    _entry = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    _entry->Bind(wxEVT_TEXT_ENTER, [&](wxCommandEvent& ev)
    {
        // Fire the PathEntryChanged event on enter
        wxQueueEvent(_entry->GetEventHandler(), new wxCommandEvent(EV_PATH_ENTRY_CHANGED, _entry->GetId()));
    });
    _entry->Bind(wxEVT_TEXT, [&](wxCommandEvent& ev)
    {
        // Fire the PathEntryChanged event on typing
        wxQueueEvent(_entry->GetEventHandler(), new wxCommandEvent(EV_PATH_ENTRY_CHANGED, _entry->GetId()));
    });

	// Generate browse button image
	const auto& appCtx = module::GlobalModuleRegistry().getApplicationContext();
	std::string fullFileName = appCtx.getBitmapsPath() + "ellipsis.png";

	wxImage image(fullFileName);

	// browse button
	_button = new wxBitmapButton(this, wxID_ANY, wxBitmap(image));

	// Connect the button
	if (foldersOnly)
	{
		_button->Connect(wxEVT_BUTTON, wxCommandEventHandler(PathEntry::onBrowseFolders), NULL, this);
	}
	else
	{
		_button->Connect(wxEVT_BUTTON, wxCommandEventHandler(PathEntry::onBrowseFiles), NULL, this);
	}

	GetSizer()->Add(_entry, 1, wxEXPAND | wxRIGHT, 6);
	GetSizer()->Add(_button, 0, wxEXPAND);
}

void PathEntry::setValue(const std::string& val)
{
	_entry->SetValue(val);
    _entry->SetInsertionPointEnd();
}

std::string PathEntry::getValue() const
{
	return _entry->GetValue().ToStdString();
}

wxTextCtrl* PathEntry::getEntryWidget()
{
	return _entry;
}

void PathEntry::setDefaultExtension(const std::string& defaultExt)
{
	_defaultExt = defaultExt;
}

void PathEntry::setAskForOverwrite(bool ask)
{
	_askForOverwrite = ask;
}

void PathEntry::onBrowseFiles(wxCommandEvent& ev)
{
	wxWindow* topLevel = wxGetTopLevelParent(this);

    FileChooser fileChooser(topLevel, _("Choose File"), _open, _fileType, _defaultExt);

	// Propagate the setting of this picker
	fileChooser.askForOverwrite(_askForOverwrite);

	auto curValue = getValue();

	if (!curValue.empty())
	{
		// Set the filename to the one contained in the text box
		fileChooser.setCurrentFile(os::getFilename(curValue));

		// If there's a non-empty path, point it to the specified folder
		auto curFolder = os::getDirectory(curValue);

		if (!curFolder.empty())
		{
			fileChooser.setCurrentPath(curFolder);
		}
	}

	auto filename = fileChooser.display();

	topLevel->Show();

	if (!filename.empty())
	{
		setValue(filename);

        // Fire the PathEntryChanged event
        wxQueueEvent(GetEventHandler(), new wxCommandEvent(EV_PATH_ENTRY_CHANGED, _entry->GetId()));
	}
}

void PathEntry::onBrowseFolders(wxCommandEvent& ev)
{
	wxWindow* topLevel = wxGetTopLevelParent(this);

    DirChooser dirChooser(topLevel, _("Choose Directory"));

	std::string curEntry = getValue();

	if (!path_is_absolute(curEntry.c_str()))
	{
		curEntry.clear();
	}

	dirChooser.setCurrentPath(curEntry);

	std::string filename = dirChooser.display();

	topLevel->Show();

	if (!filename.empty())
	{
		setValue(filename);

        // Fire the PathEntryChanged event
        wxQueueEvent(GetEventHandler(), new wxCommandEvent(EV_PATH_ENTRY_CHANGED, _entry->GetId()));
	}
}

wxDEFINE_EVENT(EV_PATH_ENTRY_CHANGED, wxCommandEvent);

} // namespace wxutil
