#include "stdafx.h"
#include "ContentLoader.h"
#include "Engine/Utils/StringUtils.h"
#include "Engine/Content/DefaultVertex.h"
#include "ContentManager/AssetLoading/MeshLoader.h"

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
#include "Dependencies/stb/stb_image.h"
#include "Dependencies/stb/stb_image_write.h"
#include "Dependencies/stb/stb_image_resize2.h"
#include "Dependencies/stb/stb_rect_pack.h"
#include "Dependencies/stb/stb_vorbis.h"
#pragma warning(default: 26819)
#pragma warning(default: 6262)

using namespace ContentData;
using namespace WellEngine;


#pragma region Mesh
static void CalcMeshBounds(MeshData &meshData)
{
	// Calculate bounds
	dx::XMFLOAT4A
		min = { FLT_MAX,  FLT_MAX,  FLT_MAX, 0 },
		max = { -FLT_MAX, -FLT_MAX, -FLT_MAX, 0 };

	UINT vertexCount = meshData.vertexInfo.nrOfVerticesInBuffer;

	for (UINT i = 0; i < vertexCount; i++)
	{
		const FormattedVertex &vData = reinterpret_cast<FormattedVertex *>(meshData.vertexInfo.vertexData)[i];

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
		meshData.vertexInfo.nrOfVerticesInBuffer,
		reinterpret_cast<const dx::XMFLOAT3 *>(meshData.vertexInfo.vertexData),
		sizeof(FormattedVertex)
	);

	// TODO: Modify so that the box is roughly axis-aligned.
	// CreateFromPoints can result in a box with its forward axis pointing upwards, or even backwards.
	// We want each local axis to at least point more in its world axis than any other axes.
	*/

	meshData.boundingBox = minMaxBox;
}

bool WellEngine::LoadMeshFromFile(const char *path, MeshData *meshData)
{
	using namespace ContentManager;
	using namespace ContentManager::AssetLoader;

	AssetData::Mesh mesh = LoadMesh(path);

	const size_t vertCount = mesh.vertices.size();
	const size_t indexCount = mesh.indices.size();

	// Insert mesh data into meshData
	meshData->vertexInfo.nrOfVerticesInBuffer = static_cast<UINT>(vertCount);
	meshData->vertexInfo.sizeOfVertex = sizeof(FormattedVertex);
	meshData->vertexInfo.vertexData = reinterpret_cast<float *>(new FormattedVertex[vertCount]);

	for (size_t i = 0; i < vertCount; i++)
	{
		const AssetData::Vertex &vIn = mesh.vertices[i];
		FormattedVertex *vOut = reinterpret_cast<FormattedVertex *>(meshData->vertexInfo.vertexData) + i;

		vOut->px = vIn.px;
		vOut->py = vIn.py;
		vOut->pz = vIn.pz;

		vOut->nx = vIn.nx;
		vOut->ny = vIn.ny;
		vOut->nz = vIn.nz;

		vOut->tx = vIn.tx;
		vOut->ty = vIn.ty;
		vOut->tz = vIn.tz;

		vOut->u = vIn.u0;
		vOut->v = vIn.v0;
	}

	meshData->indexInfo.nrOfIndicesInBuffer = static_cast<UINT>(indexCount);
	meshData->indexInfo.indexData = new UINT[indexCount];

	std::memcpy(
		meshData->indexInfo.indexData,
		mesh.indices.data(),
		sizeof(UINT) * indexCount
	);

	// Generate submesh info
	meshData->subMeshInfo.resize(mesh.subMeshes.size());
	for (size_t i = 0; i < mesh.subMeshes.size(); i++)
	{
		const AssetData::SubMesh &subMesh = mesh.subMeshes[i];
		MeshData::SubMeshInfo &subMeshInfo = meshData->subMeshInfo[i];

		subMeshInfo.startIndexValue =subMesh.startIndex;
		subMeshInfo.nrOfIndicesInSubMesh = subMesh.indexCount;
	}

	CalcMeshBounds(*meshData);

	return true;
}
#pragma endregion


#pragma region Texture
std::string WellEngine::GetTextureBakePath(const std::string &file)
{
	std::string assetPath = file;

	size_t assetPathStart = assetPath.find(WE_D_ASSET_TEXTURE);
	if (assetPathStart == std::string::npos)
	{
		size_t lastSlash = assetPath.find_last_of("\\/");
		assetPath = assetPath.substr(lastSlash + 1);
	}
	else
	{
		size_t toCut = strlen(WE_D_ASSET_TEXTURE) + 1; // +1 to also remove slash
		assetPath = assetPath.substr(assetPathStart + toCut);
	}

	size_t lastDot = assetPath.find_last_of('.');
	assetPath = assetPath.substr(0, lastDot);

	return WE_DFE(WE_D_COMPILED_TEXTURE, assetPath, "dds");
}

