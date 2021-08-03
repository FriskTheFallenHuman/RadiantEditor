/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "dds.h"

#include <stdlib.h>
#include <algorithm>
#include <map>

#include "ifilesystem.h"
#include "iarchive.h"
#include "idatastream.h"

#include "ddslib.h"
#include "util/Noncopyable.h"
#include "RGBAImage.h"

namespace image
{

// Metadata for a single MipMap level
struct MipMapInfo
{
    /// Width in pixels
    std::size_t width = 0;

    /// Height in pixels
    std::size_t height = 0;

    /// Size in bytes
    std::size_t size = 0;

    /// Offset from data buffer start to the beginning of this mipmap
    std::size_t offset = 0;
};
typedef std::vector<MipMapInfo> MipMapInfoList;

// Image subclass for DDS images
class DDSImage: public Image, public util::Noncopyable
{
    // The actual pixels
    mutable std::vector<uint8_t> _pixelData;

    // The GL format of the texture data, and a boolean flag to indicate if we
    // need to upload with glCompressedTexImage2D rather than glTexImage2D
    GLenum _format = 0;
    bool _compressed = true;

    // Metadata for each mipmap. All pixel data is stored in _pixelData, with
    // the offset to each mipmap stored in the _mipMapInfo list.
    MipMapInfoList _mipMapInfo;

public:

    // Pass the required memory size to the constructor
    DDSImage(std::size_t size): _pixelData(size)
    {}

    // Set the compression format
    void setFormat(GLenum format, bool compressed)
    {
        _format = format;
        _compressed = compressed;
    }

    // Add a new mipmap with the given parameters and return a pointer to its
    // allocated byte data
    uint8_t* addMipMap(const MipMapInfo& info)
    {
        // Store MipMapInfo metadata in our list
        _mipMapInfo.push_back(info);

        // Return the absolute pointer to the new mipmap's byte data
        assert(info.offset < _pixelData.size());
        return _pixelData.data() + info.offset;
    }

    /* Image implementation */
    uint8_t* getPixels() const override { return _pixelData.data(); }
    std::size_t getWidth(std::size_t level = 0) const override
    {
        return _mipMapInfo[level].width;
    }
    std::size_t getHeight(std::size_t level = 0) const override
    {
        return _mipMapInfo[level].height;
    }
    std::size_t getLevels() const override { return _mipMapInfo.size(); }
    bool isPrecompressed() const override { return _compressed; }
    GLenum getGLFormat() const override { return _format; }

