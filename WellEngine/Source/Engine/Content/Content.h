#pragma once

#include <vector>
#include <string>
#include <set>

#include "D3D/InputLayoutD3D11.h"
#include "D3D/ShaderD3D11.h"
#include "D3D/MeshD3D11.h"
#include "D3D/SamplerD3D11.h"
#include "D3D/ShaderResourceTextureD3D11.h"
#include "Material.h"
#include "Audio/SoundSource.h"
#include "HeightMap.h"

inline constexpr UINT CONTENT_NULL = 0xFFFFFFFF;

struct MaterialProperties
{
	dx::XMFLOAT4 baseColor		 = {1,1,1,1};	// Base color of the material.
	float		 alphaCutoff	 = 0.0f;		// Alpha cutoff value for discarding pixels.
	float		 specularFactor  = 1.0f;
	float		 normalFactor	 = 1.0f;
	float		 glossFactor	 = 1.0f;
	float		 occlusionFactor = 0.85f;
	float		 reflectivity	 = 0.1f;
	float		 metallic		 = 0.0f;

	// Bitwise flags for if certain maps should be sampled or skipped.
	// 0: Normal		1: Specular			2: glossiness
	// 3: reflective	4: ambient light	5: ambient occlusion
	UINT		 sampleFlags	 = 0u;

	//float _padding[1];		

	UINT SetSampleFlags(
		bool normal,	 bool specular,	bool glossiness,
		bool reflective, bool ambient,	bool occlusion)
	{
		sampleFlags = 0u;
		sampleFlags |= normal		? (1u << 0) : 0u;
		sampleFlags |= specular		? (1u << 1) : 0u;
		sampleFlags |= glossiness	? (1u << 2) : 0u;
		sampleFlags |= reflective	? (1u << 3) : 0u;
		sampleFlags |= ambient		? (1u << 4) : 0u;
		sampleFlags |= occlusion	? (1u << 5) : 0u;
		return sampleFlags;
	}
};

class ContentBase
{
public:
	std::string name;
	UINT id;

	ContentBase(std::string &name, const UINT id) : name(std::move(name)), id(id) { }
	~ContentBase() = default;
	ContentBase(const ContentBase &other) = delete;
	ContentBase &operator=(const ContentBase &other) = delete;
	ContentBase(ContentBase &&other) = delete;
	ContentBase &operator=(ContentBase &&other) = delete;
};

class Mesh : public ContentBase
{
public:
	MeshD3D11 data;

	Mesh(std::string name, const UINT id) : ContentBase(name, id) { }
	~Mesh() = default;
	Mesh(const Mesh &other) = delete;
	Mesh &operator=(const Mesh &other) = delete;
	Mesh(Mesh &&other) = delete;
	Mesh &operator=(Mesh &&other) = delete;
};

class Shader : public ContentBase
{
public:
	ShaderD3D11 data;

	Shader(std::string name, const UINT id) : ContentBase(name, id) { }
	~Shader() = default;
	Shader(const Shader &other) = delete;
	Shader &operator=(const Shader &other) = delete;
	Shader(Shader &&other) = delete;
	Shader &operator=(Shader &&other) = delete;
};

class Texture : public ContentBase
{
public:
	std::string path;
	bool mipmapped = false;
	ShaderResourceTextureD3D11 data;

	Texture(std::string name, std::string path, const UINT id, bool mipmapped) 
		: ContentBase(name, id), path(std::move(path)), mipmapped(mipmapped) { }
	~Texture() = default;
	Texture(const Texture &other) = delete;
	Texture &operator=(const Texture &other) = delete;
	Texture(Texture &&other) = delete;
	Texture &operator=(Texture &&other) = delete;
};

class Cubemap : public ContentBase
{
public:
	std::string path;
	ShaderResourceTextureD3D11 data;

	Cubemap(std::string name, std::string path, const UINT id)
		: ContentBase(name, id), path(std::move(path)) { }
	~Cubemap() = default;
	Cubemap(const Cubemap &other) = delete;
	Cubemap &operator=(const Cubemap &other) = delete;
	Cubemap(Cubemap &&other) = delete;
	Cubemap &operator=(Cubemap &&other) = delete;
};

class Sampler : public ContentBase
{
public:
	SamplerD3D11 data;

	Sampler(std::string name, const UINT id) : ContentBase(name, id) { }
	~Sampler() = default;
	Sampler(const Sampler &other) = delete;
	Sampler &operator=(const Sampler &other) = delete;
	Sampler(Sampler &&other) = delete;
	Sampler &operator=(Sampler &&other) = delete;
};

class BlendState : public ContentBase
{
public:
	ComPtr<ID3D11BlendState> data;

	BlendState(std::string name, const UINT id) : ContentBase(name, id) { }
	~BlendState() = default;
	BlendState(const BlendState &other) = delete;
	BlendState &operator=(const BlendState &other) = delete;
	BlendState(BlendState &&other) = delete;
	BlendState &operator=(BlendState &&other) = delete;
};

class InputLayout : public ContentBase
{
public:
	InputLayoutD3D11 data;

