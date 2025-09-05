#include "stdafx.h"
#include "ContentLoader.h"
#include "Utils/StringUtils.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_VORBIS_IMPLEMENTATION

#pragma warning(disable: 6262)
#pragma warning(disable: 26819)
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_image_resize2.h"
#include "stb/stb_rect_pack.h"
#include "stb/stb_vorbis.h"
#pragma warning(default: 26819)
#pragma warning(default: 6262)

using namespace ContentData;


#pragma region Mesh
static bool ReadWavefront(const char *path, 
	std::vector<RawPosition> &vertexPositions, 
	std::vector<RawTexCoord> &vertexTexCoords,
	std::vector<RawNormal> &vertexNormals,
	std::vector<std::vector<RawIndex>> &indexGroups,
	std::vector<std::string> &mtlGroups,
	std::string &mtlFile)
{
	std::ifstream fileStream(path);
	std::string line;

	bool begunReadingSubMesh = false;

	while (std::getline(fileStream, line))
	{
		size_t commentStart = line.find_first_of('#');
		if (commentStart != std::string::npos)
			line = line.substr(0, commentStart); // Exclude comments

		if (line.empty())
			continue; // Skip filler line

		if (line.length() > 1)
			if (line[0] == 'f' && line[1] == ' ')
			{
				char sep = '/';
				if (line.find('\\', 0) != std::string::npos)
					sep = '\\';

				// Replace all instances of separator with whitespace.
				std::string::size_type n = 0;
				while ((n = line.find(sep, n)) != std::string::npos)
				{
					line.replace(n, 1, " ");
					n++;
				}
			}

		std::istringstream segments(line);

		std::string dataType;
		if (!(segments >> dataType))
		{
			ErrMsgF("Failed to get data type from line \"{}\", file \"{}\"!", line, path);
			return false;
		}

		if (dataType == "mtllib")
		{ // Define where to find materials
			if (!(segments >> mtlFile))
			{
				ErrMsgF("Failed to get mtl name from line \"{}\", file \"{}\"!", line, path);
				return false;
			}
		}
		else if (dataType == "g")
		{ // Mesh Group
			begunReadingSubMesh = true;
			indexGroups.emplace_back();
			mtlGroups.emplace_back("");
		}
		else if (dataType == "o")
		{ // Mesh Object
			begunReadingSubMesh = true;
			indexGroups.emplace_back();
			mtlGroups.emplace_back("");
		}
		else if (dataType == "v")
		{ // Vertex Position
			float x, y, z;
			if (!(segments >> x >> y >> z))
			{
				ErrMsgF(R"(Failed to get vertex position from line "{}", file "{}"!)", line, path);
				return false;
			}

			vertexPositions.emplace_back(x, y, z);
		}
		else if (dataType == "vt")
		{ // Vertex Texture Coordinate
			float u, v;
			if (!(segments >> u >> v))
			{
				ErrMsgF(R"(Failed to get texture coordinate from line "{}", file "{}"!)", line, path);
				return false;
			}

			vertexTexCoords.emplace_back(u, v);
		}
		else if (dataType == "vn")
		{ // Vertex Normal
			float x, y, z;
			if (!(segments >> x >> y >> z))
			{
				ErrMsgF(R"(Failed to get normal from line "{}", file "{}"!)", line, path);
				return false;
			}

			vertexNormals.emplace_back(x, y, z);
		}
		else if (dataType == "f")
		{ // Index Group
			if (!begunReadingSubMesh)
			{
				ErrMsgF(R"(Reached index group before creating submesh, file "{}"!)", path);
				return false;
			}

			std::vector<RawIndex> indicesInGroup;
			int vi, ti, ni;

			while (segments >> vi >> ti >> ni)
				indicesInGroup.emplace_back(--vi, --ti, --ni);

			size_t groupSize = indicesInGroup.size();
			if (groupSize < 3 || groupSize > 4)
			{
				ErrMsgF(R"(Unparseable group size '{}' at line "{}", file "{}"!)", groupSize, line, path);
				return false;
			}
			
			auto &indexBack = indexGroups.back();
			indexBack.emplace_back(indicesInGroup[0]);
			indexBack.emplace_back(indicesInGroup[1]);
			indexBack.emplace_back(indicesInGroup[2]);

			if (groupSize == 4)
			{ // Group is a quad
				auto &indexBack2 = indexGroups.back();
				indexBack2.emplace_back(indicesInGroup[0]);
				indexBack2.emplace_back(indicesInGroup[2]);
				indexBack2.emplace_back(indicesInGroup[3]);
			}
		}
		else if (dataType == "usemtl")
		{ // Start of submesh with material
			if (!begunReadingSubMesh)
			{ // First submesh
				begunReadingSubMesh = true;
				indexGroups.emplace_back();
				mtlGroups.emplace_back("");
			}

			if (!(segments >> mtlGroups.back()))
			{
				ErrMsgF("Failed to get sub-material name from line \"{}\", file \"{}\"!", line, path);
				return false;
			}
		}
#ifdef REPORT_UNRECOGNIZED
		else
		{
			ErrMsgF(R"(Unimplemented object flag '{}' on line "{}", file "{}"!)", dataType, line, path);

		}
#endif
	}

	if (vertexPositions.empty())
		vertexPositions.emplace_back(0.0f, 0.0f, 0.0f);

	if (vertexTexCoords.empty())
		vertexTexCoords.emplace_back(0.0f, 0.0f);

	if (vertexNormals.empty())
		vertexNormals.emplace_back(0.0f, 1.0f, 0.0f);

	size_t groupCount = indexGroups.size();
	for (size_t i = 0; i < groupCount; i++)
	{
		if (mtlGroups.size() <= i)
			mtlGroups.emplace_back("default");
		else if (mtlGroups[i].empty())
			mtlGroups[i] = "default";
	}

	return true;
}

