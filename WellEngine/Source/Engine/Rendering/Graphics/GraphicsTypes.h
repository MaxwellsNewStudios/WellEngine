#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Content/Content.h"
#ifdef USE_IMGUI
#include "Dependencies/ImGui/imgui.h"
#endif

namespace WellEngine
{
	namespace we = WellEngine;
	namespace dx = DirectX;

	class B_Camera;


	enum class FaceCullingType
	{
		NONE	= 0,
		FRONT	= 1,
		BACK	= 2
	};

	enum class RenderType
	{
		DEFAULT,
		POSITION,
		NORMAL,
		AMBIENT,
		DIFFUSE,
		DEPTH,
		SHADOW,
		REFLECTION,
		REFLECTIVITY,
		SPECULAR,
		SPECULAR_STRENGTH,
		UV_COORDS,
		OCCLUSION,
		TRANSPARENCY,
		LIGHT_TILES,
		OVERDRAW,

		COUNT
	};

	enum LightType
	{
		SPOTLIGHT,
		POINTLIGHT,
		SIMPLE_SPOTLIGHT,
		SIMPLE_POINTLIGHT,
	};


	struct LightTile
	{
		UINT spotlights[MAX_LIGHTS]{};
		UINT spotlightCount = 0;

		UINT pointlights[MAX_LIGHTS]{};
		UINT pointlightCount = 0;

		UINT simpleSpotlights[MAX_LIGHTS]{};
		UINT simpleSpotlightCount = 0;

		UINT simplePointlights[MAX_LIGHTS]{};
		UINT simplePointlightCount = 0;
	};


	struct GeneralDataBuffer
	{
		float time = 0.0f;
		float deltaTime = 0.0f;
		int randInt = 0;
		float randNorm = 0.0f;

		// Distance based fade-out
		dx::XMFLOAT4 fadeoutColor = { 0, 0, 0, 1 };
		float fadeoutDepthBegin = 0.5f;
		float fadeoutExponent = 2.0f;

		float _padding[2];
	};

	struct FogSettingsBuffer
	{
		float	thickness = 0.2f;
		float	sampleBias = 1.5f;
		int		maxSteps = 64;
		float	depthFadeBegin = 0.5f;
		float	depthFadeEnd = 1.0f;
		float	depthFadeExp = 1.0f;

		float	_padding[2];
	};

	struct EmissionSettingsBuffer
	{
		float strength = 1.0f;
		float exponent = 0.5f;
		float threshold = 1.0f;
		float whiteBias = 0.25f;
	};

	struct DistortionSettingsBuffer
	{
		dx::XMFLOAT3 distortionOrigin = { 0.0f, 0.0f, 0.0f };
		float distortionStrength = 0.0f;
	};

	struct DepthOfFieldSettingsBuffer
	{
		float focalPlane = 0.25f;
		float aperture = 15;
		float imageDistance = 1;

		float _padding[1];
	};

	struct OutlineSettingsBuffer
	{
		dx::XMFLOAT4 color = { 0.2f, 0.7f, 1.0f, 1.0f };
		float strength = 2.5f;
		float exponent = 1.0f;
		float smoothing = 0.8f;

		float _padding[1];
	};

	#ifdef USE_IMGUI
	struct NotificationMessage
	{
		enum class SeverityColor
		{
			White = 0, // Default
			Green,	   // Info
			Yellow,	   // Notice
			Orange,	   // Warning
			Red,	   // Problem

			// Misc
			Blue,
			Magenta,
			Cyan,
			Black,
			Gray,

			// Special
			Rainbow,
		};

		std::string message, id = "";
		SeverityColor severity;
		float duration;
		float fontSize;
		float blinkFrequency = -1.0f;
		float bgAlpha = -1.0f;

		NotificationMessage(std::string message, SeverityColor severity = SeverityColor::White, float duration = -1.0f, float fontSize = 16.0f, float blinkFrequency = -1.0f, float bgAlpha = -1.0f)
			: message(std::move(message)), severity(severity), duration(duration), fontSize(fontSize), blinkFrequency(blinkFrequency), bgAlpha(bgAlpha) { }

		NotificationMessage(std::string message, int severity = 0, float duration = -1.0f, float fontSize = 16.0f, float blinkFrequency = -1.0f, float bgAlpha = -1.0f)
			: message(std::move(message)), severity((SeverityColor)severity), duration(duration), fontSize(fontSize), blinkFrequency(blinkFrequency), bgAlpha(bgAlpha) { }
	};
	#endif
}


