#include "stdafx.h"
#include "Game.h"
//#include <dxgiformat.h>
#include "Debug/DebugData.h"
#ifdef USE_IMGUI
#include "UI/UILayout.h"
#endif

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;


Game::Game()
{
	_activeSceneIndex = -1;
}
Game::~Game()
{
#ifdef DEFERRED_CONTEXTS
	for (int i = 0; i < PARALLEL_THREADS; i++)
	{
		if (_deferredContexts[i])
			_deferredContexts[i] = nullptr;
	}
#endif

	_graphics.Shutdown();
	_content.Shutdown();
	DebugDrawer::Instance().Shutdowm();
	_scenes.clear();

	if (_workerThread.joinable())
	{
		_workerState = false;
		_mainSemaphore.release();
		_workerSemaphore.release();
		_workerThread.join();
	}

	_immediateContext.Reset();
	_window = {};

#ifdef _DEBUG
	if (DebugData::Get().reportComObjectsOnShutdown)
	{
		ID3D11Device *devicePtr = _device.Get();
		ReportLiveDeviceObjects(devicePtr);
	}
#endif
}

bool Game::CompileContent(const std::vector<std::string> &meshNames)
{
	ZoneScopedC(RandomUniqueColor());

	std::ofstream writer;
	writer.open(ASSET_COMPILED_FILE_MESHES, std::ios::binary | std::ios::ate);
	if (!writer.is_open())
	{
		ErrMsg("Failed to open compiled content file!");
		return false;
	}

	for (int i = 0; i < meshNames.size(); i++)
	{
		const std::string &meshName = meshNames[i];
		std::string meshPath = std::format("{}\\{}.obj", ASSET_PATH_MESHES, meshName);

		// Ensure the file exists
		struct stat buffer;
		if (stat(meshPath.c_str(), &buffer) != 0)
			continue;

		CompiledData data = _content.GetMeshData(meshPath.c_str());

		size_t meshNameStart = meshName.find_last_of("/\\");
		if (meshNameStart == std::string::npos)
			meshNameStart = 0;
		else
			meshNameStart += 1;

		std::string name = meshName.substr(meshNameStart);

		// Write name of file, size of contents, then contents
		writer.write((char *)(name + '\0').data(), name.size() + 1);
		writer.write((char *)&data.size, sizeof(size_t));
		writer.write(data.data, data.size);
		writer.flush();
	}

	writer.close();
	return true;
}

bool Game::DecompileContent()
{
	ZoneScopedC(RandomUniqueColor());

	std::ifstream reader(ASSET_COMPILED_FILE_MESHES, std::ios::binary | std::ios::in | std::ios::ate);
	if (!reader.is_open())
	{
		ErrMsg("Failed to open compiled content file! Try running once with COMPILE_CONTENT enabled.");
		return false;
	}

	size_t fileSize = reader.tellg();
	reader.seekg(0, std::ios::beg);

	while (reader.tellg() < fileSize)
	{
		std::string meshName = "";
		while (true)
		{
			char c = 0;
			reader.read(&c, 1);

			if (c == '\0')
				break;

			meshName += c;
		}

		size_t size = 0;
		reader.read((char *)&size, sizeof(size_t));

		std::vector<char> data;
		data.resize(size);
		reader.read(data.data(), size);

		MeshData *meshData = new MeshData();

		size_t offset = 0;
		meshData->Decompile(data, offset);

		if (_content.AddMesh(_device.Get(), std::format("{}", meshName), &meshData) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add Mesh {}!", meshName);
			reader.close();
			return false;
		}
	}

	reader.close();
	return true;
}

bool Game::LoadContent(
	const std::vector<TextureData> &textureNames,
	const std::vector<TextureData> &cubemapNames,
	const std::vector<ShaderData> &shaderNames,
	const std::vector<HeightMapData> &heightMapNames)
{
	ZoneScopedC(RandomUniqueColor());
	
	if (!DecompileContent())
	{
		ErrMsg("Failed to decompile content!");
		return false;
	}

	for (int i = 0; i < textureNames.size(); i++)
	{
		const TextureData &texture = textureNames[i];
		
		if (_content.AddTexture(
			_device.Get(), _immediateContext.Get(),
			texture.name,
			texture.file,
			texture.type,
			texture.mipmapped,
			texture.downsample) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add Tex {}!", texture.name);
		}
	}
	
#pragma warning(disable: 6993)
#pragma omp parallel num_threads(4)
	{
#pragma omp for nowait
		for (int i = 0; i < cubemapNames.size(); i++)
		{
			const TextureData &cubemap = cubemapNames[i];

			if (_content.AddCubemap(
				_device.Get(), _immediateContext.Get(),
				cubemap.name,
				cubemap.file,
				cubemap.type,
				cubemap.mipmapped,
				cubemap.downsample) == CONTENT_NULL)
			{
				ErrMsgF("Failed to add cubemap {}!", cubemap.name);
			}
		}

#pragma omp for nowait
		for (int i = 0; i < heightMapNames.size(); i++)
		{
			const HeightMapData &heightMap = heightMapNames[i];

			if (_content.AddHeightMap(
				heightMap.name, 
				PATH_FILE_EXT(
					ASSET_PATH_TEXTURES, 
					heightMap.name, "png")) == CONTENT_NULL)
			{
				ErrMsgF("Failed to add heightmap {}!", heightMap.name);
			}
		}
	}
#pragma warning(default: 6993)

	for (const ShaderData &shader : shaderNames)
	{
		if (_content.AddShader(_device.Get(), shader.name, shader.type, PATH_FILE_EXT(ASSET_COMPILED_PATH_CSO, shader.file, "cso")) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add shader {}!", shader.name);
			return false;
		}
	}
	
	// Samplers
	{
		if (_content.AddSampler(_device.Get(), "Fallback", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_ANISOTROPIC) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Fallback!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Point", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Point!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Clamp", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_ANISOTROPIC) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Clamp!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Wrap", D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FILTER_ANISOTROPIC) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Wrap!");
			return false;
		}

		D3D11_SAMPLER_DESC samplerDesc = { };
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		if (_content.AddSampler(_device.Get(), "HQ", samplerDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler HQ!");
			return false;
		}

		samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;

		if (_content.AddSampler(_device.Get(), "Shadow", samplerDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add shadow sampler!");
			return false;
		}

		samplerDesc.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;

		if (_content.AddSampler(_device.Get(), "ShadowCube", samplerDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add shadow cube sampler!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Test", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR) == CONTENT_NULL)
		{
			ErrMsg("Failed to add test sampler!");
			return false;
		}
	}
	
	// Blend States
	{
		D3D11_BLEND_DESC blendDesc = { };
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = TRUE;

		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;

		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		if (_content.AddBlendState(_device.Get(), "Fallback", blendDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add fallback blend state!");
			return false;
		}
	}

	// Input layouts
	{
		const std::vector<Semantic> fallbackInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32_FLOAT },
			{ "NORMAL",		DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TANGENT",	DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TEXCOORD",	DXGI_FORMAT_R32G32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "Fallback", fallbackInputLayout, _content.GetShaderID("VS_Geometry")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_Fallback!");
			return false;
		}

#ifdef DEBUG_BUILD
		const std::vector<Semantic> debugDrawInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32_FLOAT		},
			{ "SIZE",		DXGI_FORMAT_R32_FLOAT			},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDraw", debugDrawInputLayout, _content.GetShaderID("VS_DebugDraw")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDraw!");
			return false;
		}

		const std::vector<Semantic> debugDrawTriInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDrawTri", debugDrawTriInputLayout, _content.GetShaderID("VS_DebugDrawTri")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDrawTri!");
			return false;
		}

		const std::vector<Semantic> debugDrawSpriteInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "TEXCOORD",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "SIZE",		DXGI_FORMAT_R32G32_FLOAT		},
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDrawSprite", debugDrawSpriteInputLayout, _content.GetShaderID("VS_DebugDrawSprite")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDrawSprite!");
			return false;
		}
#endif
	}

	return true;
}