static bool ReadMaterial(const char *path, MaterialGroup &material)
{
	material.mtlName = static_cast<std::string>(path).substr(
		static_cast<std::string>(path).find_last_of('\\') + 1, 
		static_cast<std::string>(path).find_last_of('.')
	);

	const std::string folderPath = static_cast<std::string>(path).substr(
		0,
		static_cast<std::string>(path).find_last_of('\\') + 1
	);

	std::ifstream fileStream(path);
	std::string line;

	while (std::getline(fileStream, line))
	{
		size_t commentStart = line.find_first_of('#');
		if (commentStart != std::string::npos)
			line = line.substr(0, commentStart); // Exclude comments

		if (line.empty())
			continue; // Skip filler line

		std::istringstream segments(line);

		std::string dataType;
		if (!(segments >> dataType))
		{
			ErrMsgF("Failed to get data type from line \"{}\", file \"{}\"!", line, path);
			return false;
		}

		if (dataType == "newmtl")
		{ // New Sub-material
			std::string mtlName;
			if (!(segments >> mtlName))
			{
				ErrMsgF("Failed to get sub-material name from line \"{}\", file \"{}\"!", line, path);
				return false;
			}

			material.subMaterials.emplace_back(mtlName, "", "", "", 0.0f);
		}
		else if (dataType == "map_Ka")
		{ 
			std::string mapPath;
			if (!(segments >> mapPath))
			{
				ErrMsgF("Failed to get map_Ka path from line \"{}\", file \"{}\"!", line, path);
				return false;
			}

			if (material.subMaterials.empty())
				material.subMaterials.emplace_back("", "", "", "", 0.0f);
			material.subMaterials.back().ambientPath = folderPath + mapPath;
		}
		else if (dataType == "map_Kd")
		{ 
			std::string mapPath;
			if (!(segments >> mapPath))
			{
				ErrMsgF("Failed to get map_Kd path from line \"{}\", file \"{}\"!", line, path);
				return false;
			}

			if (material.subMaterials.empty())
				material.subMaterials.emplace_back("", "", "", "", 0.0f);
			material.subMaterials.back().diffusePath = folderPath + mapPath;
		}
		else if (dataType == "map_Ks")
		{ 
			std::string mapPath;
			if (!(segments >> mapPath))
			{
				ErrMsgF("Failed to get map_Ks path from line \"{}\", file \"{}\"!", line, path);
				return false;
			}

			if (material.subMaterials.empty())
				material.subMaterials.emplace_back("", "", "", "", 0.0f);
			material.subMaterials.back().specularPath = folderPath + mapPath;
		}
		else if (dataType == "Ns")
		{ 
			float exponent;
			if (!(segments >> exponent))
			{
				ErrMsgF("Failed to get Ns value from line \"{}\", file \"{}\"!", line, path);
				return false;
			}

			if (material.subMaterials.empty())
				material.subMaterials.emplace_back("", "", "", "", 0.0f);

			material.subMaterials.back().specularExponent = exponent;
		}
#ifdef REPORT_UNRECOGNIZED
		else
		{
			ErrMsgF(R"(Unimplemented material flag '{}' on line "{}", file "{}"!)", dataType, line, path);
		}
#endif
	}

	if (material.subMaterials.empty())
	{
		material.subMaterials.emplace_back("default", "", "", "", 50.0f);
	}

	return true;
}


struct FormattedIndexGroup {
	std::vector<uint32_t> indices;
	std::string mtlName;
};

