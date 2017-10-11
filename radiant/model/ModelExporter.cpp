#include "ModelExporter.h"

#include "i18n.h"
#include "ibrush.h"
#include "iclipper.h" // for caulk shader registry key
#include "ipatch.h"
#include "itextstream.h"
#include "imodel.h"
#include "os/fs.h"
#include "entitylib.h"
#include "registry/registry.h"
#include <stdexcept>
#include <fstream>

namespace model
{

namespace
{

// Adapter methods to convert brush vertices to ArbitraryMeshVertex type
ArbitraryMeshVertex convertWindingVertex(const WindingVertex& in)
{
	ArbitraryMeshVertex out;

	out.vertex = in.vertex;
	out.normal = in.normal;
	out.texcoord = in.texcoord;
	out.bitangent = in.bitangent;
	out.tangent = in.tangent;
	out.colour.set(1.0, 1.0, 1.0);

	return out;
}

// Adapter methods to convert patch vertices to ArbitraryMeshVertex type
ArbitraryMeshVertex convertPatchVertex(const VertexNT& in)
{
	ArbitraryMeshVertex out;

	out.vertex = in.vertex;
	out.normal = in.normal;
	out.texcoord = in.texcoord;
	out.colour.set(1.0, 1.0, 1.0);

	return out;
}

}

ModelExporter::ModelExporter(const model::IModelExporterPtr& exporter) :
	_exporter(exporter),
	_skipCaulk(false),
	_caulkMaterial(registry::getValue<std::string>(RKEY_CLIPPER_CAULK_SHADER)),
	_centerObjects(false),
	_origin(0,0,0),
	_useOriginAsCenter(false),
	_centerTransform(Matrix4::getIdentity())
{
	if (!_exporter)
	{
		rError() << "Cannot save out scaled models, no exporter found." << std::endl;
		return;
	}
}

void ModelExporter::setSkipCaulkMaterial(bool skipCaulk)
{
	_skipCaulk = skipCaulk;
}

void ModelExporter::setCenterObjects(bool centerObjects)
{
	_centerObjects = centerObjects;
}

void ModelExporter::setOrigin(const Vector3& origin)
{
	_origin = origin;
	_useOriginAsCenter = true;
}

bool ModelExporter::pre(const scene::INodePtr& node)
{
	// Skip worldspawn
	if (Node_isWorldspawn(node)) return true;

	_nodes.push_back(node);

	return true;
}

const Matrix4& ModelExporter::getCenterTransform()
{
	return _centerTransform;
}

void ModelExporter::processNodes()
{
	AABB bounds = calculateModelBounds();

	if (_centerObjects)
	{
		// Depending on the center point, we need to use the object bounds
		// or just the translation towards the user-defined origin, ignoring bounds
		_centerTransform = _useOriginAsCenter ?
			Matrix4::getTranslation(-_origin) :
			Matrix4::getTranslation(-bounds.origin);
	}

	for (const scene::INodePtr& node : _nodes)
	{
		if (Node_isModel(node))
		{
			model::ModelNodePtr modelNode = Node_getModel(node);

			// Push the geometry into the exporter
			model::IModel& model = modelNode->getIModel();

			Matrix4 exportTransform = node->localToWorld().getPremultipliedBy(_centerTransform);

			for (int s = 0; s < model.getSurfaceCount(); ++s)
			{
				const model::IModelSurface& surface = model.getSurface(s);

				if (isExportableMaterial(surface.getDefaultMaterial()))
				{
					_exporter->addSurface(surface, exportTransform);
				}
			}
		}
		else if (Node_isBrush(node))
		{
			processBrush(node);
		}
		else if (Node_isPatch(node))
		{
			processPatch(node);
		}
	}
}

AABB ModelExporter::calculateModelBounds()
{
	AABB bounds;

	for (const scene::INodePtr& node : _nodes)
	{
		// Only consider the node types supported by processNodes()
		if (!Node_isModel(node) && !Node_isBrush(node) && !Node_isPatch(node))
		{
			continue;
		}

		bounds.includeAABB(node->worldAABB());
	}

	return bounds;
}

void ModelExporter::processPatch(const scene::INodePtr& node)
{
	IPatch* patch = Node_getIPatch(node);

	if (patch == nullptr) return;

	const std::string& materialName = patch->getShader();

	if (!isExportableMaterial(materialName)) return;

	PatchMesh mesh = patch->getTesselatedPatchMesh();

	std::vector<model::ModelPolygon> polys;

	for (std::size_t h = 0; h < mesh.height - 1; ++h)
	{
		for (std::size_t w = 0; w < mesh.width - 1; ++w)
		{
			model::ModelPolygon poly;

			poly.a = convertPatchVertex(mesh.vertices[w + (h*mesh.width)]);
			poly.b = convertPatchVertex(mesh.vertices[w + 1 + (h*mesh.width)]);
			poly.c = convertPatchVertex(mesh.vertices[w + mesh.width + (h*mesh.width)]);

			polys.push_back(poly);
				
			poly.a = convertPatchVertex(mesh.vertices[w + 1 + (h*mesh.width)]);
			poly.b = convertPatchVertex(mesh.vertices[w + 1 + mesh.width + (h*mesh.width)]);
			poly.c = convertPatchVertex(mesh.vertices[w + mesh.width + (h*mesh.width)]);

			polys.push_back(poly);
		}
	}

	Matrix4 exportTransform = node->localToWorld().getPremultipliedBy(_centerTransform);

	_exporter->addPolygons(materialName, polys, exportTransform);
}

void ModelExporter::processBrush(const scene::INodePtr& node)
{
	IBrush* brush = Node_getIBrush(node);

	if (brush == nullptr) return;

	Matrix4 exportTransform = node->localToWorld().getPremultipliedBy(_centerTransform);

	for (std::size_t b = 0; b < brush->getNumFaces(); ++b)
	{
		const IFace& face = brush->getFace(b);

		const std::string& materialName = face.getShader();

		if (!isExportableMaterial(materialName)) continue;

		const IWinding& winding = face.getWinding();

		std::vector<model::ModelPolygon> polys;

		if (winding.size() < 3)
		{
			rWarning() << "Skipping face with less than 3 winding verts" << std::endl;
			continue;
		}

		// Create triangles for this winding 
		for (std::size_t i = 1; i < winding.size() - 1; ++i)
		{
			model::ModelPolygon poly;

			poly.a = convertWindingVertex(winding[i + 1]);
			poly.b = convertWindingVertex(winding[i]);
			poly.c = convertWindingVertex(winding[0]);

			polys.push_back(poly);
		}

		_exporter->addPolygons(materialName, polys, exportTransform);
	}
}

bool ModelExporter::isExportableMaterial(const std::string& materialName)
{
	return !_skipCaulk || materialName != _caulkMaterial;
}

void ModelExporter::ExportToPath(const model::IModelExporterPtr& exporter,
	const std::string& outputPath, const std::string& filename)
{
	fs::path targetPath = outputPath;

	// Open a temporary file (leading underscore)
	fs::path tempFile = targetPath / ("_" + filename);

	std::ofstream::openmode mode = std::ofstream::out;

	if (exporter->getFileFormat() == model::IModelExporter::Format::Binary)
	{
		mode |= std::ios::binary;
	}

	std::ofstream tempStream(tempFile.string().c_str(), mode);

	if (!tempStream.is_open())
	{
		throw std::runtime_error(
			fmt::format(_("Cannot open file for writing: {0}"), tempFile.string()));
	}

	exporter->exportToStream(tempStream);

	tempStream.close();

	// The full OS path to the output file
	targetPath /= filename;

	if (fs::exists(targetPath))
	{
		try
		{
			fs::rename(targetPath, targetPath.string() + ".bak");
		}
		catch (fs::filesystem_error& e)
		{
			rError() << "Could not rename the existing file to .bak: " << targetPath.string() << std::endl
				<< e.what() << std::endl;

			throw std::runtime_error(
				fmt::format(_("Could not rename the existing file to .bak: {0}"), tempFile.string()));
		}
	}

	try
	{
		fs::rename(tempFile, targetPath);
	}
	catch (fs::filesystem_error& e)
	{
		rError() << "Could not rename the temporary file " << tempFile.string() << std::endl
			<< e.what() << std::endl;

		throw std::runtime_error(
			fmt::format(_("Could not rename the temporary file: {0}"), tempFile.string()));
	}
}

}
