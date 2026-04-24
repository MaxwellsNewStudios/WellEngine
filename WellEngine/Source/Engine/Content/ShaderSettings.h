#pragma once

#include <memory>
#include <format>
#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Content/Content.h"
#ifdef USE_IMGUI
#include "Dependencies/ImGui/imgui.h"
#include "Engine/UI/ImGuiUtils.h"
#endif // USE_IMGUI


namespace WellEngine::Shaders
{
	constexpr UINT SETTINGS_REGISTER_PS = 4;

	// Generic shader setting container
	struct SettingsContainer
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

#pragma region PS Settings Structs

	struct TriplanarSettings
	{
		dx::XMFLOAT2 texSize = { 1, 1 };
		float blendSharpness = 1.0f;
		bool flipWithNormal = true;


		static const std::string_view GetName() { return "PS_TriPlanar"; }

#ifdef USE_IMGUI
		bool RenderUI()
		{
			ImGui::SeparatorText(std::format("{} Settings", GetName()).c_str());
			bool changed = false;

			ImGui::Text("Texture Scale:"); ImGui::SameLine();
			if (ImGui::DragFloat2("##TextureScale", &texSize.x, 0.01f))
				changed = true;
			ImGuiUtils::LockMouseOnActive();

			ImGui::Text("Blend Sharpness:"); ImGui::SameLine();
			if (ImGui::DragFloat("##BlendSharpness", &blendSharpness, 0.01f))
			{
				blendSharpness = MAX(blendSharpness, 0.001f);
				changed = true;
			}
			ImGuiUtils::LockMouseOnActive();

			if (ImGui::Checkbox("Correct Normals", &flipWithNormal))
				changed = true;

			return changed;
		}
#endif // USE_IMGUI
	};

	struct CelSettings
	{
		float levels = 4.0f;

		float _padding[3] = {};


		static const std::string_view GetName() { return "PS_Cel"; }

#ifdef USE_IMGUI
		bool RenderUI()
		{
			ImGui::SeparatorText(std::format("{} Settings", GetName()).c_str());
			bool changed = false;

			ImGui::Text("Levels:"); ImGui::SameLine();
			if (ImGui::DragFloat("##Levels", &levels, 0.05f, 1.0f))
			{
				levels = MAX(levels, 1.0f);
				changed = true;
			}
			ImGuiUtils::LockMouseOnActive();

			return changed;
		}
#endif // USE_IMGUI
	};

	// NOTE: Define setting structs here...

#pragma endregion // PS Settings Structs


#pragma region Shader Name to Settings Mapping

#pragma region Internal
	// Registration helpers
	struct GenericSettings
	{
		std::string_view shaderName;
		std::function<void(SettingsContainer &, void *)> constructFunc;
		std::function<bool(SettingsContainer &)> drawUIFunc;

		GenericSettings(std::string_view shaderName,
			std::function<void(SettingsContainer &, void *)> constructFunc,
			std::function<bool(SettingsContainer &)> drawUIFunc) 
			: shaderName(shaderName), constructFunc(constructFunc), drawUIFunc(drawUIFunc)
		{ }
	};

#define SHADER_CONSTRUCT_FUNC(T, settings, data)					\
	if (!settings.data || settings.size != sizeof(T)) {				\
		settings = SettingsContainer();								\
		settings.size = sizeof(T);									\
		settings.data = std::make_unique<char[]>(settings.size);	\
	}																\
	settings.dirty = true;											\
	T *dataPtr = settings.GetData<T>();								\
	if (data) *dataPtr = *reinterpret_cast<const T *>(data);		\
	else *dataPtr = T();

#ifdef USE_IMGUI
#define SHADER_DRAWUI_FUNC(T, settings)		\
	T *dataPtr = settings.GetData<T>();		\
	if (dataPtr)							\
		return dataPtr->RenderUI();			\
	return false;
#else // USE_IMGUI
#define SHADER_DRAWUI_FUNC(T, settings) return false;
#endif // USE_IMGUI

#define REGISTER_SHADER_SETTINGS(T) { T::GetName(), GenericSettings(T::GetName(),					\
	[](SettingsContainer &settings, const void *data) {SHADER_CONSTRUCT_FUNC(T, settings, data)},	\
	[](SettingsContainer &settings) -> bool {SHADER_DRAWUI_FUNC(T, settings)}) }
#pragma endregion // Internal

	// Map of shader name to settings construction and UI drawing functions
	static const std::unordered_map<std::string_view, GenericSettings> SettingsMap = {
		REGISTER_SHADER_SETTINGS(TriplanarSettings),
		REGISTER_SHADER_SETTINGS(CelSettings),
		// NOTE: Register setting structs here...
	};

	static bool HasShaderSettings(const std::string_view &name)
	{
		return SettingsMap.find(name) != SettingsMap.end();
	}

	static const GenericSettings *GetShaderSettings(const std::string_view &name)
	{
		auto it = SettingsMap.find(name);
		if (it != SettingsMap.end())
			return &it->second;
		return nullptr;
	}

#pragma endregion // Shader Name to Settings Mapping
}