static void FormatRawMesh(
	std::vector<FormattedVertex> &formattedVertices,
	std::vector<FormattedIndexGroup> &formattedIndexGroups,
	const std::vector<RawPosition> &vertexPositions,
	const std::vector<RawTexCoord> &vertexTexCoords,
	const std::vector<RawNormal> &vertexNormals,
	const std::vector<std::vector<RawIndex>> &indexGroups,
	const std::vector<std::string> &mtlGroups)
{
	// Format vertices & index groups
	const size_t groupCount = indexGroups.size();
	for (size_t groupIndex = 0; groupIndex < groupCount; groupIndex++)
	{
		formattedIndexGroups.emplace_back();
		FormattedIndexGroup *formattedGroup = &formattedIndexGroups.back();
		const std::vector<RawIndex> *rawGroup = &indexGroups[groupIndex];

		if (mtlGroups.size() > groupIndex)
			formattedGroup->mtlName = mtlGroups[groupIndex];

		const size_t startingVertex = formattedVertices.size();

		const size_t groupSize = rawGroup->size();
		for (size_t vertIndex = 0; vertIndex < groupSize; vertIndex++)
		{
			const RawIndex rI = (*rawGroup)[vertIndex];
			const RawPosition rP = vertexPositions[rI.v];
			const RawTexCoord rT = vertexTexCoords[rI.t];
			const RawNormal rN = vertexNormals[rI.n];

			formattedGroup->indices.emplace_back(static_cast<uint32_t>(formattedVertices.size()));
			formattedVertices.emplace_back(
				rP.x, rP.y, rP.z,
				rN.x, rN.y, rN.z,
				0,	  0,    0,
				rT.u, rT.v
			);
		}

		// Generate tangents
		for (size_t triIndex = 0; triIndex < groupSize; triIndex += 3)
		{
			FormattedVertex *verts[3] = {
				&formattedVertices[startingVertex + triIndex + 0],
				&formattedVertices[startingVertex + triIndex + 1],
				&formattedVertices[startingVertex + triIndex + 2]
			};

			const dx::XMFLOAT3A
				v0 = { verts[0]->px, verts[0]->py, verts[0]->pz },
				v1 = { verts[1]->px, verts[1]->py, verts[1]->pz },
				v2 = { verts[2]->px, verts[2]->py, verts[2]->pz };

			const dx::XMFLOAT3A
				edge1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z },
				edge2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };

			const dx::XMFLOAT2A
				uv0 = { verts[0]->u, verts[0]->v },
				uv1 = { verts[1]->u, verts[1]->v },
				uv2 = { verts[2]->u, verts[2]->v };

			const dx::XMFLOAT2A
				deltaUV1 = { uv1.x - uv0.x,	uv1.y - uv0.y },
				deltaUV2 = { uv2.x - uv0.x,	uv2.y - uv0.y };

			const float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
			dx::XMFLOAT4A tangent = {
				f * (edge1.x * deltaUV2.y - edge2.x * deltaUV1.y),
				f * (edge1.y * deltaUV2.y - edge2.y * deltaUV1.y),
				f * (edge1.z * deltaUV2.y - edge2.z * deltaUV1.y),
				0.0f
			};

			for (size_t i = 0; i < 3; i++)
			{
				// Gram-Schmidt orthogonalization
				const dx::XMFLOAT4A normal = { verts[i]->nx, verts[i]->ny, verts[i]->nz, 0.0f };

				const dx::XMVECTOR
					n = *reinterpret_cast<const dx::XMVECTOR *>(&normal),
					t = *reinterpret_cast<const dx::XMVECTOR *>(&tangent);

				const dx::XMVECTOR newTangentVec = dx::XMVector3Normalize(
					dx::XMVectorSubtract(t,
						dx::XMVectorScale(n,
							dx::XMVectorGetX(
								dx::XMVector3Dot(n, t)))));

				const dx::XMFLOAT4A newTangent = *reinterpret_cast<const dx::XMFLOAT4A *>(&newTangentVec);

				verts[i]->tx = newTangent.x;
				verts[i]->ty = newTangent.y;
				verts[i]->tz = newTangent.z;
			}
		}
	}

	for (UINT i = 0; i < formattedIndexGroups.size(); i++)
		if (formattedIndexGroups[i].indices.empty())
			formattedIndexGroups.erase(formattedIndexGroups.begin() + i--);
}

