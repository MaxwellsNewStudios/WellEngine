#pragma once

#include "Game/Behaviour.h"
#include "Engine/Rendering/Graphics/GraphicsTypes.h"
#include "Engine/Content/Content.h"
#include "Engine/Content/ShaderSettings.h"

namespace WellEngine
{
	class [[register_behaviour]] B_Mesh final : public Behaviour, public IRefTarget<B_Mesh>
	{
	public:
		const std::string & GetName() const override { return "Mesh"; }

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
		FaceCullingType _cullMode = FaceCullingType::BACK;
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
		float _ambientFactor = 1.0f;
		float _occlusionFactor = 0.85f;
		float _reflectivity = 0.1f;
		float _metallic = 0.0f;

		UINT _lastUsedLODIndex = 0;
		float _lastUsedLODDist = 0.0f;

		ConstantBufferD3D11
			_materialBuffer,
			_posBuffer;

		Shaders::SettingsContainer _psSettings;

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
		[[nodiscard]] bool Render(RenderQueuer &queuer, const RendererInfo &rendererInfo) override;

	#ifdef USE_IMGUI
		// RenderUI runs every frame during ImGui rendering if the entity is selected.
		[[nodiscard]] bool RenderUI() override;
	#endif

		// BindBuffers runs before drawcalls pertaining to the Entity are performed.
		[[nodiscard]] bool BindBuffers(ID3D11DeviceContext *context) override;

		// OnDirty runs when the Entity's transform is modified.
		void OnDirty() override;

	public:
		B_Mesh() = default;
		B_Mesh(const dx::BoundingOrientedBox &bounds, bool isTransparent = false, bool castShadows = true, bool isOverlay = false) : 
			_bounds(bounds), _isTransparent(isTransparent), _castShadows(castShadows), _isOverlay(isOverlay) {}
		B_Mesh(const dx::BoundingOrientedBox &bounds, UINT meshID, const Material *material, bool isTransparent = false, bool castShadows = true, bool isOverlay = false) : 
			_bounds(bounds), _meshID(meshID), _material(material), _isTransparent(isTransparent), _castShadows(castShadows), _isOverlay(isOverlay) {}
		~B_Mesh() = default;

		void StoreBounds(dx::BoundingOrientedBox &meshBounds);

		void SetMeshID(UINT meshID, bool updateBounds = false);
		void SetBlendStateID(UINT blendStateID);
		[[nodiscard]] bool SetMaterial(const Material *material, float discardSettings = true);
		void SetTransparent(bool state);
		void SetOverlay(bool state);
		void SetCastShadows(bool state);
		void SetShadowsOnly(bool state);
		void SetAlphaCutoff(float value);
		void SetColor(const dx::XMFLOAT4 &color);
		void SetBounds(dx::BoundingOrientedBox &newBounds);
		void SetCullMode(FaceCullingType cullMode);

		[[nodiscard]] UINT GetMeshID() const;
		[[nodiscard]] UINT GetBlendStateID() const;
		[[nodiscard]] const Material *GetMaterial() const;
		[[nodiscard]] const dx::XMFLOAT4 &GetColor() const;
		[[nodiscard]] float GetAlphaCutoff() const;
		[[nodiscard]] FaceCullingType GetCullMode() const;

		void SetLastUsedLOD(UINT lodIndex, float normalizedDist);
		[[nodiscard]] UINT GetLastUsedLODIndex() const;
		[[nodiscard]] float GetLastUsedLODDist() const;
		[[nodiscard]] UINT GetLODCount() const;

		// Serializes the behaviour to a string.
		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		// Deserializes the behaviour from a string.
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

		[[nodiscard]] bool PostDeserialize() override;


		void CopyPSSettings(const Shaders::SettingsContainer &newSettings)
		{
			if (!_psSettings.data || _psSettings.size != newSettings.size)
			{
				_psSettings = Shaders::SettingsContainer();
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
				_psSettings = Shaders::SettingsContainer();
				_psSettings.size = sizeof(T);
				_psSettings.data = std::make_unique<char[]>(_psSettings.size);
			}

			_psSettings.dirty = true;

			T *dataPtr = _psSettings.GetData<T>();
			*dataPtr = newData;
		}
	};
}
