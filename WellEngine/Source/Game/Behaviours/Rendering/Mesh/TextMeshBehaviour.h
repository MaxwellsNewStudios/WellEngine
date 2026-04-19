#pragma once

#include "Game/Behaviour.h"
#include "Engine/D3D/MeshD3D11.h"
#include "MeshBehaviour.h"

namespace WellEngine
{
	class [[register_behaviour]] TextMeshBehaviour : public Behaviour
	{
	private:
		Ref<MeshBehaviour> _meshBehaviour = nullptr;
		UINT _fontAtlasID = CONTENT_NULL;
		UINT _meshID = CONTENT_NULL;
		std::string _text = "";

		dx::XMFLOAT4 _color = { 1, 1, 1, 1 };
		float _thickness = 0.5f;

		static std::vector<UINT> &GetUnusedMeshIDs()
		{
			static std::vector<UINT> unusedMeshIDs;
			return unusedMeshIDs;
		}

	protected:
		[[nodiscard]] bool Start() override;

	#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
	#endif

		// Serializes the behaviour to a string.
		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		// Deserializes the behaviour from a string.
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

		void PostDeserialize() override;

		void RecreateMesh();

	public:
		TextMeshBehaviour() = default;
		~TextMeshBehaviour();

		const std::string &GetText() const;
		void SetText(std::string_view text, bool skipRebuild = false);

		UINT GetFontAtlasID() const;
		void SetFontAtlasID(UINT id, bool skipRebuild = false);

		UINT GetMeshID() const;

		MeshBehaviour *GetMeshBehaviour() const;
	};
}