static void SendFormattedMeshToMeshData(MeshData *meshData,
	const std::vector<FormattedVertex> &formattedVertices,
	const std::vector<FormattedIndexGroup> &formattedIndexGroups,
	const MaterialGroup &material)
{
	// Send vertex data to meshData
	meshData->vertexInfo.nrOfVerticesInBuffer = static_cast<UINT>(formattedVertices.size());
	meshData->vertexInfo.sizeOfVertex = sizeof(FormattedVertex);
	meshData->vertexInfo.vertexData = reinterpret_cast<float *>(new FormattedVertex[meshData->vertexInfo.nrOfVerticesInBuffer]);

	std::memcpy(
		meshData->vertexInfo.vertexData,
		formattedVertices.data(),
		static_cast<size_t>(meshData->vertexInfo.sizeOfVertex) * meshData->vertexInfo.nrOfVerticesInBuffer
	);

	// Send index data to meshData
	std::vector<uint32_t> inlineIndices;

	const size_t groupCount = formattedIndexGroups.size();
	for (size_t group_i = 0; group_i < groupCount; group_i++)
	{
		MeshData::SubMeshInfo subMeshInfo = { };
		const FormattedIndexGroup *currGroup = &formattedIndexGroups[group_i];

		subMeshInfo.startIndexValue = static_cast<UINT>(inlineIndices.size());
		subMeshInfo.nrOfIndicesInSubMesh = static_cast<UINT>(currGroup->indices.size());

		SubMaterial const *currMat = nullptr;
		for (const SubMaterial &subMat : material.subMaterials)
			if (subMat.mtlName == currGroup->mtlName)
			{
				currMat = &subMat;
				break;
			}

		subMeshInfo.ambientTexturePath	= (currMat != nullptr) ? currMat->ambientPath		: "";
		subMeshInfo.diffuseTexturePath	= (currMat != nullptr) ? currMat->diffusePath		: "";
		subMeshInfo.specularTexturePath = (currMat != nullptr) ? currMat->specularPath		: "";
		subMeshInfo.specularExponent	= (currMat != nullptr) ? currMat->specularExponent	: 0.0f;

		inlineIndices.insert(inlineIndices.end(), currGroup->indices.begin(), currGroup->indices.end());
		meshData->subMeshInfo.emplace_back(subMeshInfo);
	}

	meshData->indexInfo.nrOfIndicesInBuffer = static_cast<UINT>(inlineIndices.size());
	meshData->indexInfo.indexData = new uint32_t[meshData->indexInfo.nrOfIndicesInBuffer];

	std::memcpy(
		meshData->indexInfo.indexData,
		inlineIndices.data(),
		sizeof(uint32_t) * meshData->indexInfo.nrOfIndicesInBuffer
	);

	// Calculate bounds
	dx::XMFLOAT4A
		min = {  FLT_MAX,  FLT_MAX,  FLT_MAX, 0 },
		max = { -FLT_MAX, -FLT_MAX, -FLT_MAX, 0 };

	for (const FormattedVertex &vData : formattedVertices)
	{
		if (vData.px < min.x)		min.x = vData.px;
		else if (vData.px > max.x)	max.x = vData.px;

		if (vData.py < min.y)		min.y = vData.py;
		else if (vData.py > max.y)	max.y = vData.py;

		if (vData.pz < min.z)		min.z = vData.pz;
		else if (vData.pz > max.z)	max.z = vData.pz;
	}

	float midX = (min.x + max.x) / 2.0f;
	float midY = (min.y + max.y) / 2.0f;
	float midZ = (min.z + max.z) / 2.0f;

	if (min.x >= max.x - 0.001f)
	{
		min.x = midX - 0.001f;
		max.x = midX + 0.001f;
	}
	if (min.y >= max.y - 0.001f)
	{
		min.y = midY - 0.001f;
		max.y = midY + 0.001f;
	}
	if (min.z >= max.z - 0.001f)
	{
		min.z = midZ - 0.001f;
		max.z = midZ + 0.001f;
	}

	dx::BoundingBox box;
	dx::BoundingBox().CreateFromPoints(
		box,
		*reinterpret_cast<dx::XMVECTOR *>(&min),
		*reinterpret_cast<dx::XMVECTOR *>(&max)
	);

	dx::BoundingOrientedBox minMaxBox;
	dx::BoundingOrientedBox().CreateFromBoundingBox(minMaxBox, box);

	/*dx::BoundingOrientedBox().CreateFromPoints( // HACK
		minMaxBox,
		meshData->vertexInfo.nrOfVerticesInBuffer,
		reinterpret_cast<const dx::XMFLOAT3 *>(meshData->vertexInfo.vertexData),
		sizeof(FormattedVertex)
	);

	// TODO: Modify so that the box is roughly axis-aligned.
	// CreateFromPoints can result in a box with its forward axis pointing upwards, or even backwards.
	// We want each local axis to at least point more in its world axis than any other axes.
	*/

	meshData->boundingBox = minMaxBox;
}


bool LoadMeshFromFile(const char *path, MeshData *meshData)
{
	if (meshData->vertexInfo.vertexData != nullptr || 
		meshData->indexInfo.indexData != nullptr)
	{
		ErrMsg("meshData is not nullified!");
		return false;
	}

	std::string ext = path;
	ext.erase(0, ext.find_last_of('.') + 1);

	std::vector<RawPosition> vertexPositions;
	std::vector<RawTexCoord> vertexTexCoords;
	std::vector<RawNormal> vertexNormals;
	std::vector<std::vector<RawIndex>> indexGroups;
	std::vector<std::string> mtlGroups;

	if (ext == "obj")
	{
		if (!ReadWavefront(path, vertexPositions, vertexTexCoords, vertexNormals, indexGroups, mtlGroups, meshData->mtlFile))
		{
			ErrMsg("Failed to read wavefront file!");
			return false;
		}

		const std::string materialPath = path;
		meshData->mtlFile = materialPath.substr(0, materialPath.find_last_of('\\') + 1) + meshData->mtlFile;
	}
	else
	{
		ErrMsgF("Unimplemented mesh file extension '{}'!", ext);
		return false;
	}

	MaterialGroup material;
	if (!ReadMaterial(meshData->mtlFile.c_str(), material))
	{
		ErrMsg("Failed to read material file!");
		return false;
	}

	std::vector<FormattedVertex> formattedVertices;
	std::vector<FormattedIndexGroup> formattedIndexGroups;

	FormatRawMesh(formattedVertices, formattedIndexGroups, vertexPositions, vertexTexCoords, vertexNormals, indexGroups, mtlGroups);

	SendFormattedMeshToMeshData(meshData, formattedVertices, formattedIndexGroups, material);

	return true;	
}

