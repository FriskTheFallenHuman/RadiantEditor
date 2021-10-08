#pragma once

#include "ibrush.h"
#include "math/Plane3.h"
#include "math/Matrix4.h"
#include "ExportUtil.h"

namespace map
{

class BrushDef3Exporter
{
public:

	// Writes a brushDef3 definition from the given brush to the given stream
	static void exportBrush(std::ostream& stream, const IBrushNodePtr& brushNode, bool writeContentsFlags = true)
	{
		const IBrush& brush = brushNode->getIBrush();

		// Brush decl header
		stream << "{" << std::endl;
		stream << "brushDef3" << std::endl;
		stream << "{" << std::endl;

		// Iterate over each brush face, exporting the tokens from all faces
		for (std::size_t i = 0; i < brush.getNumFaces(); ++i)
		{
			writeFace(stream, brush.getFace(i), writeContentsFlags, brush.getDetailFlag());
		}

		// Close brush contents and header
		stream << "}" << std::endl << "}" << std::endl;
	}

private:

	static void writeFace(std::ostream& stream, const IFace& face, bool writeContentsFlags, IBrush::DetailFlag detailFlag)
	{
		// greebo: Don't export faces with degenerate or empty windings (they are "non-contributing")
		if (face.getWinding().size() <= 2)
		{
			return;
		}

		// Write the plane equation
		const Plane3& plane = face.getPlane3();

		stream << "( ";
		writeDoubleSafe(plane.normal().x(), stream);
		stream << " ";
		writeDoubleSafe(plane.normal().y(), stream);
		stream << " ";
		writeDoubleSafe(plane.normal().z(), stream);
		stream << " ";
		writeDoubleSafe(-plane.dist(), stream); // negate d
		stream << " ";
		stream << ") ";

		// Write TexDef
		auto texdef = face.getProjectionMatrix();
		stream << "( ";

		stream << "( ";
		writeDoubleSafe(texdef.xx(), stream);
		stream << " ";
		writeDoubleSafe(texdef.yx(), stream);
		stream << " ";
		writeDoubleSafe(texdef.zx(), stream);
		stream << " ) ";

		stream << "( ";
		writeDoubleSafe(texdef.xy(), stream);
		stream << " ";
		writeDoubleSafe(texdef.yy(), stream);
		stream << " ";
		writeDoubleSafe(texdef.zy(), stream);
		stream << " ) ";

		stream << ") ";

		// Write Shader
		const std::string& shaderName = face.getShader();

		if (shaderName.empty()) {
			stream << "\"_default\" ";
		}
		else {
			stream << "\"" << shaderName << "\" ";
		}

		// Export (dummy) contents/flags
		if (writeContentsFlags)
		{
			stream << detailFlag << " 0 0";
		}

		stream << std::endl;
	}
};

}