	InputLayout(std::string name, const UINT id) : ContentBase(name, id) { }
	~InputLayout() = default;
	InputLayout(const InputLayout &other) = delete;
	InputLayout &operator=(const InputLayout &other) = delete;
	InputLayout(InputLayout &&other) = delete;
	InputLayout &operator=(InputLayout &&other) = delete;
};

class Sound : public ContentBase
{
public:
	SoundSource data;

	Sound(std::string name, const UINT id) : ContentBase(name, id) { }
	~Sound() = default;
	Sound(const Sound& other) = delete;
	Sound& operator=(const Sound& other) = delete;
	Sound(Sound&& other) = delete;
	Sound& operator=(Sound&& other) = delete;
};

class HeightTexture : public ContentBase
{
public:
	HeightMap data;

	HeightTexture(std::string name, const UINT id) : ContentBase(name, id) { }
	~HeightTexture() = default;
	HeightTexture(const HeightTexture &other) = delete;
	HeightTexture &operator=(const HeightTexture &other) = delete;
	HeightTexture(HeightTexture &&other) = delete;
	HeightTexture &operator=(HeightTexture &&other) = delete;
};

struct CompiledData
{
	char *data = nullptr;
	size_t size = 0;

	CompiledData() = default;
	~CompiledData()
	{
		if (data)
		{
			delete[] data;
			data = nullptr;
		}
	}
};

static constexpr auto matPtrCmp = [](Material *a, Material *b) { return (*a) < (*b); };


/// Handles loading, storing and unloading of meshes, shaders, textures, texture maps, samplers and input layouts.
class Content
{
#pragma region General
private:
	std::vector<Material *> _materialVec;
	std::set<Material *, decltype(matPtrCmp)> _materialSet;

	bool _hasShutDown = false;

	template <typename C>
	[[nodiscard]] inline bool IsNameDuplicate(const std::string &name, const std::vector<C *> &contentVec, UINT *id);

public:
	Content();
	~Content();
	Content(const Content &other) = delete;
	Content &operator=(const Content &other) = delete;
	Content(Content &&other) = delete;
	Content &operator=(Content &&other) = delete;

	void Shutdown();

	const Material *GetOrAddMaterial(Material mat);
	[[nodiscard]] const Material *GetDefaultMaterial();
	[[nodiscard]] const Material *GetErrorMaterial();

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI(ID3D11Device *device);
#endif
#pragma endregion


#pragma region Mesh
private:
	std::vector<Mesh *> _meshes;

public:
	[[nodiscard]] CompiledData GetMeshData(const char *path) const;

	UINT AddMesh(ID3D11Device *device, const std::string &name, MeshData **meshData);
	UINT AddMesh(ID3D11Device *device, const std::string &name, const char *path);

	void GetMeshNames(std::vector<std::string> *names) const;
	[[nodiscard]] UINT GetMeshCount() const;

	[[nodiscard]] UINT GetMeshID(const std::string &name) const;
	[[nodiscard]] std::string GetMeshName(UINT id) const;

	[[nodiscard]] MeshD3D11 *GetMesh(const std::string &name) const;
	[[nodiscard]] MeshD3D11 *GetMesh(UINT id) const;
#pragma endregion

#pragma region Sound
private:
	std::vector<Sound *> _sounds;

public:
	[[nodiscard]] UINT GetSoundID(const std::string &name) const;
	[[nodiscard]] SoundSource *GetSound(const std::string &name) const;
	[[nodiscard]] SoundSource *GetSound(UINT id) const;
#pragma endregion

#pragma region Texture
private:
	std::vector<Texture *> _textures;
	std::vector<Cubemap *> _cubemaps;
	std::vector<HeightTexture *> _heightTextures;

public:
	UINT AddTexture(ID3D11Device *device, ID3D11DeviceContext *context,
		const std::string &name, const std::string &path, DXGI_FORMAT format,
		bool useMipmaps, int downsample = 0);
	UINT AddCubemap(ID3D11Device *device, ID3D11DeviceContext *context, 
		const std::string &name, const std::string &path, DXGI_FORMAT format,
		bool useMipmaps, int downsample = 0);
	UINT AddHeightMap(const std::string &name, const std::string &path);

	[[nodiscard]] UINT GetTextureCount() const;
	[[nodiscard]] UINT GetCubemapCount() const;

	void GetTextureNames(std::vector<std::string> *names) const;
	void GetCubemapNames(std::vector<std::string> *names) const;

	[[nodiscard]] bool HasTexture(const std::string &name) const;
	[[nodiscard]] bool HasCubemap(const std::string &name) const;

	[[nodiscard]] UINT GetTextureID(const std::string &name) const;
	[[nodiscard]] UINT GetCubemapID(const std::string &name) const;
	[[nodiscard]] UINT GetHeightMapID(const std::string &name) const;

	[[nodiscard]] UINT GetTextureIDByPath(const std::string &path) const;
	[[nodiscard]] UINT GetCubemapIDByPath(const std::string &path) const;

	[[nodiscard]] std::string GetTextureName(UINT id) const;
	[[nodiscard]] std::string GetCubemapName(UINT id) const;