// Debug Function
bool WriteMeshToFile(const char *path, const MeshData *meshData)
{
	if (meshData->vertexInfo.vertexData == nullptr || 
		meshData->indexInfo.indexData == nullptr)
	{
		ErrMsg("meshData is nullified!");
		return false;
	}

	std::ofstream fileStream(path);

	fileStream << "Loaded Mesh:\n";
	fileStream << "material = " << meshData->mtlFile << "\n\n";

	fileStream << "---------------- Vertex Data ----------------" << '\n';
	fileStream << "count = " << meshData->vertexInfo.nrOfVerticesInBuffer << '\n';
	fileStream << "size = " << meshData->vertexInfo.sizeOfVertex << "\n\n";

	for (size_t i = 0; i < meshData->vertexInfo.nrOfVerticesInBuffer; i++)
	{
		const FormattedVertex *vData = &reinterpret_cast<FormattedVertex*>(meshData->vertexInfo.vertexData)[i];

		fileStream << "Vertex " << i << '\n';

		fileStream << "\tPosition(" << vData->px << ", " << vData->py << ", " << vData->pz << ")\n";
		fileStream << "\tNormal(" << vData->nx << ", " << vData->ny << ", " << vData->nz << ")\n";
		fileStream << "\tTangent(" << vData->tx << ", " << vData->ty << ", " << vData->tz << ")\n";
		fileStream << "\tTexCoord(" << vData->u << ", " << vData->v << ")\n";

		fileStream << '\n';
	}
	fileStream << "---------------------------------------------\n\n";

	fileStream << "---------------- Index Data ----------------\n";
	fileStream << "count = " << meshData->indexInfo.nrOfIndicesInBuffer << '\n';

	for (size_t i = 0; i < meshData->indexInfo.nrOfIndicesInBuffer / 3; i++)
	{
		const uint32_t *iData = &meshData->indexInfo.indexData[i*3];

		fileStream << "indices " << i*3 << "-" << i*3+2 << "\t (" << iData[0] << "/" << iData[1] << "/" << iData[2] << ")\n";
	}
	fileStream << "--------------------------------------------\n\n";

	fileStream << "---------------- Triangle Data ----------------\n";
	fileStream << "count = " << meshData->indexInfo.nrOfIndicesInBuffer / 3 << '\n';

	for (size_t i = 0; i < meshData->indexInfo.nrOfIndicesInBuffer / 3; i++)
	{
		const uint32_t *iData = &meshData->indexInfo.indexData[i*3];

		fileStream << "Triangle " << i+1 << " {";

		for (size_t j = 0; j < 3; j++)
		{
			const FormattedVertex *vData = &reinterpret_cast<FormattedVertex *>(meshData->vertexInfo.vertexData)[iData[j]];
			fileStream << "\n\tv" << j << '\n';

			fileStream << "\t\tP Vector3(" << vData->px << ", " << vData->py << ", " << vData->pz << ")\n";
			fileStream << "\t\tN Vector3(" << vData->nx << ", " << vData->ny << ", " << vData->nz << ")\n";
			fileStream << "\t\tT Vector3(" << vData->tx << ", " << vData->ty << ", " << vData->tz << ")\n";
			fileStream << "\t\tu Vector3(" << vData->u << ", " << vData->v << ", 0)\n";
		}

		fileStream << "}\n\n";
	}
	fileStream << "--------------------------------------------\n\n\n";

	fileStream << "---------------- Submesh Data ----------------\n";
	for (size_t i = 0; i < meshData->subMeshInfo.size(); i++)
	{
		auto &submeshInfo = meshData->subMeshInfo[i];
		fileStream << "Submesh " << i << '\n';
		fileStream << "\tstart index = " << submeshInfo.startIndexValue << '\n';
		fileStream << "\tlength = " << submeshInfo.nrOfIndicesInSubMesh << '\n';
		fileStream << "\tambient = " << submeshInfo.ambientTexturePath << '\n';
		fileStream << "\tdiffuse = " << submeshInfo.diffuseTexturePath << '\n';
		fileStream << "\tspecular = " << submeshInfo.specularTexturePath << '\n';
		fileStream << "\texponent = " << submeshInfo.specularExponent << '\n';
		fileStream << '\n';
	}
	fileStream << "----------------------------------------------\n\n";
	
	fileStream.close();
	return true;
}
#pragma endregion


#pragma region Texture
bool LoadDDSTextureFromFile(
	ID3D11Device *device, ID3D11DeviceContext *context, 
	const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
	dx::DDS_ALPHA_MODE *alphaMode, bool mipmapped)
{
	std::wstring wPath = StringUtils::NarrowToWide(path);

	if (FAILED(dx::CreateDDSTextureFromFile(device, context, wPath.c_str(), (ID3D11Resource **)&texture, &srv, 0, alphaMode)))
	{
		WarnF("Failed to load DDS texture from file at path \"{}\"!", path);
		return false;
	}

	if (mipmapped && texture != nullptr)
	{
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);

		if (desc.MipLevels == 1)
		{
			context->GenerateMips(srv);
		}
	}

	return true;
}

