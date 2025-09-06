#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightData.hlsli"
#else
#include "Common.hlsli"
#include "LightData.hlsli"
#endif

void CalculateLighting(
	float3 pos, float3 viewDir,																	// View
	float3 geometryNormal, float3 surfaceNormal, float3 specularColor, float specularExponent,	// Surface
	out float3 totalDiffuseLight, out float3 totalSpecularLight,								// Output
	in bool fallbackReflectionModel = false)
{
	totalDiffuseLight = ambient_light.xyz;
	totalSpecularLight = float3(0.0, 0.0, 0.0);
	
	// Calculate light tile position
	const float4 screenPosClip = mul(float4(pos, 1.0f), view_proj_matrix);
	const float3 screenPosNDC = screenPosClip.xyz / screenPosClip.w;
	const float2 screenUV = float2((screenPosNDC.x * 0.5f) + 0.5f, (screenPosNDC.y * 0.5f) + 0.5f);
	
	uint lightTiles, lightTileDim, _;
	LightTileBuffer.GetDimensions(lightTiles, _);
	lightTileDim = sqrt(lightTiles);
	
	uint2 lightTileCoord = uint2(screenUV * float(lightTileDim));
	int lightTileIndex = lightTileCoord.x + (lightTileCoord.y * lightTileDim);
	
	LightTile lightTile = LightTileBuffer[lightTileIndex];
	
	
	
	// Get spotlight data
	uint spotlightCount = lightTile.spotlightCount;
	uint spotWidth, spotHeight;
	SpotShadowMaps.GetDimensions(0, spotWidth, spotHeight, _, _);
	
	const float
		spotDX = 1.0 / float(spotWidth),
		spotDY = 1.0 / float(spotHeight);
	
	// Per-spotlight calculations
	for (uint spotlight_i = 0; spotlight_i < spotlightCount; spotlight_i++)
	{
		// Prerequisite variables
		uint currentSpotlight = lightTile.spotlights[spotlight_i];
		const SpotLight light = SpotLights[currentSpotlight];
	
		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		float3 toLightScaled = toLight * light.falloff;
		toLightScaled *= toLightScaled;
		
		const float projectedDist = dot(light.direction, -toLight);
		float inverseLightDistSqr = light.orthographic > 0 
			? // If orthographic
			(projectedDist > 0.0 ? 1.0 : 0.0) * (1.0 / (1.0 + pow(projectedDist, 2.0))) 
			: // Else perspective
			1.0 / (1.0 + (toLightScaled.x + toLightScaled.y + toLightScaled.z));
		
		// Remap light intensity and skip light if it has no impact
		CutoffLight(light.color, inverseLightDistSqr);
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		const float offsetAngle = saturate(1.0 - (
			light.orthographic > 0 ?
			length(cross(light.direction, toLight)) :
			acos(dot(-toLightDir, light.direction))
		) / (light.angle * 0.5));


		float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, surfaceNormal, light.color, specularExponent, diffuseLightCol, specularLightCol, fallbackReflectionModel);
		specularLightCol *= specularColor;


		// Calculate shadow projection
		const float4 fragPosLightClip = mul(float4(pos + (SHADOW_NORMAL_OFFSET * geometryNormal), 1.0),
		light.vp_matrix);
		const float3 fragPosLightNDC = fragPosLightClip.xyz / fragPosLightClip.w;
		
		const bool isInsideFrustum = (
			fragPosLightNDC.x > -1.0 && fragPosLightNDC.x < 1.0 &&
			fragPosLightNDC.y > -1.0 && fragPosLightNDC.y < 1.0
		);
		
		const float3 spotUV = float3(mad(fragPosLightNDC.x, 0.5, 0.5), mad(fragPosLightNDC.y, -0.5, 0.5), currentSpotlight);
		float shadowFactor = SpotShadowMaps.SampleCmpLevelZero(
			ShadowSampler,
			spotUV,
			fragPosLightNDC.z + SHADOW_DEPTH_EPSILON
		);
		const float shadow = isInsideFrustum * saturate(offsetAngle * shadowFactor);
		
		// Apply lighting
		totalDiffuseLight += diffuseLightCol * shadow * inverseLightDistSqr;
		totalSpecularLight += specularLightCol * shadow * inverseLightDistSqr;
	}
	
	
	
	// Get simple spotlight data
	uint simpleSpotlightCount = lightTile.simpleSpotlightCount;
	
	// Per-simple spotlight calculations
	for (uint simpleSpotlight_i = 0; simpleSpotlight_i < simpleSpotlightCount; simpleSpotlight_i++)
	{
		// Prerequisite variables
		uint currentSimpleSpotlight = lightTile.simpleSpotlights[simpleSpotlight_i];
		const SimpleSpotLight light = SimpleSpotLights[currentSimpleSpotlight];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		float3 toLightScaled = toLight * light.falloff;
		toLightScaled *= toLightScaled;
		
		const float projectedDist = dot(light.direction, -toLight);
		float inverseLightDistSqr = light.orthographic > 0 
			? // If orthographic
			(projectedDist > 0.0 ? 1.0 : 0.0) * (1.0 / (1.0 + pow(projectedDist, 2.0))) 
			: // Else perspective
			1.0 / (1.0 + (toLightScaled.x + toLightScaled.y + toLightScaled.z));
		
		// Remap light intensity and skip light if it has no impact
		CutoffLight(light.color, inverseLightDistSqr);
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		const float offsetAngle = saturate(1.0 - (
			light.orthographic > 0 ?
			length(cross(light.direction, toLight)) :
			acos(dot(-toLightDir, light.direction))
		) / (light.angle * 0.5));


		float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, surfaceNormal, light.color, specularExponent, diffuseLightCol, specularLightCol, fallbackReflectionModel);
		specularLightCol *= specularColor;
		
		// Apply lighting
		totalDiffuseLight += diffuseLightCol * saturate(offsetAngle) * inverseLightDistSqr;
		totalSpecularLight += specularLightCol * saturate(offsetAngle) * inverseLightDistSqr;
	}
	
	
	
	// Get pointlight data
	uint pointlightCount = lightTile.pointlightCount;
	uint pointWidth, pointHeight;
	PointShadowMaps.GetDimensions(0, pointWidth, pointHeight, _, _);

	const float
		pointDX = 1.0 / (float)pointWidth,
		pointDY = 1.0 / (float)pointHeight;
	
	// Per-pointlight calculations
	for (uint pointlight_i = 0; pointlight_i < pointlightCount; pointlight_i++)
	{
		// Prerequisite variables
		uint currentPointlight = lightTile.pointlights[pointlight_i];
		const PointLight light = PointLights[currentPointlight];
		
		const float3 
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		float3 toLightScaled = toLight * light.falloff;
		toLightScaled *= toLightScaled;
		
		float inverseLightDistSqr = 1.0 / (1.0 + (toLightScaled.x + toLightScaled.y + toLightScaled.z));
		
		// Remap light intensity and skip light if it has no impact
		CutoffLight(light.color, inverseLightDistSqr);
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, surfaceNormal, light.color, specularExponent, diffuseLightCol, specularLightCol, fallbackReflectionModel);
		specularLightCol *= specularColor;
		
		// Calculate shadow projection
		float3 normalOffset = SHADOWCUBE_NORMAL_OFFSET * length(toLight) * geometryNormal;
		float4 lightToPosDir = float4(toLight - normalOffset, currentPointlight);
		float lightDist = max(min(length(lightToPosDir.xyz), length(toLight)) - SHADOWCUBE_DEPTH_EPSILON, 0.0);
		lightToPosDir.xyz = normalize(lightToPosDir.xyz);
		lightToPosDir.x *= -1.0;
		
		float shadowFactor = PointShadowMaps.SampleCmpLevelZero(
			ShadowCubeSampler,
			lightToPosDir,
			//1.0 - (lightDist / light.farZ)
			1.0 - ((lightDist - light.nearZ) / (light.farZ - light.nearZ))
		);
		const float shadow = saturate(shadowFactor);
		
		// Apply lighting
		totalDiffuseLight += diffuseLightCol * shadow * inverseLightDistSqr;
		totalSpecularLight += specularLightCol * shadow * inverseLightDistSqr;
	}
	
	
	
	// Get simple pointlight data
	uint simplePointlightCount = lightTile.simplePointlightCount;
	
	// Per-simple pointlight calculations
	for (uint simplePointlight_i = 0; simplePointlight_i < simplePointlightCount; simplePointlight_i++)
	{
		// Prerequisite variables
		uint currentSimplePointlight = lightTile.simplePointlights[simplePointlight_i];
		const SimplePointLight light = SimplePointLights[currentSimplePointlight];
		
		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		float3 toLightScaled = toLight * light.falloff;
		toLightScaled *= toLightScaled;
		
		float inverseLightDistSqr = 1.0 / (1.0 + (toLightScaled.x + toLightScaled.y + toLightScaled.z));
		
		// Remap light intensity and skip light if it has no impact
		CutoffLight(light.color, inverseLightDistSqr);
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, surfaceNormal, light.color, specularExponent, diffuseLightCol, specularLightCol, fallbackReflectionModel);
		specularLightCol *= specularColor;
				
		// Apply lighting
		totalDiffuseLight += diffuseLightCol * inverseLightDistSqr;
		totalSpecularLight += specularLightCol * inverseLightDistSqr;
	}
}