#pragma once

#include "wxutil/Bitmap.h"
#include <wx/artprov.h>

namespace wxutil
{

/**
 * \brief
 * Get a wxBitmap from the local "radianteditor" art provider
 *
 * \param name
 * Image file name with no prefix, e.g. "something.png"
 */
inline wxBitmap GetLocalBitmap(const std::string& name)
{
    return wxArtProvider::GetBitmap("radianteditor:" + name);
}

/**
 * \brief
 * Get a wxBitmap from the local "radianteditor" art provider, passing the given
 * art client along.
 *
 * \param name
 * Image file name with no prefix, e.g. "something.png"
 */
inline wxBitmap GetLocalBitmap(const std::string& name, const wxArtClient& client)
{
    return wxArtProvider::GetBitmap("radianteditor:" + name, client);
}

}
