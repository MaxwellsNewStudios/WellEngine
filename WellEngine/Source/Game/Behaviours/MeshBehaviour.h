#pragma once
#include "Behaviour.h"
#include "Content/Content.h"

struct TriplanarSettings
{
	dx::XMFLOAT2 texSize = {1, 1};
	float blendSharpness = 1.0f;
	bool flipWithNormal = true;
};

struct ShaderSettings
{
	std::unique_ptr<ConstantBufferD3D11> buffer = nullptr;
	std::unique_ptr<char[]> data = nullptr; // Pointer to struct containing settings
	UINT size = 0; // Size of the struct in bytes
	bool dirty = true;

	template<typename T>
	T *GetData()
	{
		if (!data || size < sizeof(T))
			return nullptr;

		return reinterpret_cast<T *>(data.get());
	}
};

class [[register_behaviour]] MeshBehaviour final : public Behaviour
{
private:
	struct DeserializedMesh
	{
		std::string
			mesh, texture, normal, 
			specular, glossiness, ambient, 
			reflect, occlusion, 
			sampler, blendState, vs, ps;

		DeserializedMesh() : 
			mesh(), texture(), normal(), 
			specular(), glossiness(), ambient(), 
			reflect(), occlusion(),
			sampler(), blendState(), vs(), ps() 
		{ }
	};
	std::unique_ptr<DeserializedMesh> _deserializedMesh = nullptr;

	UINT _meshID = CONTENT_NULL;
	UINT _blendStateID = CONTENT_NULL;
	const Material *_material = nullptr;

	bool _updatePosBuffer = true;
	bool _updateMatBuffer = false;
	bool _recalculateBounds = true;
	bool _isTransparent = false;
	bool _isOverlay = false;
	bool _castShadows = true;
	bool _shadowsOnly = false;
	
	dx::XMFLOAT4 _baseColor = {1,1,1,1};
	float _alphaCutoff = 0.0f;
	float _normalFactor = 1.0f;
	float _specularFactor = 1.0f;
	float _glossFactor = 1.0f;
	float _occlusionFactor = 0.85f;
	float _reflectivity = 0.1f;
	float _metallic = 0.0f;

	UINT _lastUsedLODIndex = 0;
	float _lastUsedLODDist = 0.0f;

	ConstantBufferD3D11
		_materialBuffer,
		_posBuffer;

	ShaderSettings _psSettings;

	dx::BoundingOrientedBox 
		_bounds,
		_transformedBounds;

	[[nodiscard]] bool ValidateMaterial(const Material **material);

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

	// Render runs every frame when objects are being queued for rendering.
	[[nodiscard]] bool Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo) override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

	// BindBuffers runs before drawcalls pertaining to the Entity are performed.
	[[nodiscard]] bool BindBuffers(ID3D11DeviceContext *context) override;

	// OnDirty runs when the Entity's transform is modified.
	void OnDirty() override;

public:
	MeshBehaviour() = default;
	MeshBehaviour(const dx::BoundingOrientedBox &bounds, bool isTransparent = false, bool castShadows = true, bool isOverlay = false) : 
		_bounds(bounds), _isTransparent(isTransparent), _castShadows(castShadows), _isOverlay(isOverlay) {}
	MeshBehaviour(const dx::BoundingOrientedBox &bounds, UINT meshID, const Material *material, bool isTransparent = false, bool castShadows = true, bool isOverlay = false) : 
		_bounds(bounds), _meshID(meshID), _material(material), _isTransparent(isTransparent), _castShadows(castShadows), _isOverlay(isOverlay) {}
	~MeshBehaviour() = default;

	void StoreBounds(dx::BoundingOrientedBox &meshBounds);

	void SetMeshID(UINT meshID, bool updateBounds = false);
	void SetBlendStateID(UINT blendStateID);
	[[nodiscard]] bool SetMaterial(const Material *material);
	void SetTransparent(bool state);
	void SetOverlay(bool state);
	void SetCastShadows(bool state);
	void SetShadowsOnly(bool state);
	void SetAlphaCutoff(float value);
	void SetColor(const dx::XMFLOAT4 &color);
	void SetBounds(dx::BoundingOrientedBox &newBounds);

	[[nodiscard]] UINT GetMeshID() const;
	[[nodiscard]] UINT GetBlendStateID() const;
	[[nodiscard]] const Material *GetMaterial() const;

	void SetLastUsedLOD(UINT lodIndex, float normalizedDist);
	[[nodiscard]] UINT GetLastUsedLODIndex() const;
	[[nodiscard]] float GetLastUsedLODDist() const;
	[[nodiscard]] UINT GetLODCount() const;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	void PostDeserialize() override;


	void CopyPSSettings(const ShaderSettings &newSettings)
	{
		if (!_psSettings.data || _psSettings.size != newSettings.size)
		{
			_psSettings = ShaderSettings();
			_psSettings.size = newSettings.size;
			_psSettings.data = std::make_unique<char[]>(newSettings.size);
		}

		_psSettings.dirty = true;
		memcpy(_psSettings.data.get(), newSettings.data.get(), newSettings.size);
	}

	template<typename T>
	void SetPSSettings(const T &newData)
	{
		if (!_psSettings.data || _psSettings.size != sizeof(T))
		{
			_psSettings = ShaderSettings();
			_psSettings.size = sizeof(T);
			_psSettings.data = std::make_unique<char[]>(_psSettings.size);
		}

		_psSettings.dirty = true;

		T *dataPtr = _psSettings.GetData<T>();
		*dataPtr = newData;
	}
};