bool Game::Setup(TimeUtils &time, Window window)
{
	ZoneScopedC(RandomUniqueColor());

	_window = std::move(window);
	const bool fullscreen = false;
	const UINT width = _window.GetWidth();
	const UINT height = _window.GetHeight();

#ifdef DEFERRED_CONTEXTS
	ID3D11DeviceContext **deferredContexts[PARALLEL_THREADS]{};
	for (int i = 0; i < PARALLEL_THREADS; i++)
	{
		deferredContexts[i] = (_deferredContexts[i]).ReleaseAndGetAddressOf();
	}
#else
	ID3D11DeviceContext **intermediateDevicePtr = nullptr;
	ID3D11DeviceContext ***deferredContexts = &intermediateDevicePtr;
#endif

	if (!_graphics.Setup(fullscreen, width, height, window,
		*_device.ReleaseAndGetAddressOf(), 
		*_immediateContext.ReleaseAndGetAddressOf(), 
		*deferredContexts,
		&_content))
	{
		ErrMsg("Failed to setup d3d11!");
		return false;
	}

	std::string line;

	// Determine if a compiled content file exists or if it needs to be created
	bool compileContent = false;

#ifdef COMPILE_CONTENT
	compileContent = true;
	DbgMsg("Forcing recompilation of content files...");
#else
	FILE *compileFile = nullptr;
	errno_t compileErr =  fopen_s(&compileFile, ASSET_COMPILED_FILE_MESHES, "r");

	if (compileErr != 0 || compileFile == nullptr)
	{
		compileContent = true;
		DbgMsg("No compiled content file found. Recompiling...");
	}

	if (compileFile != nullptr)
		fclose(compileFile);
#endif

	if (compileContent)
	{ 
		std::vector<std::string> meshNames = { "Error", "Fallback", "Cube", "Plane" };

		std::ifstream fileStream(ASSET_REGISTRY_FILE_MESHES);
		if (!fileStream)
		{
			ErrMsg("Could not load file for meshes");
			return false;
		}

		while (std::getline(fileStream, line))
		{
			auto comment = line.find('#');
			if (comment != std::string::npos)
				line = line.substr(0, comment);

			if (line.empty())
				continue;

			if (line.starts_with("<DEBUG>"))
#ifdef DEBUG_BUILD
				continue;
#else
				break;
#endif

			// See if the mesh is already in the list
			auto it = std::find(meshNames.begin(), meshNames.end(), line);
			if (it != meshNames.end())
				continue;

			meshNames.emplace_back(line);
		}
		fileStream.close();

		time.TakeSnapshot("CompileContent");
		if (!CompileContent(meshNames))
		{
			ErrMsg("Failed to compile content!");
			return false;
		}
		time.TakeSnapshot("CompileContent");

		// Print content compile times.
		DbgMsgF("Content Compile: {} s", time.CompareSnapshots("CompileContent"));
	}

	std::string texPath = ASSET_PATH_TEXTURES;
#ifdef USE_BAKED_TEXTURES
	texPath = ASSET_COMPILED_PATH_TEXTURES;
#endif

	std::vector<TextureData> textureNames = {
		{ DXGI_FORMAT_UNKNOWN,			"Error",						PATH_FILE(texPath, "Error.dds"),						false,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"Fallback",						PATH_FILE(texPath, "Fallback.dds"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"White",						PATH_FILE(texPath, "White.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Black",						PATH_FILE(texPath, "Black.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Ambient",						PATH_FILE(texPath, "Ambient.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"AmbientBright",				PATH_FILE(texPath, "AmbientBright.png"),				false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Red",							PATH_FILE(texPath, "Red.png"),							false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Green",						PATH_FILE(texPath, "Green.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Blue",							PATH_FILE(texPath, "Blue.png"),							false,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Noise",						PATH_FILE(texPath, "Noise.dds"),						false,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Maxwell",						PATH_FILE(texPath, "Maxwell.dds"),						false,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Whiskers",						PATH_FILE(texPath, "Whiskers.dds"),						false,	0	},
	    { DXGI_FORMAT_R8G8B8A8_UNORM,	"Gray",							PATH_FILE(texPath, "Gray.png"),							false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"DarkGray",						PATH_FILE(texPath, "DarkGray.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Default_Normal",				PATH_FILE(texPath, "Default_Normal.png"),				true,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Metal_Normal",					PATH_FILE(texPath, "Metal_Normal.dds"),					true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"Support_Beam_Normal",			PATH_FILE(texPath, "Support_Beam_Normal.dds"),			true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"Camera_Normal",				PATH_FILE(texPath, "Camera_Normal.dds"),				true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"FlashlightBody_Normal",		PATH_FILE(texPath, "FlashlightBody_Normal.dds"),		true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"FlashlightLever_Normal",		PATH_FILE(texPath, "FlashlightLever_Normal.dds"),		true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"Cave_Wall_Normal",				PATH_FILE(texPath, "Cave_Wall_Normal.dds"),				true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Default_Specular",				PATH_FILE(texPath, "AmbientBright.png"),				true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Black_Specular",				PATH_FILE(texPath, "Black.png"),						true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"DarkGray_Specular",			PATH_FILE(texPath, "DarkGray.png"),						true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Gray_Specular",				PATH_FILE(texPath, "Gray.png"),							true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"White_Specular",				PATH_FILE(texPath, "White.png"),						true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Red_Specular",					PATH_FILE(texPath, "Red.png"),							true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Green_Specular",				PATH_FILE(texPath, "Green.png"),						true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Blue_Specular",				PATH_FILE(texPath, "Blue.png"),							true,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Metal_Specular",				PATH_FILE(texPath, "Metal_Specular.dds"),				true,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"Support_Beam_Specular",		PATH_FILE(texPath, "Support_Beam_Specular.dds"),		true,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"Camera_Specular",				PATH_FILE(texPath, "Camera_Specular.dds"),				true,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"FlashlightBody_Specular",		PATH_FILE(texPath, "FlashlightBody_Specular.dds"),		true,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"FlashlightLever_Specular",		PATH_FILE(texPath, "FlashlightLever_Specular.dds"),		true,	2	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Default_Glossiness",			PATH_FILE(texPath, "Gray.png"),							false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"White_Glossiness",				PATH_FILE(texPath, "White.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Black_Glossiness",				PATH_FILE(texPath, "Black.png"),						false,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"DarkGray_Glossiness",			PATH_FILE(texPath, "DarkGray.png"),						false,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"FlashlightBody_Glossiness",	PATH_FILE(texPath, "FlashlightBody_Glossiness.dds"),	false,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"FlashlightLever_Glossiness",	PATH_FILE(texPath, "FlashlightLever_Glossiness.dds"),	false,	1	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"Default_Occlusion",			PATH_FILE(texPath, "White.png"),						true,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Support_Beam_Occlusion",		PATH_FILE(texPath, "Support_Beam_Occlusion.dds"),		true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"Camera_Occlusion",				PATH_FILE(texPath, "Camera_Occlusion.dds"),				true,	2	},

#ifdef DEBUG_BUILD
		{ DXGI_FORMAT_UNKNOWN,			"texture1",						PATH_FILE(texPath, "texture1.dds"),						true,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"Character_Texture",			PATH_FILE(texPath, "Character_Texture.dds"),			true,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Sphere",						PATH_FILE(texPath, "Sphere.dds"),						false,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"Fade",							PATH_FILE(texPath, "Fade.dds"),							true,	0	},
		{ DXGI_FORMAT_R8G8B8A8_UNORM,	"TransformGizmo",				PATH_FILE(texPath, "TransformGizmo.png"),				false,	0	},
		{ DXGI_FORMAT_UNKNOWN,			"Cornell",						PATH_FILE(texPath, "Cornell.dds"),						false,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"SoundEmitter",					PATH_FILE(texPath, "SoundEmitter.dds"),					true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"LightSource",					PATH_FILE(texPath, "LightSource.dds"),					true,	1	},
		{ DXGI_FORMAT_UNKNOWN,			"Cornell_Light",				PATH_FILE(texPath, "Cornell_Light.dds"),				true,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"Transparent",					PATH_FILE(texPath, "Transparent.dds"),					false,	2	},
		{ DXGI_FORMAT_UNKNOWN,			"Transparent2",					PATH_FILE(texPath, "Transparent2.dds"),					false,	2	},
#endif
	};

#ifdef USE_BAKED_TEXTURES
	for (TextureData &texData : textureNames)
	{
		if (texData.file.ends_with(".dds") || texData.file.ends_with(".DDS"))
			continue;

		size_t extPos = texData.file.find_last_of('.');
		if (extPos != std::string::npos)
			texData.file = texData.file.substr(0, extPos);

		texData.file += ".dds";
	}
#endif

	std::ifstream texStream(ASSET_REGISTRY_FILE_TEXTURES);
	if (texStream)
	{
		std::vector<DXGI_FORMAT> formatStack;
		std::vector<bool> mipmappedStack;
		std::vector<int> downsampleStack;
		std::vector<bool> editorOnlyStack;

		while (std::getline(texStream, line))
		{
			auto comment = line.find('#');
			if (comment != std::string::npos)
				line = line.substr(0, comment);

			line = StringUtils::Trim(line);

			if (line.empty())
				continue;

			// Check if the line is a command to push or pop texture parameters
			// Example: 
			//	 <push transparent=true mipmapped=false downsample=1>
			//	 <pop transparent downsample>
			if (line[0] == '<')
			{
				// Remove the '<' and '>' characters
				line.erase(0, 1);
				if (line.back() == '>')
					line.pop_back();

				// Check if it's a push or pop command
				bool isPush;
				if (line.starts_with("push"))
				{
					line.erase(0, 5); // Remove "push "
					isPush = true;
				}
				else if (line.starts_with("pop"))
				{
					line.erase(0, 4); // Remove "pop "
					isPush = false;
				}
				else
				{
					Warn(std::format("Invalid command: '{}'", line));
					continue;
				}

				// Get each parameter name and value
				std::istringstream paramStream(line);
				std::string param;

				while (std::getline(paramStream, param, ' '))
				{
					if (param.empty())
						continue;

					std::string key, value;

					if (isPush)
					{
						auto equalPos = param.find('=');
						if (equalPos == std::string::npos)
						{
							Warn(std::format("Invalid parameter format: '{}'", param));
							continue;
						}

						key = param.substr(0, equalPos);
						value = param.substr(equalPos + 1);
					}
					else
					{
						key = param;
					}

					if (key == "editoronly" || key == "e")
					{
						if (isPush)
							editorOnlyStack.push_back(value == "true");
						else if (!editorOnlyStack.empty())
							editorOnlyStack.pop_back();
					}
					else if (key == "mipmapped" || key == "m")
					{
						if (isPush)
							mipmappedStack.push_back(value == "true");
						else if (!mipmappedStack.empty())
							mipmappedStack.pop_back();
					}
					else if (key == "downsample" || key == "d")
					{
						if (isPush)
							downsampleStack.push_back(std::stoi(value));
						else if (!downsampleStack.empty())
							downsampleStack.pop_back();
					}
					else if (key == "format" || key == "f")
					{
						std::transform(value.begin(), value.end(), value.begin(), ::toupper);
						DXGI_FORMAT format = D3D11FormatData::TypeFromName(value);

						if (isPush)
							formatStack.push_back(format);
						else if (!formatStack.empty())
							formatStack.pop_back();
					}
					else
					{
						Warn(std::format("Unknown texture parameter: '{}'", key));
					}
				}

				continue;
			}

			size_t dotPos = line.find_last_of('.');
			if (dotPos == std::string::npos || dotPos == 0 || dotPos == line.size() - 1)
			{
				WarnF("Invalid texture name: '{}'", line);
				continue;
			}

			size_t fileNameStart = line.find_last_of("/\\");
			if (fileNameStart == std::string::npos)
				fileNameStart = 0;
			else
				fileNameStart += 1;

			std::string name = line.substr(fileNameStart, dotPos - fileNameStart);

			// Check if the texture is already in the list
			bool found = false;
			UINT textureCount = static_cast<UINT>(textureNames.size());
			for (UINT i = 0; i < textureCount; i++)
			{
				if (textureNames[i].name == name)
				{
					found = true;
					break;
				}
			}

			if (found)
				continue;

			bool editorOnly = editorOnlyStack.empty() ? false : editorOnlyStack.back();
			bool mipmapped = mipmappedStack.empty() ? true : mipmappedStack.back();
			int downsample = downsampleStack.empty() ? 0 : downsampleStack.back();
			DXGI_FORMAT format = formatStack.empty() ? DXGI_FORMAT_UNKNOWN : formatStack.back();

#ifndef DEBUG_BUILD
			if (editorOnly)
				continue;
#endif

#ifdef USE_BAKED_TEXTURES
			mipmapped = true;
			downsample = 0;
			format = DXGI_FORMAT_UNKNOWN;

			size_t assetPathStart = line.find(ASSET_PATH_TEXTURES);
			if (assetPathStart == std::string::npos)
				assetPathStart = 0;
			else
				assetPathStart += strlen(ASSET_PATH_TEXTURES) + 1;

			std::string assetPath = line.substr(assetPathStart, fileNameStart - assetPathStart);

			line = PATH_FILE_EXT(ASSET_COMPILED_PATH_TEXTURES, assetPath + name, "dds");
#else
			line = PATH_FILE(ASSET_PATH_TEXTURES, line);
#endif

			textureNames.emplace(textureNames.begin(), format, name, line, mipmapped, downsample);
		}
		texStream.close();
	}

	texPath = ASSET_PATH_TEXTURES; // HACK: cubemaps should also be baked. Until then, always load from source
	std::vector<TextureData> cubemapNames = {
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"Fallback",				PATH_FILE(texPath, "FallbackCubemap.png"),				true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"RundownIndustrial",	PATH_FILE(texPath, "RundownIndustrialCubemap.png"),		true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"SandstoneCity",		PATH_FILE(texPath, "SandstoneCityCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"MonkstownCastle",		PATH_FILE(texPath, "MonkstownCastleCubemap.png"),		true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"AutumnForest",			PATH_FILE(texPath, "AutumnForestCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"BlankSkybox",			PATH_FILE(texPath, "BlankSkyboxCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"ParkTrail",			PATH_FILE(texPath, "ParkTrailCubemap.png"),				true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"SuburbanNight",		PATH_FILE(texPath, "SuburbanNightCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"RooftopNight",			PATH_FILE(texPath, "RooftopNightCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"BlueGrotto",			PATH_FILE(texPath, "BlueGrottoCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"BurntWarehouse",		PATH_FILE(texPath, "BurntWarehouseCubemap.png"),		true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"CanaryWharf",			PATH_FILE(texPath, "CanaryWharfCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"SandstoneCove",		PATH_FILE(texPath, "SandstoneCoveCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"HospitalRoom",			PATH_FILE(texPath, "HospitalRoomCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"WoodsRiverbank",		PATH_FILE(texPath, "WoodsRiverbankCubemap.png"),		true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"WinterCityNight",		PATH_FILE(texPath, "WinterCityNightCubemap.png"),		true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"IndustrialAlley",		PATH_FILE(texPath, "IndustrialAlleyCubemap.png"),		true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"CaveWall",				PATH_FILE(texPath, "CaveWallCubemap.png"),				true,	0 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM,		"NeonCityNight",		PATH_FILE(texPath, "NeonCityNightCubemap.png"),			true,	0 },
		{ DXGI_FORMAT_R16G16B16A16_UNORM,	"BarrenFieldHDR",		PATH_FILE(texPath, "BarrenFieldHDRCubemap.png"),		true,	0 },
	};

	// Height Maps should be placed in the Texture folder
	std::vector<HeightMapData> heightMapNames = {
		{ "CaveHeightmap",					"CaveHeightmap.png"},
		{ "CaveRoofHeightmap",				"CaveRoofHeightmap.png"},
		{ "CaveWallsHeightmap",				"CaveWallsHeightmap.png"},
#ifdef DEBUG_BUILD
		{ "ExampleHeightMap",				"ExampleHeightMap.png"},
		{ "ExampleHeightMap2",				"ExampleHeightMap2.png"},
#endif
	};

	std::vector<ShaderData> shaderNames = {
		{ ShaderType::VERTEX_SHADER,		"VS_Geometry",					"VS_Geometry"					},
		{ ShaderType::VERTEX_SHADER,		"VS_GeometryDistortion",		"VS_GeometryDistortion"			},
		{ ShaderType::VERTEX_SHADER,		"VS_Depth",						"VS_Depth"						},
		{ ShaderType::VERTEX_SHADER,		"VS_DepthDistortion",			"VS_DepthDistortion"			},
		{ ShaderType::VERTEX_SHADER,		"VS_DepthCubemap",				"VS_DepthCubemap"				},
		{ ShaderType::VERTEX_SHADER,		"VS_DepthDistortionCubemap",	"VS_DepthDistortionCubemap"		},
		{ ShaderType::VERTEX_SHADER,		"VS_ScreenEffect",				"VS_ScreenEffect"				},

		{ ShaderType::GEOMETRY_SHADER,		"GS_Billboard",					"GS_Billboard"					},
		{ ShaderType::GEOMETRY_SHADER,		"GS_DepthCubemap",				"GS_DepthCubemap"				},

		{ ShaderType::PIXEL_SHADER,			"PS_Geometry",					"PS_Geometry"					},
		{ ShaderType::PIXEL_SHADER,			"PS_GeometryMetallic",			"PS_GeometryMetallic"			},
		{ ShaderType::PIXEL_SHADER,			"PS_TriPlanar",					"PS_TriPlanar"					},
		{ ShaderType::PIXEL_SHADER,			"PS_ReflectionProbe",			"PS_ReflectionProbe"			},
		{ ShaderType::PIXEL_SHADER,			"PS_Static",					"PS_Static"						},
		{ ShaderType::PIXEL_SHADER,			"PS_Transparent",				"PS_Transparent"				},
		{ ShaderType::PIXEL_SHADER,			"PS_DepthCubemap",				"PS_DepthCubemap"				},
		{ ShaderType::PIXEL_SHADER,			"PS_SkyboxDefault",				"PS_SkyboxDefault"				},
		{ ShaderType::PIXEL_SHADER,			"PS_SkyboxNormal",				"PS_SkyboxNormal"				},
		{ ShaderType::PIXEL_SHADER,			"PS_SkyboxEnvironment",			"PS_SkyboxEnvironment"			},
		{ ShaderType::PIXEL_SHADER,			"PS_ScreenEffectFog",			"PS_ScreenEffectFog"			},

		{ ShaderType::COMPUTE_SHADER,		"CS_BlurHorizontalFX",			"CS_BlurHorizontalFX"			},
		{ ShaderType::COMPUTE_SHADER,		"CS_BlurVerticalFX",			"CS_BlurVerticalFX"				},
		{ ShaderType::COMPUTE_SHADER,		"CS_BlurHorizontalEmission",	"CS_BlurHorizontalEmission"		},
		{ ShaderType::COMPUTE_SHADER,		"CS_BlurVerticalEmission",		"CS_BlurVerticalEmission"		},
		{ ShaderType::COMPUTE_SHADER,		"CS_Downsample",				"CS_Downsample"					},
		{ ShaderType::COMPUTE_SHADER,		"CS_DownsampleCheap",			"CS_DownsampleCheap"			},
		{ ShaderType::COMPUTE_SHADER,		"CS_FogFX",						"CS_FogFX"						},
		{ ShaderType::COMPUTE_SHADER,		"CS_CombineFX",					"CS_CombineFX"					},

#ifdef DEBUG_BUILD
		{ ShaderType::VERTEX_SHADER,		"VS_DebugDraw",					"VS_DebugDraw"					},
		{ ShaderType::VERTEX_SHADER,		"VS_DebugDrawTri",				"VS_DebugDrawTri"				},
		{ ShaderType::VERTEX_SHADER,		"VS_DebugDrawSprite",			"VS_DebugDrawSprite"			},

		{ ShaderType::GEOMETRY_SHADER,		"GS_DebugLine",					"GS_DebugLine"					},
		{ ShaderType::GEOMETRY_SHADER,		"GS_DebugDrawSprite",			"GS_DebugDrawSprite"			},

		{ ShaderType::PIXEL_SHADER,			"PS_SelectionOutline",			"PS_SelectionOutline"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugDraw",					"PS_DebugDraw"					},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugDrawSprite",			"PS_DebugDrawSprite"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewPosition",			"PS_DebugViewPosition"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewNormal",			"PS_DebugViewNormal"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewAmbient",			"PS_DebugViewAmbient"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewDiffuse",			"PS_DebugViewDiffuse"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewDepth",			"PS_DebugViewDepth"				},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewLighting",			"PS_DebugViewLighting"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewShadow",			"PS_DebugViewShadow"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewReflection",		"PS_DebugViewReflection"		},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewReflectivity",		"PS_DebugViewReflectivity"		},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewSpecular",			"PS_DebugViewSpecular"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewSpecStr",			"PS_DebugViewSpecStr"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewUVCoords",			"PS_DebugViewUVCoords"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewOcclusion",		"PS_DebugViewOcclusion"			},
		{ ShaderType::PIXEL_SHADER,			"PS_DebugViewLightTiles",		"PS_DebugViewLightTiles"		},

		{ ShaderType::COMPUTE_SHADER,		"CS_BlurHorizontalOutline",		"CS_BlurHorizontalOutline"		},
		{ ShaderType::COMPUTE_SHADER,		"CS_BlurVerticalOutline",		"CS_BlurVerticalOutline"		},
		{ ShaderType::COMPUTE_SHADER,		"CS_CombineOutlineFX",			"CS_CombineOutlineFX"			},
#endif
	};

	time.TakeSnapshot("LoadContent");
	if (!LoadContent(textureNames, cubemapNames, shaderNames, heightMapNames))
	{
		ErrMsg("Failed to load game content!");
		return false;
	}
	time.TakeSnapshot("LoadContent");

	// Add all scenes & load the active scene
	time.TakeSnapshot("AddScenes");
	{
		Scene *tempScene = nullptr;

		// Search for all .scene files in ASSET_PATH_SAVES
		for (const auto &entry : std::filesystem::directory_iterator(ASSET_PATH_SCENES))
		{
			const auto &path = entry.path();
			std::string filename = path.filename().string();
			std::string ext = filename.c_str() + filename.find_last_of('.') + 1;

			if (ext != ASSET_EXT_SCENE)
				continue; // Skip non-scene files

			filename = filename.substr(0, filename.find_last_of('.'));

			// Load the scene
			DbgMsgF("Adding Scene '{}'...", filename); LogIndentIncr();
			tempScene = new Scene(filename);
			if (!AddScene(&tempScene))
			{
				ErrMsg("Failed to add scene!");
				return false;
			}
			LogIndentDecr();
		}

#ifndef EDIT_MODE
		DbgMsg("Adding Scene 'Credits'..."); LogIndentIncr();
		tempScene = new Scene("Credits", true);
		if (!AddScene(&tempScene))
		{
			ErrMsg("Failed to add scene!");
			return false;
		}
		LogIndentDecr();
#endif

#ifdef EDIT_MODE
		const std::string &activeSceneName = DebugData::Get().activeScene;
		DbgMsgF("Loading Active Scene '{}'...", activeSceneName); LogIndentIncr();

		// Set active scene to the game scene
		if (!SetScene(activeSceneName))
		{
			LogIndentDecr();
			DbgMsg("Loading Active Scene Failed! \nDefaulting to 'MainMenu'..."); 
			LogIndentIncr();

			if (!SetScene("MainMenu"))
			{
				ErrMsg("Failed to set scene!");
				return false;
			}
		}
#else
		DbgMsg("Loading Active Scene 'MainMenu'..."); LogIndentIncr();
		if (!SetScene("MainMenu"))
		{
			ErrMsg("Failed to set scene!");
			return -1;
		}
#endif
		LogIndentDecr();
	}
	time.TakeSnapshot("AddScenes");

	// Ensure the main menu scene is loaded
	if (_pendingSceneChange != "MainMenu")
	{
		if (!SetSceneInternal("MainMenu"))
			DbgMsg("Failed to load MainMenu scene!");
	}

	// Then open the pending scene (if there is one)
	if (!_pendingSceneChange.empty())
	{
		if (!SetSceneInternal(_pendingSceneChange))
			DbgMsgF("Failed to change scene to '{}'!", _pendingSceneChange);

		_pendingSceneChange.clear();
	}

#ifdef _DEBUG
	_graphics.notifications.emplace_back("Debug", 0);
#endif
#ifdef DEBUG_D3D11_DEVICE
	_graphics.notifications.emplace_back("D3D11 Debug Device", 0);
#endif
#if (MESH_COLLISION_DETAIL_REDUCTION == 3)
	_graphics.notifications.emplace_back("Mesh Collision Disabled", 2, 15.0f);
#endif

	// Create worker thread
	_workerThread = std::thread(&Game::UpdateWorker, this);

	return true;
}

bool Game::AddScene(Scene **newScene, const bool setActive)
{
	if (newScene == nullptr)
		return false;

	if (*newScene == nullptr)
		return false;

	// Take ownership of the scene
	Scene *scene = *newScene;
	(*newScene) = nullptr;

	// Ensure the scene name is unique
	for (const auto &existingScene : _scenes)
	{
		if (existingScene->GetName() == scene->GetName())
		{
			delete scene;
			DbgMsgF("A scene with the name '{}' already exists!", scene->GetName());
			return false;
		}
	}

	const UINT sceneIndex = (UINT)_scenes.size();
	_scenes.emplace_back(scene);

	if (setActive)
		return SetScene(sceneIndex);

	return true;
}
bool Game::ActiveSceneIsValid()
{
	if (_activeSceneIndex < _scenes.size())
	{
		if (_scenes[_activeSceneIndex] == nullptr)
		{
			return false;
		}

		if (!_scenes[_activeSceneIndex]->IsInitialized())
		{
			return false;
		}
	}

	return true;
}

bool Game::SetSceneInternal(const std::string &sceneName)
{
	UINT sceneIndex = GetSceneIndex(sceneName);
	if (sceneIndex == CONTENT_NULL)
	{
		DbgMsgF("Scene '{}' not found!", sceneName);
		return false;
	}

	if (sceneIndex == _activeSceneIndex)
		return true;

	Scene *prevScene = _activeSceneIndex == -1 ? nullptr : _scenes[_activeSceneIndex].get();
	Scene *scene = _scenes[sceneIndex].get();

	if (!scene)
		return false;

	_activeSceneIndex = sceneIndex;

	if (prevScene)
		prevScene->ExitScene();

	if (!scene->IsInitialized())
	{
		if (sceneName == "MainMenu")
		{
			if (!scene->InitializeMenu(sceneName, _device.Get(), _immediateContext.Get(), this, &_content, &_graphics, _gameVolume))
			{
				ErrMsg("Failed to initialize menu scene!");
				return false;
			}
		}
		else if (sceneName == "Cave")
		{
			if (!scene->InitializeCave(sceneName, _device.Get(), _immediateContext.Get(), this, &_content, &_graphics, _gameVolume))
			{
				ErrMsg("Failed to initialize scene!");
				return false;
			}
		}
		else if (sceneName == "Credits")
		{
			if (!scene->InitializeCred(sceneName, _device.Get(), _immediateContext.Get(), this, &_content, &_graphics, _gameVolume))
			{
				ErrMsg("Failed to initialize scene!");
				return false;
			}
		}
		else if (sceneName == "StartCutscene")
		{
			if (!scene->InitializeEntr(sceneName, _device.Get(), _immediateContext.Get(), this, &_content, &_graphics, _gameVolume))
			{
				ErrMsg("Failed to initialize scene!");
				return false;
			}
		}
		else
		{
			if (!scene->InitializeBase(sceneName, _device.Get(), _immediateContext.Get(), this, &_content, &_graphics, _gameVolume))
			{
				ErrMsg("Failed to initialize scene!");
				return false;
			}
		}
	}

	scene->EnterScene();
	scene->SetSceneVolume(_gameVolume);

#ifdef DEBUG_BUILD
	if (!scene->IsTransitionScene())
		DebugData::Get().activeScene = sceneName;
#endif

	return true;
}
bool Game::SetScene(const UINT sceneIndex)
{
	if (sceneIndex >= _scenes.size())
	{
		DbgMsgF("Scene index '{}' is out of range!", sceneIndex);
		return false;
	}

	Scene *scene = _scenes[sceneIndex].get();
	if (!scene)
	{
		DbgMsgF("Scene at index '{}' is null!", sceneIndex);
		return false;
	}

	_pendingSceneChange = scene->GetName();
	return true;
}
bool Game::SetScene(const std::string &sceneName)
{
	UINT sceneIndex = GetSceneIndex(sceneName);
	if (sceneIndex == CONTENT_NULL)
	{
		DbgMsgF("Scene '{}' not found!", sceneName);
		return false;
	}

	_pendingSceneChange = sceneName;
	return true;
}

Scene *Game::GetScene(const UINT sceneIndex)
{
	return _scenes[sceneIndex].get();
}
Scene *Game::GetScene(const std::string &sceneName)
{
	UINT sceneIndex = GetSceneIndex(sceneName);
	if (sceneIndex == CONTENT_NULL)
		return nullptr;

	return _scenes[sceneIndex].get();
}
Scene *Game::GetSceneByUID(const size_t uid)
{
	for (const auto &scene : _scenes)
	{
		if (scene->GetUID() == uid)
			return scene.get();
	}
	return nullptr;
}

const std::vector<std::unique_ptr<Scene>> *Game::GetScenes() const noexcept
{
	return &_scenes;
}
UINT Game::GetSceneIndex(const std::string &sceneName) noexcept
{
	for (UINT i = 0; i < _scenes.size(); i++)
	{
		if (_scenes[i]->GetName() == sceneName)
		{
			return i;
		}
	}
	return CONTENT_NULL;
}
UINT Game::GetActiveSceneIndex() const noexcept
{
	return _activeSceneIndex;
}
std::string_view Game::GetActiveSceneName() const noexcept
{
	if (_activeSceneIndex == CONTENT_NULL)
		return "";
	if (_activeSceneIndex >= _scenes.size())
		return "";
	if (!_scenes[_activeSceneIndex])
		return "";

	return _scenes[_activeSceneIndex]->GetName();
}

Graphics *Game::GetGraphics() noexcept
{
	return &_graphics;
}
Window &Game::GetWindow() noexcept
{
	return _window;
}

float Game::GetGameVolume() const noexcept
{
	return _gameVolume;
}
void Game::SetGameVolume(float volume)
{
	_gameVolume = volume;

	if (ActiveSceneIsValid())
		_scenes[_activeSceneIndex]->SetSceneVolume(_gameVolume);
}

bool Game::Update(TimeUtils &time, const Input& input)
{
	ZoneScopedC(RandomUniqueColor());

	for (const std::string &sceneName : _pendingSceneRemovals)
	{
		UINT sceneIndex = GetSceneIndex(sceneName);
		if (sceneIndex == CONTENT_NULL)
			continue;

		_scenes.erase(_scenes.begin() + sceneIndex);

		if (sceneIndex == _activeSceneIndex)
		{
			UINT newSceneIndex = sceneIndex;
			if (newSceneIndex >= _scenes.size())
				newSceneIndex = _scenes.size() - 1;

			const std::string &newSceneName = _scenes[newSceneIndex]->GetName();
			if (!SetSceneInternal(newSceneName))
				return false;
		}
		else if (sceneIndex < _activeSceneIndex)
		{
			_activeSceneIndex--;
		}
	}
	_pendingSceneRemovals.clear();

	if (!_pendingSceneChange.empty())
	{
		if (!SetSceneInternal(_pendingSceneChange))
			DbgMsgF("Failed to change scene to '{}'!", _pendingSceneChange);

		_pendingSceneChange.clear();
	}

	// Update
	time.TakeSnapshot("SceneUpdateTime");
	if (ActiveSceneIsValid())
	{
		if (!_scenes[_activeSceneIndex]->Update(time, input))
		{
			ErrMsg("Failed to update scene!");
			return false;
		}
	}
	time.TakeSnapshot("SceneUpdateTime");

	// Fixed update
	static bool firstFixedUpdate = true;
	_tickTimer += time.GetDeltaTime();
	while (_tickTimer >= time.GetFixedDeltaTime())
	{
		time.TakeSnapshot("SceneFixedUpdateTime");
		_tickTimer -= time.GetFixedDeltaTime();
		if (firstFixedUpdate)
		{
			firstFixedUpdate = false;
			_tickTimer = 0.0f;
		}

		if (ActiveSceneIsValid())
		{
			if (!_scenes[_activeSceneIndex]->FixedUpdate(time.GetFixedDeltaTime(), input))
			{
				ErrMsg("Failed to update scene at fixed step!");
				return false;
			}
		}

#ifdef _DEBUG
		if (_tickTimer >= time.GetFixedDeltaTime() * 4.0f)
			_tickTimer = time.GetFixedDeltaTime() * 4.0f;
#endif
		time.TakeSnapshot("SceneFixedUpdateTime");
	}

	// Late update
	time.TakeSnapshot("SceneLateUpdateTime");
	if (ActiveSceneIsValid())
	{
		if (!_scenes[_activeSceneIndex]->LateUpdate(time, input))
		{
			ErrMsg("Failed to late update scene!");
			return false;
		}
	}
	time.TakeSnapshot("SceneLateUpdateTime");

	return true;
}
void Game::UpdateWorker()
{
	tracy::SetThreadName("Worker Thread");

	while (true)
	{
		_workerSemaphore.acquire();

		if (!_workerState)
			return;
		
		// Update worker thread
		{
			ZoneScopedC(RandomUniqueColor());

			if (!_graphics.ResetRenderState())
			{
				_workerState = false;
				return;
			}

			if (ActiveSceneIsValid())
			{
				// Fixed update
				/*static bool firstFixedUpdate = true;
				_tickTimer += Time::GetInstance().deltaTime;
				while (_tickTimer >= _tickRate)
				{
					Time::GetInstance().TakeSnapshot("SceneFixedUpdateTime");
					_tickTimer -= _tickRate;
					if (firstFixedUpdate)
					{
						firstFixedUpdate = false;
						_tickTimer = 0.0f;
					}

					if (!_scenes[_activeSceneIndex]->FixedUpdate(_tickRate, *Input::GetInstance()))
					{
						ErrMsg("Failed to update scene at fixed step!");
						_workerState = false;
						return;
					}

#ifdef _DEBUG
					if (_tickTimer >= _tickRate * 4.0f)
						_tickTimer = _tickRate * 4.0f;
#endif
					Time::GetInstance().TakeSnapshot("SceneFixedUpdateTime");
				}*/


				if (!_scenes[_activeSceneIndex]->UpdateCullingTree())
				{
					_workerState = false;
					return;
				}
			}
		}

		_mainSemaphore.release();

		if (!_workerState)
			return;
	}
}

bool Game::Render(TimeUtils &time, const Input& input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_graphics.BeginSceneRender())
	{
		ErrMsg("Failed to begin rendering!");
		return false;
	}

	time.TakeSnapshot("SceneRenderTime");
	if (ActiveSceneIsValid())
		if (!_scenes[_activeSceneIndex]->Render(time, input))
		{
			ErrMsg("Failed to render scene!");
			return false;
		}
	time.TakeSnapshot("SceneRenderTime");

	if (!_graphics.EndSceneRender(time))
	{
		ErrMsg("Failed to end rendering!");
		return false;
	}
	
#ifdef USE_IMGUI
	if (_pendingLayoutChange != "")
	{
		UILayout::LoadLayout(_pendingLayoutChange);
		_pendingLayoutChange = "";
	}

	if (!_graphics.BeginUIRender())
	{
		ErrMsg("Failed to begin UI rendering!");
		return false;
	}

	if (!RenderUI(time))
	{
		ErrMsg("Failed to render UI!");
		return false;
	}

	if (!_graphics.EndUIRender())
	{
		ErrMsg("Failed to end UI rendering!");
		return false;
	}
#endif

	if (!_graphics.ScreenSpaceRender())
	{
		ErrMsg("Failed to render screen-space!");
		return false;
	}

	// While the main thread is waiting for the GPU to finish, let worker thread work.
	_workerSemaphore.release();

	if (!_graphics.EndFrame())
	{
		ErrMsg("Failed to end frame!");
		return false;
	}

	// Wait for the worker thread to finish.
	_mainSemaphore.acquire();

	// Check result of worker thread.
	if (!_workerState)
	{
		ErrMsg("Worker thread failed!");
		return false;
	}

	return true;
}

#ifdef USE_IMGUI
bool Game::RenderUI(TimeUtils &time)
{
	ZoneScopedC(RandomUniqueColor());

	Input &input = Input::Instance();
	DebugData &debugData = DebugData::Get();

	int mainDockID = ImGui::GetID("Main");
	ImGuiWindowFlags_ defaultWindowFlags = (ImGuiWindowFlags_)((ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs) & ~ImGuiWindowFlags_NoCollapse);
	ImGuiWindowFlags viewWindowFlags = defaultWindowFlags | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar;
	float &imGuiFontScale = debugData.imGuiFontScale;
	int stylesPushed = 0;

	// Set input to be absorbed by default, it may be set back to false from the scene view window.
	input.SetKeyboardAbsorbed(true);
	input.SetMouseAbsorbed(true);

	stylesPushed = 0;
	stylesPushed++; ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	stylesPushed++; ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

	//ImGui::SetNextWindowDockID(_graphics.GetBackgroundDockID(), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("View", nullptr, viewWindowFlags))
	{
		ImGui::PopStyleVar(stylesPushed);

		ImGui::SetWindowFontScale(imGuiFontScale);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Settings##ViewViewMenu"))
			{
				bool sceneValid = ActiveSceneIsValid();

				{
					static std::string selectedLoadout = "";
					static std::string styleName = "";

					if (ImGui::Button("Load Window Layout"))
					{
						ImGui::OpenPopup("Load Layout");
						selectedLoadout = DebugData::Get().layoutName;
					}
					ImGui::SameLine();
					if (ImGui::Button("Save Window Layout"))
					{
						ImGui::OpenPopup("Save Layout");
						styleName = DebugData::Get().layoutName;
					}

					if (ImGui::BeginPopup("Load Layout"))
					{
						std::vector<std::string> layouts;
						UILayout::GetLayoutNames(layouts);

						if (ImGui::BeginCombo("Layouts", selectedLoadout.c_str()))
						{
							for (int i = 0; i < layouts.size(); i++)
							{
								const bool isSelected = layouts[i] == selectedLoadout;
								if (ImGui::Selectable(layouts[i].c_str(), isSelected))
									selectedLoadout = layouts[i];

								if (isSelected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}

						if (ImGui::Button("Confirm"))
						{
							_pendingLayoutChange = selectedLoadout;
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();

						if (ImGui::Button("Cancel"))
							ImGui::CloseCurrentPopup();

						ImGui::EndPopup();
					}

					if (ImGui::BeginPopup("Save Layout"))
					{
						ImGui::InputText("Layout Name", &styleName);

						if (ImGui::Button("Save"))
						{
							UILayout::SaveLayout(styleName);
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();

						if (ImGui::Button("Cancel"))
							ImGui::CloseCurrentPopup();

						ImGui::EndPopup();
					}
				}

				ImGui::SeparatorText("Control");

				if (sceneValid)
				{
					Scene *scene = _scenes[_activeSceneIndex].get();
					SceneHolder *sceneHolder = scene->GetSceneHolder();
					CameraBehaviour *viewCam = scene->GetViewCamera();

					if (ImGui::BeginMenu("Camera View"))
					{
						SceneContents::SceneIterator entIter = sceneHolder->GetEntities();

						while (Entity *ent = entIter.Step())
						{
							CameraBehaviour *cam = nullptr;
							if (!ent->GetBehaviourByType<CameraBehaviour>(cam))
								continue;

							if (ImGui::MenuItem(std::format("{}##SelectViewCamID{}", ent->GetName(), ent->GetID()).c_str(), NULL, cam == viewCam))
								scene->SetViewCamera(cam);
						}

						ImGui::EndMenu();
					}

					if (ImGui::Button("Select View Camera"))
						scene->SetSelection(viewCam->GetEntity());
				}
				
				float sensitivity = input.GetMouseSensitivity();
				if (ImGui::SliderFloat("Sensitivity", &sensitivity, 0.0f, 2.0f))
					input.SetMouseSensitivity(sensitivity);
				
				ImGui::DragFloat("Movement Speed", &debugData.movementSpeed, 0.01f, 0.0001f);
				ImGuiUtils::LockMouseOnActive();

				if (Window *wnd = input.GetWindow())
				{
					ImGui::SeparatorText("Window");
					static dx::XMINT2 windowResolution = {
						debugData.windowSizeX,
						debugData.windowSizeY
					};

					if (ImGui::InputInt2("##WindowResolutionInput", &windowResolution.x))
					{
						windowResolution.x = max(1, windowResolution.x);
						windowResolution.y = max(1, windowResolution.y);
					}

					if (ImGui::Button("Apply##WindowResolutionApply"))
					{
						debugData.windowSizeX = windowResolution.x;
						debugData.windowSizeY = windowResolution.y;
						wnd->SetWindowSize(windowResolution);
					}

					ImGui::SameLine();
					if (ImGui::Button("Copy Display##WindowResolutionCopyDisplay"))
					{
						auto screenSize = input.GetScreenSize();
						windowResolution.x = (int)screenSize.x;
						windowResolution.y = (int)screenSize.y;
					}

					bool isFullscreen = wnd->IsFullscreen();
					if (ImGui::MenuItem("Fullscreen", NULL, &isFullscreen))
						wnd->SetFullscreen(isFullscreen);
				}

				ImGui::SeparatorText("Scene");
				static dx::XMINT2 sceneResolution = { 
					debugData.sceneViewSizeX, 
					debugData.sceneViewSizeY 
				};

				if (ImGui::InputInt2("##SceneResolutionInput", &sceneResolution.x))
				{
					sceneResolution.x = max(1, sceneResolution.x);
					sceneResolution.y = max(1, sceneResolution.y);
				}

				if (ImGui::Button("Apply##SceneResolutionApply"))
				{
					debugData.sceneViewSizeX = sceneResolution.x;
					debugData.sceneViewSizeY = sceneResolution.y;
				}

				ImGui::SameLine();
				if (ImGui::Button("Copy Display##SceneResolutionCopyDisplay"))
				{
					auto screenSize = input.GetScreenSize();
					sceneResolution.x = (int)screenSize.x;
					sceneResolution.y = (int)screenSize.y;
				}

				ImGui::MenuItem("Stretch to Fit", NULL, &debugData.stretchToFitView);

				ImGui::SeparatorText("Gizmos");

				ImGui::MenuItem("View Manipulator", NULL, &debugData.showViewManipGizmo);

				if (sceneValid)
				{
					Scene *scene = _scenes[_activeSceneIndex].get();
					bool doUpdate = false;
					
					if (ImGui::MenuItem("Draw Icons", NULL, &debugData.billboardGizmosDraw))
						doUpdate = true;

					if (debugData.billboardGizmosDraw)
					{
						if (ImGui::MenuItem("Overlay Icons", NULL, &debugData.billboardGizmosOverlay))
							doUpdate = true;

						if (ImGui::DragFloat("Icon Size", &debugData.billboardGizmosSize, 0.001f, 0.05f))
							doUpdate = true;
						ImGuiUtils::LockMouseOnActive();
					}

					if (doUpdate)
						scene->UpdateBillboardGizmos();
				}

				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu("Scene##ViewSceneMenu"))
			{
				ImGui::Text("Current:"); ImGui::SameLine();
				ImGui::Text(_scenes[_activeSceneIndex]->GetName().c_str());

				ImGui::Separator();

				constexpr ImGuiWindowFlags popupFlags =
					ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoDocking;

				if (ImGui::BeginMenu("Scene List##SceneListMenu"))
				{
					UINT newSceneID = _activeSceneIndex;
					bool hasSelected = false;

					float longestNameWidth = 32.0f;
					for (int i = 0; i < _scenes.size(); i++)
					{
						const auto &scene = _scenes[i];

						if (!scene)
							continue;

						float nameWidth = ImGui::CalcTextSize(scene->GetName().c_str()).x;
						if (nameWidth > longestNameWidth)
						{
							longestNameWidth = nameWidth;
						}
					}

					ImGui::PushID("SceneList");
					for (int i = 0; i < _scenes.size(); i++)
					{
						const auto &scene = _scenes[i];
						if (!scene)
							continue;

						const std::string &sceneName = scene->GetName();

						ImGui::PushID(sceneName.c_str());

						const bool isSelected = (_activeSceneIndex == i);
						if (ImGui::Button(sceneName.c_str(), { longestNameWidth + 8.0f, 0.0f }))
						{
							newSceneID = i;
							hasSelected = true;
						}

						if (_scenes.size() > 1)
						{
							ImGui::SameLine();
							ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
							if (ImGui::Button(std::format("X", sceneName, i).c_str()))
							{
								ImGui::OpenPopup("ConfirmDeleteScene");
							}
							ImGuiUtils::EndButtonStyle();

							if (ImGui::BeginPopup("ConfirmDeleteScene", popupFlags))
							{
								ImGui::Text("Are you sure you want to delete scene '%s'?", sceneName.c_str());

								if (ImGui::Button("Delete Scene"))
								{
									std::string sceneFilePath = PATH_FILE_EXT(ASSET_PATH_SCENES, sceneName, ASSET_EXT_SCENE);
									std::filesystem::remove(sceneFilePath);
									_pendingSceneRemovals.push_back(sceneName);
									ImGui::CloseCurrentPopup();
								}

								ImGui::SameLine();
								if (ImGui::Button("Cancel"))
								{
									ImGui::CloseCurrentPopup();
								}

								ImGui::EndPopup();
							}
						}

						ImGui::PopID();
					}
					ImGui::PopID();

					if (hasSelected)
					{
						if (!SetScene(newSceneID))
						{
							ErrMsg("Failed to set scene!");
							ImGui::EndMenu();
							ImGui::EndMenu();
							ImGui::EndMenuBar();
							return false;
						}
					}

					ImGui::EndMenu();
				}
				
				if (ImGui::Button("New Scene"))
				{
					ImGui::OpenPopup("NewScenePopup");
				}

				if (ImGui::BeginPopup("NewScenePopup", popupFlags))
				{
					static std::string newSceneName = "New Scene";
					static dx::BoundingBox newSceneBounds = { {0, 0, 0}, {500, 250, 500} };
					static bool transitional = false;

					ImGui::Text("Create New Scene");

					ImGui::Text("Name:"); ImGui::SameLine();
					ImGui::InputText("##NewSceneName", &newSceneName);

					ImGui::Text("Center:"); ImGui::SameLine();
					ImGui::DragFloat3("##NewSceneCenter", &newSceneBounds.Center.x);

					ImGui::Text("Extents:"); ImGui::SameLine();
					if (ImGui::DragFloat3("##NewSceneExtents", &newSceneBounds.Extents.x))
					{
						newSceneBounds.Extents.x = max(0.1f, newSceneBounds.Extents.x);
						newSceneBounds.Extents.y = max(0.1f, newSceneBounds.Extents.y);
						newSceneBounds.Extents.z = max(0.1f, newSceneBounds.Extents.z);
					}

					ImGui::Text("Transitional:"); ImGui::SameLine();
					ImGui::Checkbox("##NewSceneTransitional", &transitional);

					bool isValid = true;
					if (newSceneName.empty())
					{
						isValid = false;
						ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Input a name!");
					}
					else
					{
						for (const auto &scene : _scenes)
						{
							if (scene && scene->GetName() == newSceneName)
							{
								isValid = false;
								ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Name taken!");
								break;
							}
						}
					}

					ImGui::BeginDisabled(!isValid);
					if (ImGui::Button("Create"))
					{
						Scene *newScene = new Scene(newSceneName, transitional);
						if (!AddScene(&newScene, true))
						{
							delete newScene;
							ErrMsg("Failed to create new scene!");
						}

						newSceneName = "New Scene";
						newSceneBounds = { {0, 0, 0}, {500, 250, 500} };
						transitional = false;

						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					ImGui::EndPopup();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Transform##ViewTransformMenu"))
			{
				static const std::string transformTypes[6] = { "None", "Translate", "Rotate", "Scale", "Universal", "Bounds" };
				int &transformType = debugData.transformType;

				if (ImGui::BeginMenu(std::format("Tool: {}", transformTypes[transformType]).c_str()))
				{
					for (int i = 0; i < 6; i++)
					{
						if (ImGui::MenuItem(std::format("{}##SelectTransformType{}", transformTypes[i], i).c_str(), NULL, transformType == i))
							transformType = i;
					}

					ImGui::EndMenu();
				}

				static const std::string transformOrigins[5] = { "None", "Primary", "Center", "Average", "Separate "};
				int &transformOrigin = debugData.transformOriginMode;

				if (ImGui::BeginMenu(std::format("Origin: {}", transformOrigins[transformOrigin]).c_str()))
				{
					for (int i = 0; i < 4; i++)
					{
						if (ImGui::MenuItem(std::format("{}##SelectTransformOrigin{}", transformOrigins[i], i).c_str(), NULL, transformOrigin == i))
							transformOrigin = i;
					}

					ImGui::BeginDisabled(true);
					if (ImGui::MenuItem("Separate##SelectTransformOrigin4", NULL, transformOrigin == 4))
						transformOrigin = 4;
					ImGui::EndDisabled();

					ImGui::EndMenu();
				}

				int &transformSpace = debugData.transformSpace;
				if (ImGui::Button(transformSpace == (int)Local ? "Space: Local" : "Space: World"))
					transformSpace = transformSpace == (int)Local ? (int)World : (int)Local;

				bool &transformRelative = debugData.transformRelative;
				if (ImGui::Button(transformRelative ? "Relative" : "Absolute"))
					transformRelative = !transformRelative;

				ImGui::Separator();

				ImGui::SliderFloat("Gizmo Scale", &debugData.transformScale, 0.0f, 1.0f);

				ImGui::DragFloat("Snap Size", &debugData.transformSnap, 0.01f, FLT_MIN);
				ImGuiUtils::LockMouseOnActive();

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		if (!_graphics.RenderSceneView())
		{
			ErrMsg("Failed to render scene view!");
			return false;
		}
	}
	else
		ImGui::PopStyleVar(stylesPushed);
	ImGui::End();

	//ImGui::SetNextWindowDockID(mainDockID, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("General", nullptr, defaultWindowFlags))
	{
		ImGui::SetWindowFontScale(imGuiFontScale);

#if defined(_DEBUG) && defined(DEBUG_D3D11_DEVICE)
		ImGui::Checkbox("[D3D11] Report Live Device Objects on Shutdown", &DebugData::Get().reportComObjectsOnShutdown);
#endif

		if (ImGui::DragFloat("Volume", &_gameVolume, 0.05f, 0.0f))
		{
			_gameVolume = max(0, _gameVolume);
			_scenes[_activeSceneIndex]->SetSceneVolume(_gameVolume);
		}
		ImGuiUtils::LockMouseOnActive(); 
		ImGui::Dummy({ 0, 4 });

		float timeScale = time.GetTimeScale();
		if (ImGui::DragFloat("Time Scale", &timeScale, 0.02f))
			time.SetTimeScale(timeScale);
		ImGuiUtils::LockMouseOnActive();

		float fixedDeltaTime = time.GetFixedDeltaTime();
		if (ImGui::DragFloat("Fixed Time Step", &fixedDeltaTime, 0.002f))
			time.SetFixedDeltaTime(fixedDeltaTime);
		ImGuiUtils::LockMouseOnActive();
		ImGui::Dummy({ 0, 4 });

		if (ImGui::DragFloat("ImGui Font Scale", &imGuiFontScale, 0.05f))
			imGuiFontScale = max(0.25f, imGuiFontScale);
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::Button("Reset Font Scale"))
			imGuiFontScale = 1.0f;

		static bool showStyleEditor = false;
		if (ImGui::Button("Show Style Editor"))
			showStyleEditor = true;

		if (showStyleEditor)
		{
			ImGui::Begin("Dear ImGui Style Editor", &showStyleEditor);
			ImGui::ShowStyleEditor();
			ImGui::End();
		}

		ImGui::SetWindowFontScale(imGuiFontScale);
		ImGui::Dummy({ 0, 4 });

		if (ImGui::TreeNode("Utility"))
		{
#ifdef _WIN32
#ifdef TRACY_ENABLE
			if (ImGui::Button("Launch Tracy Profiler"))
			{
				::ShellExecuteA(NULL, "open",
					".\\WellEngine\\Dependencies\\tracy-0.11.1\\Tracy\\tracy-profiler.exe",
					NULL, NULL, SW_SHOWDEFAULT
				);
			}
			ImGui::Dummy({ 0, 4 });
#endif

			if (ImGui::Button("Open ImGui Manual"))
			{
				::ShellExecuteA(NULL, "open",
					"https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html",
					NULL, NULL, SW_SHOWDEFAULT
				);
			}
			ImGui::Dummy({ 0, 4 });
#endif
			ImGui::TreePop();
		}
		ImGui::Dummy({ 0, 4 });

		if (ImGui::TreeNode("Version Info"))
		{
			ImGui::Text("Date: %s", __DATE__);
			ImGui::Dummy({ 0, 2 });

			ImGui::Text("SDL Version: %d", SDL_GetVersion());
			ImGui::Text("ImGui Version: %s, %d", IMGUI_VERSION, IMGUI_VERSION_NUM);
			ImGui::Dummy({ 0, 4 });

			if (ImGui::TreeNode("Compilation Flags"))
			{
#ifdef _DEBUG
				ImGui::Text("Debug Mode");
#else
				ImGui::Text("Release Mode");
#endif
				ImGui::Dummy({ 0, 3 });

#ifdef PARALLEL_UPDATE
				ImGui::Text("Parallel Update");
				ImGui::Text(std::format("Thread Count: {}", PARALLEL_THREADS).c_str());
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef COMPILE_CONTENT
				ImGui::Text("Compile Content");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef TRACY_ENABLE
				ImGui::Text("Tracy Profiler");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef TRACY_MEMORY
				ImGui::Text("Tracy Memory Tracking");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DEBUG_BUILD
				ImGui::Text("Debug Build");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DEBUG_MESSAGES
				ImGui::Text("Debug Messages");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef USE_IMGUIZMO
				ImGui::Text("ImGuizmo");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef EDIT_MODE
				ImGui::Text("Edit Mode");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DEBUG_DRAW
				ImGui::Text("Debug Draw");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DISABLE_MONSTER
				ImGui::Text("Disable Monster");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef LEAK_DETECTION
				ImGui::Text("Leak Detection");
				ImGui::Dummy({ 0, 3 });
#endif
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();

	//ImGui::SetNextWindowDockID(mainDockID, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Performance", nullptr, defaultWindowFlags))
	{
		ImGui::SetWindowFontScale(imGuiFontScale);

		if (ImGui::TreeNode("FPS"))
		{
			constexpr size_t FPS_BUF_SIZE = 256;
			static size_t usedBufSize = FPS_BUF_SIZE;

			static float fpsBuf[FPS_BUF_SIZE]{};
			static size_t fpsBufIndex = 0;

			float currFps = time.GetTimeScale() / time.GetDeltaTime();
			float dTime = 1.0f / currFps;
			fpsBuf[fpsBufIndex] = currFps;

			float avgFps = 0.0f;
			float dropFps = FLT_MAX;
			for (size_t i = 0; i < usedBufSize; i++)
			{
				avgFps += fpsBuf[i];

				if (dropFps > fpsBuf[i])
					dropFps = fpsBuf[i];
			}
			avgFps /= usedBufSize;

			static float minFPS = FLT_MAX;
			if (minFPS > currFps)
				minFPS = currFps;

			static UINT rebaseBufferSizeTimer = 0;
			if (++rebaseBufferSizeTimer >= 30)
			{
				size_t prevSize = usedBufSize;

				rebaseBufferSizeTimer = 0;
				if (avgFps > 240.f && usedBufSize != FPS_BUF_SIZE)
					usedBufSize = FPS_BUF_SIZE;
				else if (avgFps > 90.f && usedBufSize != (FPS_BUF_SIZE / 2))
					usedBufSize = FPS_BUF_SIZE / 2;
				else if (avgFps > 30.f && usedBufSize != (FPS_BUF_SIZE / 4))
					usedBufSize = FPS_BUF_SIZE / 4;
				else if (avgFps > 10.f && usedBufSize != (FPS_BUF_SIZE / 8))
					usedBufSize = FPS_BUF_SIZE / 8;

				if (prevSize != usedBufSize)
				{
					for (size_t i = 0; i < FPS_BUF_SIZE; i++)
						fpsBuf[i] = avgFps;
				}
			}

			// plot
			{
				ImGui::BeginChild("FPS Plot", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);

				static float history = 3.0f;
				ImGui::SliderFloat("History", &history, 0.1f, 15.0f, "%.1f s");

				static float currTimeCoverage = 0.0f;
				static float t = 0.0f;

				static std::vector<ImVec2> plotFpsData;
				if (plotFpsData.empty())
					plotFpsData.reserve(FPS_BUF_SIZE);

				float yData = currFps;

				while (currTimeCoverage > history + 0.1f)
				{
					currTimeCoverage -= 1.0f / plotFpsData[0].y;
					plotFpsData.erase(plotFpsData.begin());
				}
				plotFpsData.emplace_back(t, yData);
				currTimeCoverage += dTime;
				t += dTime;

				float minValue = currFps;
				float maxValue = currFps;
				float avgValue = 0.0f;
				for (int i = 0; i < plotFpsData.size(); i++)
				{
					minValue = min(minValue, plotFpsData[i].y);
					maxValue = max(maxValue, plotFpsData[i].y);
					avgValue += plotFpsData[i].y;
				}
				avgValue /= plotFpsData.size();

				ImVec4 minVals = { t - history, minValue, t, minValue };
				ImVec4 maxVals = { t - history, maxValue, t, maxValue };
				ImVec4 avgVals = { t - history, avgValue, t, avgValue };

				ImPlotFlags plotFlags = ImPlotFlags_None;
				plotFlags |= ImPlotFlags_NoTitle;
				//plotFlags |= ImPlotFlags_NoLegend;
				plotFlags |= ImPlotFlags_NoMouseText;
				plotFlags |= ImPlotFlags_NoBoxSelect;

				ImPlotAxisFlags xFlags = ImPlotAxisFlags_None;
				xFlags |= ImPlotAxisFlags_NoDecorations;

				ImPlotAxisFlags yFlags = ImPlotAxisFlags_None;
				yFlags |= ImPlotAxisFlags_AutoFit;

				ImVec2 availRegion = ImGui::GetContentRegionAvail();

				if (ImPlot::BeginPlot("##Scrolling", availRegion, plotFlags))
				{
					const ImVec2 minMaxFPS = { 0.0f, 165.0f };

					ImPlot::SetupLegend(ImPlotLocation_West);
					ImPlot::SetupAxes(nullptr, nullptr, xFlags, yFlags);
					ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
					ImPlot::SetupAxisLimits(ImAxis_Y1, minMaxFPS.x, minMaxFPS.y);

					ImPlot::PlotLine("Min", &minVals.x, &minVals.y, 2, 0, 0, 2 * sizeof(float));
					ImPlot::PlotLine("Max", &maxVals.x, &maxVals.y, 2, 0, 0, 2 * sizeof(float));
					ImPlot::PlotLine("Avg", &avgVals.x, &avgVals.y, 2, 0, 0, 2 * sizeof(float));

					ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);
					ImPlot::PlotShaded("FPS", &plotFpsData[0].x, &plotFpsData[0].y, plotFpsData.size(), -INFINITY, 0, 0, 2 * sizeof(float));

					ImPlot::EndPlot();
				}
				ImGui::EndChild();
			}

			(++fpsBufIndex) %= usedBufSize;

			char fps[8]{};
			snprintf(fps, sizeof(fps), "%.2f", avgFps);
			ImGui::Text(std::format("Avg: {}", fps).c_str());

			snprintf(fps, sizeof(fps), "%.2f", dropFps);
			ImGui::Text(std::format("Drop: {}", fps).c_str());

			snprintf(fps, sizeof(fps), "%.2f", minFPS);
			ImGui::Text(std::format("Min: {}", fps).c_str());

			static bool countLongAvg = false;
			static bool hasLongAvg = false;
			static float longAvgAccumulation = 0.0f;
			static int longAvgCount = 0;

			ImGui::Text("Long Exposure Avg: ");
			ImGui::SameLine();
			if (!countLongAvg)
			{
				if (ImGui::SmallButton("Start"))
				{
					countLongAvg = true;

					hasLongAvg = true;
					longAvgAccumulation = currFps;
					longAvgCount = 1;
				}
			}
			else
			{
				if (ImGui::SmallButton("Stop"))
					countLongAvg = false;

				longAvgAccumulation += currFps;
				longAvgCount++;
			}

			if (hasLongAvg)
			{
				ImGui::SameLine();
				ImGui::Text(std::format("Iter: {}", longAvgCount).c_str());

				if (longAvgCount > 0)
					ImGui::Text(std::format("Result: {}", (longAvgAccumulation / (float)longAvgCount)).c_str());
				else
					ImGui::Text("Result: NaN");
			}

			if (ImGui::Button("Reset"))
			{
				minFPS = currFps;

				for (size_t i = 0; i < FPS_BUF_SIZE; i++)
					fpsBuf[i] = 0.0f;

				countLongAvg = false;
				hasLongAvg = false;
				longAvgAccumulation = 0.0f;
				longAvgCount = 0;
			}
			ImGui::TreePop();
		}

		ImGui::PushID("Frame Time");
		if (ImGui::TreeNode("Frame Time"))
		{
			char timeStr[32]{};

			snprintf(timeStr, sizeof(timeStr), "%.6f", time.GetDeltaTime());
			ImGui::Text(std::format("{} Frame", timeStr).c_str());

			ImGui::Spacing();

			if (ImGui::TreeNode("Scene"))
			{
				if (ImGui::TreeNode("Update"))
				{
					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("SceneUpdateTime"));
					ImGui::Text(std::format("{} Scene Update", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("SceneLateUpdateTime"));
					ImGui::Text(std::format("{} Scene Late Update", timeStr).c_str());

					static float fixedUpdateTime = -1.0f;
					time.TryCompareSnapshots("SceneFixedUpdateTime", &fixedUpdateTime);
					snprintf(timeStr, sizeof(timeStr), "%.6f", fixedUpdateTime);
					ImGui::Text(std::format("{} Scene Fixed Update", timeStr).c_str());

					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Render"))
				{
					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("SceneRenderTime"));
					ImGui::Text(std::format("{} Scene Render", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingTotal"));
					ImGui::Text(std::format("{} Culling Total", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingSetup"));
					ImGui::Text(std::format("{} Culling Setup", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingCameras"));
					ImGui::Text(std::format("{} Culling Cameras", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingCameraCubes"));
					ImGui::Text(std::format("{} Culling Camera Cubes", timeStr).c_str());

					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			ImGui::Spacing();

			if (ImGui::TreeNode("Graphics"))
			{
				ImGui::TreePop();
			}

			ImGui::Spacing();
			ImGui::TreePop();
		}
		ImGui::PopID();

		if (ImGui::TreeNode("Collisions"))
		{
			char timeStr[32]{};

			snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CollisionChecks"));
			ImGui::Text(std::format("{} Collision Checks Total", timeStr).c_str());

			ImGui::TreePop();
		}
	}
	ImGui::End();

	//ImGui::SetNextWindowDockID(mainDockID, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Content", nullptr, defaultWindowFlags))
	{
		ImGui::SetWindowFontScale(imGuiFontScale);

		if (!_content.RenderUI(_device.Get()))
		{
			ErrMsg("Failed to render content UI!");
			return false;
		}
	}
	ImGui::End();

	//ImGui::SetNextWindowDockID(mainDockID, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Graphics", nullptr, defaultWindowFlags))
	{
		ImGui::SetWindowFontScale(imGuiFontScale);

		if (!_graphics.RenderUI(time))
		{
			ErrMsg("Failed to render graphics UI!");
			return false;
		}
	}
	ImGui::End();

	//ImGui::SetNextWindowDockID(mainDockID, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Scene", nullptr, defaultWindowFlags))
	{
		ImGui::SetWindowFontScale(imGuiFontScale);

		if (ActiveSceneIsValid())
		{
			Scene *scene = _scenes[_activeSceneIndex].get();

			if (!scene->IsUndocked())
			{
				if (ImGui::Button("Undock Scene"))
				{
					if (!ImGuiUtils::Utils::OpenWindow(
						std::format("Scene '{}'", scene->GetName()),
						scene->GetName(),
						std::bind(&Scene::RenderUI, scene)))
					{
						ErrMsg("Failed to open scene window!");
						return false;
					}
				}
			}

			if (!scene->RenderUI())
			{
				ErrMsg("Failed to render scene UI!");
				return false;
			}
		}
	}
	ImGui::End();

	if (!ImGuiUtils::Utils::Render())
	{
		ErrMsg("Failed to render ImGuiUtils!");
		return false;
	}

	//ImGui::SetNextWindowDockID(mainDockID, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Input", nullptr, defaultWindowFlags))
	{
		ImGui::SetWindowFontScale(imGuiFontScale);

		if (!input.RenderUI())
		{
			ErrMsg("Failed to render input UI!");
			return false;
		}
	}
	ImGui::End();

#ifdef USE_IMGUIZMO
	if (ActiveSceneIsValid())
	{
		Scene *scene = _scenes[_activeSceneIndex].get();
		if (!scene->RenderGizmoUI())
		{
			ErrMsg("Failed to render scene gizmo UI!");
			return false;
		}
	}
#endif

	return true;
}
#endif

void Game::Exit()
{
	DbgMsg("Exiting game...");
	_isExiting = true;
}
bool Game::IsExiting() const noexcept
{
	return _isExiting;
}