bool LoadTextureFromFile(
	ID3D11Device *device, ID3D11DeviceContext *context, 
	const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
	DXGI_FORMAT format, bool mipmapped, UINT downsample, bool flip)
{
	std::wstring wPath = StringUtils::NarrowToWide(path);

	std::string ext = path;
	ext.erase(0, ext.find_last_of('.') + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	HRESULT hr{};

	dx::TexMetadata metadata;
	dx::ScratchImage image;

	if		(ext == "dds")
	{
		hr = dx::LoadFromDDSFile(wPath.c_str(), dx::DDS_FLAGS_NONE, &metadata, image);
		if (FAILED(hr))
		{
			ErrMsgF("Failed to load image from DDS file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
			return false;
		}

		flip = false; // Assume DDS files are pre-flipped
	}
	else if (ext == "tga")
	{
		hr = dx::LoadFromTGAFile(wPath.c_str(), &metadata, image);
		if (FAILED(hr))
		{
			ErrMsgF("Failed to load image from TGA file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
			return false;
		}
	}
	else if (ext == "hdr")
	{
		hr = dx::LoadFromHDRFile(wPath.c_str(), &metadata, image);
		if (FAILED(hr))
		{
			ErrMsgF("Failed to load image from HDR file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
			return false;
		}
	}
	else
	{
		hr = dx::LoadFromWICFile(wPath.c_str(), dx::WIC_FLAGS_NONE, &metadata, image);
		if (FAILED(hr))
		{
			ErrMsgF("Failed to load image from WIC file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
			return false;
		}
	}

	if (format == DXGI_FORMAT_UNKNOWN)
		format = metadata.format;

	bool generateMips = mipmapped && (metadata.mipLevels <= 1);

	bool isBCFormat = D3D11FormatData::IsBCFormat(metadata.format);
	bool toBCFormat = D3D11FormatData::IsBCFormat(format);

	if (isBCFormat && (flip || generateMips || downsample > 0))
	{
		dx::ScratchImage decompressed;

		// Decompress BC format
		hr = dx::Decompress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DXGI_FORMAT_UNKNOWN, decompressed);
		if (FAILED(hr))
		{
			WarnF("Failed to decompress BC image from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
			return false;
		}

		image.Release();
		image = std::move(decompressed);
		metadata = image.GetMetadata();

		isBCFormat = false;
	}

	if (!isBCFormat && downsample > 0)
	{
		int scaleDiv = 1 << downsample;
		dx::ScratchImage downsampled;

		hr = dx::Resize(image.GetImages(), image.GetImageCount(), image.GetMetadata(), metadata.width / scaleDiv, metadata.height / scaleDiv, dx::TEX_FILTER_DEFAULT, downsampled);
		if (FAILED(hr))
		{
			WarnF("Failed to downsample image from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
			return false;
		}

		image.Release();
		image = std::move(downsampled);
		metadata = image.GetMetadata();

		// Resizing removes mips
		generateMips = mipmapped && (metadata.mipLevels <= 1);
	}
	
	if (flip)
	{
		if (isBCFormat)
		{
			// Cannot flip BC formatted image. Make sure to flip pre-compressed images externally.
			flip = false;
		}
		else
		{
			dx::ScratchImage flipped;

			// Flip
			hr = dx::FlipRotate(image.GetImages(), image.GetImageCount(), image.GetMetadata(), dx::TEX_FR_FLIP_VERTICAL, flipped);
			if (FAILED(hr))
			{
				WarnF("Failed to flip image from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
				return false;
			}

			image.Release();
			image = std::move(flipped);
			metadata = image.GetMetadata();
		}
	}

	if (!isBCFormat && generateMips)
	{
		if (metadata.width > 1 && metadata.height > 1)
		{
			size_t mipsLevels = 0;
			if (toBCFormat)
			{
				// Ensure smallest dimension is divisible by 4
				mipsLevels = 1;
					
				int width = static_cast<int>(metadata.width);
				int height = static_cast<int>(metadata.height);

				while (width % 4 == 0 && height % 4 == 0 && width > 4 && height > 4)
				{
					width /= 2;
					height /= 2;
					mipsLevels++;
				}
			}

			if (mipsLevels != 1)
			{
				// Generate MipMaps
				dx::ScratchImage mipmap;
				hr = dx::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), dx::TEX_FILTER_DEFAULT, mipsLevels, mipmap);
				if (FAILED(hr))
				{
					WarnF("Failed to generate mipmaps for image from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
					return false;
				}

				image.Release();
				image = std::move(mipmap);
				metadata = image.GetMetadata();
			}
			else
			{
				generateMips = false;
				mipmapped = false;
			}
		}
		else
		{
			mipmapped = false;
		}
	}

	if (metadata.format != format)
	{
		if (toBCFormat)
		{
			// Convert to BC format

			bool bc6hbc7 = false;
			switch (format)
			{
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				bc6hbc7 = true;
				break;

			default:
				break;
			}

			dx::TEX_COMPRESS_FLAGS cflags = dx::TEX_COMPRESS_DEFAULT;

			if (dx::IsSRGB(metadata.format))
				cflags |= dx::TEX_COMPRESS_SRGB_IN;
			if (dx::IsSRGB(format))
				cflags |= dx::TEX_COMPRESS_SRGB_OUT;

			dx::ScratchImage compressed;
			if (bc6hbc7)
			{
				cflags |= dx::TEX_COMPRESS_BC7_USE_3SUBSETS;
				hr = dx::Compress(device, image.GetImages(), image.GetImageCount(), metadata, format, cflags, 0.1f, compressed);
			}
			else
			{
#ifdef _OPENMP
				cflags |= dx::TEX_COMPRESS_PARALLEL;
#endif
				hr = dx::Compress(image.GetImages(), image.GetImageCount(), metadata, format, cflags, 0.5f, compressed);
			}

			if (FAILED(hr))
			{
				WarnF("Failed to compress image to BC format from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
				return false;
			}

			image.Release();
			image = std::move(compressed);
			metadata = image.GetMetadata();
		}
		else
		{
			/*dx::ScratchImage converted;

			// Convert to non-BC format
			hr = dx::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), format, dx::TEX_FILTER_DEFAULT, 0.5f, converted);
			if (FAILED(hr))
			{
				WarnF("Failed to convert image to non-BC format from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
				return false;
			}

			image.Release();
			image = std::move(converted);
			metadata = image.GetMetadata();*/
		}
	}

#if !defined(_DEPLOY) && defined(BAKE_TEXTURES)
	// Save processed texture to TEXTURE_BAKE_PATH
	{
		std::string assetPath = path;

		size_t assetPathStart = assetPath.find(ASSET_PATH_TEXTURES);
		if (assetPathStart == std::string::npos)
		{
			size_t lastSlash = assetPath.find_last_of("\\/");
			assetPath = assetPath.substr(lastSlash + 1);
		}
		else
		{
			size_t toCut = strlen(ASSET_PATH_TEXTURES) + 1; // +1 to also remove slash
			assetPath = assetPath.substr(assetPathStart + toCut);
		}

		size_t lastDot = assetPath.find_last_of('.');
		assetPath = assetPath.substr(0, lastDot);

		std::string bakePath = PATH_FILE_EXT(ASSET_COMPILED_PATH_TEXTURES, assetPath, "dds");
		std::wstring wBakePath = StringUtils::NarrowToWide(bakePath);

		// Ensure directory exists
		{
			std::string dirPath = bakePath;

			size_t lastSlash = dirPath.find_last_of("\\/");
			if (lastSlash != std::string::npos)
			{
				dirPath = dirPath.substr(0, lastSlash);
			}
			else
			{
				dirPath = ".";
			}

			CreateDirectoryA(dirPath.c_str(), NULL);
		}

		hr = dx::SaveToDDSFile(image.GetImages(), image.GetImageCount(), metadata, dx::DDS_FLAGS_NONE, wBakePath.c_str());
		if (FAILED(hr))
		{
			WarnF("Failed to save baked texture to file at path \"{}\"! hr: {}, {}", bakePath, hr, StringUtils::HResultToString(hr));
		}
	}
#endif

	// Create D3D11 texture
	hr = dx::CreateTexture(device, image.GetImages(), image.GetImageCount(), metadata, (ID3D11Resource**)&texture);
	if (FAILED(hr))
	{
		WarnF("Failed to create D3D11 texture from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
		return false;
	}
	image.Release();

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = metadata.mipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
	if (FAILED(hr)) 
	{
		WarnF("Failed to create shader resource view from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
		return false;
	}

	if (mipmapped)
	{
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);

		if (desc.MipLevels == 1)
		{
			context->GenerateMips(srv);
		}
	}

	return true;
}

bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned char> &data)
{
	stbi_set_flip_vertically_on_load(1);

	int w, h, comp;
	unsigned char *imgData = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
	if (imgData == nullptr)
	{
		Warn(std::format("Failed to load texture from file at path \"{}\"!", path));
		return false;
	}

	width = static_cast<UINT>(w);
	height = static_cast<UINT>(h);
	data = std::vector(imgData, imgData + static_cast<size_t>(4ull * w * h));

	stbi_image_free(imgData);
	return true;
}
bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned short> &data)
{
	stbi_set_flip_vertically_on_load(1);

	int w, h, comp;
	unsigned short *imgData = stbi_load_16(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
	if (imgData == nullptr)
	{
		ErrMsgF("Failed to load texture from file at path \"{}\"!", path);
		return false;
	}

	width = static_cast<UINT>(w);
	height = static_cast<UINT>(h);
	data = std::vector(imgData, imgData + static_cast<size_t>(4ul * w * h));

	stbi_image_free(imgData);
	return true;
}
bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<float> &data, int nChannels, bool highPrecision)
{
	if (nChannels < 1 || nChannels > 4)
	{
		ErrMsgF("Trying to read incorrect number of channels: {}!", nChannels);
		return false;
	}

	stbi_set_flip_vertically_on_load(1);

	int w, h, comp;

	if (highPrecision)
	{
		unsigned short *imgData = stbi_load_16(path.c_str(), &w, &h, &comp, nChannels);

		if (imgData == nullptr)
		{
			ErrMsgF("Failed to load texture from file at path \"{}\"!", path);
			return false;
		}

		width = static_cast<UINT>(w);
		height = static_cast<UINT>(h);

		std::vector<unsigned short> temp = std::vector(imgData, imgData + static_cast<size_t>(nChannels * w * h));
		data.resize(temp.size());
		for (int i = 0; i < temp.size(); i++)
			data[i] = (float)temp[i] / 65535.0f;

		stbi_image_free(imgData);
	}
	else
	{
		unsigned char *imgData = stbi_load(path.c_str(), &w, &h, &comp, nChannels);

		if (imgData == nullptr)
		{
			ErrMsgF("Failed to load texture from file at path \"{}\"!", path);
			return false;
		}

		width = static_cast<UINT>(w);
		height = static_cast<UINT>(h);

		std::vector<unsigned char> temp = std::vector(imgData, imgData + static_cast<size_t>(nChannels * w * h));
		data.resize(temp.size());
		for (int i = 0; i < temp.size(); i++)
			data[i] = (float)temp[i] / 255.0f;

		stbi_image_free(imgData);
	}
	return true;
}
bool LoadTextureFromFile(const std::string &path, std::vector<unsigned char> &data, UINT &width, UINT &height, UINT &channels, UINT &bitsPerChannel)
{
	stbi_set_flip_vertically_on_load(1);

	FILE *f = stbi__fopen(path.c_str(), "rb");
	if (!f)
	{
		ErrMsgF("Failed to open texture file at path '{}'!", path);
		return false;
	}

	const int bits = stbi_is_16_bit_from_file(f) == 0 ? 8 : 16;
	int w, h, n;
	uint8_t *imgData = nullptr;

	if (bits == 16)	imgData = (uint8_t*)stbi_load_from_file_16(f, &w, &h, &n, 0);
	else			imgData = stbi_load_from_file(f, &w, &h, &n, 0);
	fclose(f);

	if (!imgData)
	{
		ErrMsgF("Failed to load texture from file at path '{}'!", path);
		return false;
	}

	const size_t imgSizeInBytes = static_cast<size_t>(w) * h * n * bits / 8;
	data.resize(imgSizeInBytes);
	std::memcpy(data.data(), imgData, imgSizeInBytes);

	width = static_cast<UINT>(w);
	height = static_cast<UINT>(h);
	channels = static_cast<UINT>(n);
	bitsPerChannel = static_cast<UINT>(bits);

	stbi_image_free(imgData);
	return true;
}

bool DownsampleTexture(std::vector<uint8_t> &data, UINT inWidth, UINT inHeight, UINT outWidth, UINT outHeight)
{
	ZoneScopedXC(RandomUniqueColor());

	if (inWidth < outWidth)
	{
		ErrMsg("Input width is smaller than output width!");
		return false;
	}

	if (inHeight < outHeight)
	{
		ErrMsg("Input height is smaller than output height!");
		return false;
	}

	if (data.size() % ((size_t)inWidth * inHeight) != 0)
	{
		ErrMsg("Input data size is not a multiple of input width and height!");
		return false;
	}

	UINT stride = data.size() / ((size_t)inWidth * inHeight);
	UINT samplesY = inHeight / outHeight;
	UINT samplesX = inWidth / outWidth;
	size_t newSize = (size_t)outWidth * outHeight * stride;

#ifdef USE_OWN_RESIZE_ALGORITHM
	// Use higher precision so we can average pixel values without causing an integer overflow
	std::vector<uint32_t> newPixel(stride);

	std::fill(newPixel.begin(), newPixel.end(), 0);

	for (UINT outY = 0; outY < outHeight; outY++)
	{
		UINT inY = outY * inHeight / outHeight;

		for (UINT outX = 0; outX < outWidth; outX++)
		{
			UINT inX = outX * inWidth / outWidth;

			size_t outPixelIndex = ((size_t)outY * outWidth + outX) * stride;
			size_t inPixelIndex = ((size_t)inY * inWidth + inX) * stride;

			UINT samplesTaken = 0;
			for (UINT dY = 0; dY < samplesY; dY++)
			{
				// Skip if out of bounds
				if (inY + dY >= inHeight)
					break;

				for (UINT dX = 0; dX < samplesX; dX++)
				{
					// Skip if out of bounds
					if (inX + dX >= inWidth)
						break;

					size_t sampleIndex = inPixelIndex + ((size_t)dX + ((size_t)dY * (size_t)inWidth)) * (size_t)stride;

					for (UINT c = 0; c < stride; c++)
					{
						newPixel[c] += (uint32_t)data[sampleIndex + c];
					}

					samplesTaken++;
				}
			}

			for (UINT c = 0; c < stride; c++)
			{
				uint32_t downsample = newPixel[c] / samplesTaken;
				newPixel[c] = 0;

				data[outPixelIndex + c] = (uint8_t)downsample;
			}
		}
	}

	// Resize the vector to the new size
	data.resize(newSize);
#else
	uint8_t *output = new uint8_t[newSize];

	uint8_t *ret = stbir_resize_uint8_linear(
		data.data(), inWidth, inHeight,
		0, // input stride
		output, outWidth, outHeight,
		0, // output stride
		(stbir_pixel_layout)stride // Works for 1-4 channels, assuming RGBA layout
	);

	if (ret == nullptr)
	{
		ErrMsg("Failed to downsample texture using stb_image_resize!");
		delete[] output;
		return false;
	}

	// Resize the vector to the new size
	data.resize(newSize);

	// Copy the output data to the original vector
	std::memcpy(data.data(), output, newSize);

	delete[] output; // Free the output buffer allocated by stb_image_resize
#endif

	return true;
}

#pragma endregion

#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STB_IMAGE_RESIZE_IMPLEMENTATION
#undef STB_RECT_PACK_IMPLEMENTATION
#undef STB_VORBIS_IMPLEMENTATION