	[[nodiscard]] ShaderResourceTextureD3D11 *GetTexture(UINT id) const;
	[[nodiscard]] ShaderResourceTextureD3D11 *GetTexture(const std::string &name) const;
	[[nodiscard]] ShaderResourceTextureD3D11 *GetCubemap(UINT id) const;
	[[nodiscard]] ShaderResourceTextureD3D11 *GetCubemap(const std::string &name) const;
	[[nodiscard]] HeightMap *GetHeightMap(const std::string &name) const;
	[[nodiscard]] HeightMap *GetHeightMap(UINT id) const;
#pragma endregion

#pragma region Shader
private:
	std::vector<Shader *> _shaders;

public:
	[[nodiscard]] CompiledData GetShaderData(const std::string &name, const char *path, ShaderType shaderType) const;

	UINT AddShader(ID3D11Device *device, const std::string &name, ShaderType shaderType, ID3DBlob *&shaderBlob);
	UINT AddShader(ID3D11Device *device, const std::string &name, ShaderType shaderType, const void *dataPtr, size_t dataSize);
	UINT AddShader(ID3D11Device *device, const std::string &name, ShaderType shaderType, const std::string &path);

	[[nodiscard]] ID3DBlob *CompileShader(ID3D11Device *device, const std::string &path, ShaderType shaderType) const;
	[[nodiscard]] bool RecompileShader(ID3D11Device *device, const std::string &name) const;

	void GetShaderNames(std::vector<std::string> *names) const;

	[[nodiscard]] UINT GetShaderID(const std::string &name) const;
	[[nodiscard]] std::string GetShaderName(UINT id) const;
	[[nodiscard]] ShaderD3D11 *GetShader(const std::string &name) const;
	[[nodiscard]] ShaderD3D11 *GetShader(UINT id) const;

	void GetShadersOfType(std::vector<UINT> &shaders, ShaderType type);
	static ShaderType GetShaderTypeFromName(const std::string &name);
#pragma endregion

#pragma region Input Layout
private:
	std::vector<InputLayout *> _inputLayouts;

public:
	UINT AddInputLayout(ID3D11Device *device, const std::string &name, const std::vector<Semantic> &semantics, const void *vsByteData, size_t vsByteSize);
	UINT AddInputLayout(ID3D11Device *device, const std::string &name, const std::vector<Semantic> &semantics, UINT vShaderID);

	[[nodiscard]] UINT GetInputLayoutID(const std::string &name) const;
	[[nodiscard]] InputLayoutD3D11 *GetInputLayout(const std::string &name) const;
	[[nodiscard]] InputLayoutD3D11 *GetInputLayout(UINT id) const;
#pragma endregion

#pragma region Sampler
private:
	std::vector<Sampler *> _samplers;

public:
	UINT AddSampler(ID3D11Device *device, const std::string &name, 
		D3D11_TEXTURE_ADDRESS_MODE adressMode, D3D11_FILTER filter, 
		D3D11_COMPARISON_FUNC comparisonFunc = D3D11_COMPARISON_NEVER);
	UINT AddSampler(ID3D11Device *device, const std::string &name, const D3D11_SAMPLER_DESC &samplerDesc);

	[[nodiscard]] UINT GetSamplerCount() const;
	void GetSamplerNames(std::vector<std::string> *names) const;

	[[nodiscard]] UINT GetSamplerID(const std::string &name) const;
	[[nodiscard]] std::string GetSamplerName(UINT id) const;

	[[nodiscard]] SamplerD3D11 *GetSampler(const std::string &name) const;
	[[nodiscard]] SamplerD3D11 *GetSampler(UINT id) const;
#pragma endregion

#pragma region Blend State
private:
	std::vector<BlendState *> _blendStates;

public:
	UINT AddBlendState(ID3D11Device *device, const std::string &name, const D3D11_BLEND_DESC &blendDesc);

	[[nodiscard]] UINT GetBlendStateCount() const;
	void GetBlendStateNames(std::vector<std::string> *names) const;

	[[nodiscard]] UINT GetBlendStateID(const std::string &name) const;
	[[nodiscard]] std::string GetBlendStateName(UINT id) const;

	[[nodiscard]] ID3D11BlendState *GetBlendState(const std::string &name) const;
	[[nodiscard]] ID3D11BlendState *GetBlendState(UINT id) const;
	[[nodiscard]] ComPtr<ID3D11BlendState> *GetBlendStateAddress(const std::string &name) const;
	[[nodiscard]] ComPtr<ID3D11BlendState> *GetBlendStateAddress(UINT id) const;
#pragma endregion

	TESTABLE()
};

template<typename C>
inline bool Content::IsNameDuplicate(const std::string &name, const std::vector<C *> &contentVec, UINT *id)
{
	static_assert(std::is_base_of<ContentBase, C>::value, "C must be derived from ContentBase");

	UINT size = (UINT)contentVec.size();

	for (UINT i = 0; i < size; i++)
	{
		if (contentVec[i]->name != name)
			continue;

		if (id) 
			*id = i;

		return true;
	}

	if (id) 
		*id = size;

	return false;
}