bool WellEngine::LoadDDSTextureFromFile(
	ID3D11Device *device, ID3D11DeviceContext *context, 
	const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
	TexLoadInfo *info)
{
	TexLoadInfo defaultInfo;
	if (info == nullptr)
		info = &defaultInfo;

	std::wstring wPath = StringUtils::NarrowToWide(path);

	if (FAILED(dx::CreateDDSTextureFromFile(device, context, wPath.c_str(), (ID3D11Resource **)&texture, &srv, 0, &info->alphaMode)))
	{
		WarnF("Failed to load DDS texture from file at path \"{}\"!", path);
		return false;
	}

	if (info->mipmapped && texture != nullptr)
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

bool WellEngine::LoadTextureFromFile(ID3D11Device *device, ID3D11DeviceContext *context, 
	const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
	TexLoadInfo *info, bool bake)
{
	TexLoadInfo defaultInfo;
	if (info == nullptr)
		info = &defaultInfo;

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

		info->flipX = false; // Assume DDS files are pre-flipped
		info->flipY = false;
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

	if (info->format == DXGI_FORMAT_UNKNOWN)
		info->format = metadata.format;

	bool generateMips = info->mipmapped && (metadata.mipLevels <= 1);

	bool isBCFormat = D3D11FormatData::IsBCFormat(metadata.format);
	bool toBCFormat = D3D11FormatData::IsBCFormat(info->format);

	if (isBCFormat && (info->flipX || info->flipY || generateMips || info->downsample > 0))
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

	if (!isBCFormat && info->downsample > 0)
	{
		// Clamp downsample to max possible value, given the image dimensions
		info->downsample = MIN(info->downsample, static_cast<UINT>(MAX(0, static_cast<int>(std::log2(MIN(metadata.width, metadata.height))) - 2)));

		int scaleDiv = 1 << info->downsample;
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
		generateMips = info->mipmapped && (metadata.mipLevels <= 1);
	}
	
	if (info->flipX || info->flipY)
	{
		if (isBCFormat)
		{
			// Cannot flip BC formatted image. Make sure to flip pre-compressed images externally.
			info->flipX = false;
			info->flipY = false;
		}
		else
		{
			dx::ScratchImage flipped;

			dx::TEX_FR_FLAGS flipFlags = dx::TEX_FR_ROTATE0;

			if (info->flipX)
				flipFlags |= dx::TEX_FR_FLIP_HORIZONTAL;
			if (info->flipY)
				flipFlags |= dx::TEX_FR_FLIP_VERTICAL;

			// Flip
			hr = dx::FlipRotate(image.GetImages(), image.GetImageCount(), image.GetMetadata(), flipFlags, flipped);
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
				info->mipmapped = false;
			}
		}
		else
		{
			info->mipmapped = false;
		}
	}

	if (metadata.format != info->format)
	{
		if (toBCFormat)
		{
			// Convert to BC format

			bool bc6hbc7 = false;
			switch (info->format)
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
			if (dx::IsSRGB(info->format))
				cflags |= dx::TEX_COMPRESS_SRGB_OUT;

			dx::ScratchImage compressed;
			if (bc6hbc7)
			{
				cflags |= dx::TEX_COMPRESS_BC7_USE_3SUBSETS;
				hr = dx::Compress(device, image.GetImages(), image.GetImageCount(), metadata, info->format, cflags, 0.1f, compressed);
			}
			else
			{
#ifdef _OPENMP
				cflags |= dx::TEX_COMPRESS_PARALLEL;
#endif
				hr = dx::Compress(image.GetImages(), image.GetImageCount(), metadata, info->format, cflags, 0.5f, compressed);
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

#ifndef _DEPLOY
	// Save processed texture to TEXTURE_BAKE_PATH
#ifndef FORCE_BAKE_TEXTURES
	if (bake)
#endif
	{
		std::string bakePath = GetTextureBakePath(path);
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
	hr = dx::CreateTexture(device, image.GetImages(), image.GetImageCount(), metadata, (ID3D11Resource **)&texture);
	if (FAILED(hr))
	{
		WarnF("Failed to create D3D11 texture from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
		return false;
	}
	image.Release();

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;

	switch (metadata.dimension)
	{
	case dx::TEX_DIMENSION_TEXTURE1D:
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		srvDesc.Texture1D.MipLevels = metadata.mipLevels;
		srvDesc.Texture1D.MostDetailedMip = 0;
		break;

	case dx::TEX_DIMENSION_TEXTURE2D:
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = metadata.mipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		break;

	case dx::TEX_DIMENSION_TEXTURE3D:
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = metadata.mipLevels;
		srvDesc.Texture3D.MostDetailedMip = 0;
		break;

	default:
		break;
	}

	hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
	if (FAILED(hr)) 
	{
		WarnF("Failed to create shader resource view from file at path \"{}\"! hr: {}, {}", path, hr, StringUtils::HResultToString(hr));
		return false;
	}

	if (info->mipmapped)
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

bool WellEngine::LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned char> &data)
{
	stbi_set_flip_vertically_on_load(1);

	int w, h, comp;
	unsigned char *imgData = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);
	if (imgData == nullptr)
	{
		WarnF("Failed to load texture from file at path \"{}\"!", path);
		return false;
	}

	width = static_cast<UINT>(w);
	height = static_cast<UINT>(h);
	data = std::vector(imgData, imgData + static_cast<size_t>(4ull * w * h));

	stbi_image_free(imgData);
	return true;
}
bool WellEngine::LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned short> &data)
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
bool WellEngine::LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<float> &data, int nChannels, bool highPrecision)
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
bool WellEngine::LoadTextureFromFile(const std::string &path, std::vector<unsigned char> &data, UINT &width, UINT &height, UINT &channels, UINT &bitsPerChannel)
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

bool WellEngine::DownsampleTexture(std::vector<uint8_t> &data, UINT inWidth, UINT inHeight, UINT outWidth, UINT outHeight)
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