    /* BindableTexture implementation */
    TexturePtr bindTexture(const std::string& name) const
    {
        // Allocate a new texture number and store it into the Texture structure
        GLuint textureNum;
        glGenTextures(1, &textureNum);
        glBindTexture(GL_TEXTURE_2D, textureNum);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        debug::checkGLErrors("before uploading DDS mipmaps");
        for (std::size_t i = 0; i < _mipMapInfo.size(); ++i)
        {
            const MipMapInfo& mipMap = _mipMapInfo[i];

            if (_compressed)
            {
                glCompressedTexImage2D(
                    GL_TEXTURE_2D, static_cast<GLint>(i), _format,
                    static_cast<GLsizei>(mipMap.width),
                    static_cast<GLsizei>(mipMap.height),
                    0, static_cast<GLsizei>(mipMap.size),
                    _pixelData.data() + mipMap.offset
                );

                // If the upload failed but this is not level 0, we can fall
                // back to regenerating the mipmaps.
                if (debug::checkGLErrors("uploading DDS mipmap") != GL_NO_ERROR
                    && i > 0)
                {
                    rWarning() << "DDSImage: failed to upload mipmap " << (i+1)
                               << " of " << _mipMapInfo.size()
                               << " [" << mipMap.width << "x" << mipMap.height << "],"
                               << " regenerating mipmaps.\n";
                    glGenerateMipmap(GL_TEXTURE_2D);

                    // Don't process any more mipmaps
                    break;
                }
            }
            else
            {
                // For uncompressed textures the format specifies the layout in
                // memory, not the internal format we want OpenGL to use (which
                // is always GL_RGB).
                glTexImage2D(
                    GL_TEXTURE_2D, static_cast<GLint>(i), GL_RGB,
                    static_cast<GLsizei>(mipMap.width),
                    static_cast<GLsizei>(mipMap.height),
                    0, _format, GL_UNSIGNED_BYTE,
                    _pixelData.data() + mipMap.offset
                );
            }

            // Handle unsupported format error
            if (glGetError() == GL_INVALID_ENUM)
            {
                rError() << "[DDSImage] Unable to bind texture '" << name
                         << "': unsupported texture format " << _format
                         << (_compressed ? " (compressed)" : " (uncompressed)")
                         << std::endl;

                return TexturePtr();
            }

            debug::assertNoGlErrors();
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(_mipMapInfo.size() - 1));

        // Un-bind the texture
        glBindTexture(GL_TEXTURE_2D, 0);

        // Create and return texture object
        BasicTexture2DPtr texObj(new BasicTexture2D(textureNum, name));
        texObj->setWidth(getWidth());
        texObj->setHeight(getHeight());

        debug::assertNoGlErrors();

        return texObj;
    }
};
typedef std::shared_ptr<DDSImage> DDSImagePtr;

// Map DDS FOURCC values to GLenum compression formats
static const std::map<std::string, GLenum> GL_FMT_FOR_FOURCC
{
    { "DXT1", GL_COMPRESSED_RGBA_S3TC_DXT1_EXT },
    { "DXT3", GL_COMPRESSED_RGBA_S3TC_DXT3_EXT },
    { "DXT5", GL_COMPRESSED_RGBA_S3TC_DXT5_EXT },
    { "ATI2", GL_COMPRESSED_RG_RGTC2 }
};

// Map uncompressed DDS bit depths to GLenum memory layouts
static const std::map<int, GLenum> GL_FMT_FOR_BITDEPTH
{
    { 24, GL_BGR },
    { 32, GL_BGRA }
};

DDSImagePtr LoadDDSFromStream(InputStream& stream)
{
    // Load the header
    typedef StreamBase::byte_type byteType;
    DDSHeader header;
    stream.read(reinterpret_cast<byteType*>(&header), sizeof(header));

    // Reject any invalid DDS structure
    if (!header.isValid())
    {
        rError() << "Invalid DDS header" << std::endl;
    }

    // Extract basic metadata: width, height, format and mipmap count
    int width = header.getWidth(), height = header.getHeight();
    std::string compressionFormat = header.getCompressionFormat();
    int bitDepth = header.getRGBBits();
    std::size_t mipMapCount = header.getMipMapCount();

    MipMapInfoList mipMapInfo;
    mipMapInfo.resize(mipMapCount);

    // Calculate the total memory requirements (greebo: DXT1 has 8 bytes per block)
    std::size_t blockBytes = (compressionFormat == "DXT1") ? 8 : 16;

    std::size_t size = 0;
    std::size_t offset = 0;

    for (std::size_t i = 0; i < mipMapCount; ++i)
    {
        // Create a new mipmap structure
        MipMapInfo& mipMap = mipMapInfo[i];

        mipMap.offset = offset;
        mipMap.width = width;
        mipMap.height = height;

        // Calculate size in bytes for this mipmap. For compressed formats,
        // this is based on the block size, otherwise it derives from the bytes
        // per pixel.
        if (header.isCompressed())
            mipMap.size = ((width + 3) / 4) * ((height + 3) / 4) * blockBytes;
        else
            mipMap.size = width * height * (bitDepth / 8);

        // Update the offset for the next mipmap
        offset += mipMap.size;

        // Increase the size counter
        size += mipMap.size;

        // Go to the next mipmap
        width = std::max(width/2, 1);
        height = std::max(height/2, 1);
    }

    // Allocate a new DDS image with that size
    DDSImagePtr image(new DDSImage(size));

    // Set the format of this DDS image
    if (GL_FMT_FOR_FOURCC.count(compressionFormat) == 1)
        image->setFormat(GL_FMT_FOR_FOURCC.at(compressionFormat), true);
    else if (GL_FMT_FOR_BITDEPTH.count(bitDepth) == 1)
        image->setFormat(GL_FMT_FOR_BITDEPTH.at(bitDepth), false);
    else
        rError() << "Unknown DDS format (" << compressionFormat << ")" << std::endl;

    // Load the mipmaps into the allocated memory
    for (std::size_t i = 0; i < mipMapInfo.size(); ++i)
    {
        // Appaned a new mipmap and store the offset
        const MipMapInfo& mipMap = mipMapInfo[i];
        uint8_t* mipMapBytes = image->addMipMap(mipMap);

        // Read the data into the DDSImage's memory
        std::size_t bytesRead = stream.read(
            reinterpret_cast<byteType*>(mipMapBytes), mipMap.size
        );
        assert(bytesRead == mipMap.size);
    }

    return image;
}

ImagePtr LoadDDS(ArchiveFile& file) {
    return LoadDDSFromStream(file.getInputStream());
}

ImagePtr DDSLoader::load(ArchiveFile& file) const
{
    // Pass the call to the according load function
    return LoadDDS(file);
}

ImageTypeLoader::Extensions DDSLoader::getExtensions() const
{
    Extensions extensions;
    extensions.push_back("dds");
    return extensions;
}

}
