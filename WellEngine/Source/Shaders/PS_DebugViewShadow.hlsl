#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightData.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightData.hlsli"
#endif

static const float EXP_CURVE = 0.5;

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 world_position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float3 diffuseCol = Texture.Sample(Sampler, input.tex_coord).xyz;
	float compressedSurfaceColor = (diffuseCol.r + diffuseCol.g + diffuseCol.b) / 3.0;
	
	float fragmentSpotShadows = 0.0;
	float fragmentPointShadows = 0.0;
	
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 normal = input.normal;
	
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
		
		
		// Calculate shadow projection
		const float4 fragPosLightClip = mul(float4(pos + normal * SHADOW_NORMAL_OFFSET, 1.0), light.vp_matrix);
		const float3 fragPosLightNDC = fragPosLightClip.xyz / fragPosLightClip.w;
		
		const bool isInsideFrustum = (
			fragPosLightNDC.x > -1.0 && fragPosLightNDC.x < 1.0 &&
			fragPosLightNDC.y > -1.0 && fragPosLightNDC.y < 1.0 &&
			fragPosLightNDC.z > 0.0 && fragPosLightNDC.z < 1.0
		);
		
		/*const float3 spotUV = float3(mad(fragPosLightNDC.x, 0.5, 0.5), mad(fragPosLightNDC.y, -0.5, 0.5), currentSpotlight);
		float spotDepth = SpotShadowMaps.SampleLevel(ShadowSampler, spotUV, 0.0).x;
		const float spotResult = spotDepth < fragPosLightNDC.z ? 1.0 : 0.0;
		const float shadow = saturate(spotResult);*/
		
		const float3 spotUV = float3(mad(fragPosLightNDC.x, 0.5, 0.5), mad(fragPosLightNDC.y, -0.5, 0.5), currentSpotlight);
		float shadowFactor = SpotShadowMaps.SampleCmpLevelZero(
			ShadowSampler,
			spotUV,
			fragPosLightNDC.z + SHADOW_DEPTH_EPSILON
		);
		const float shadow = saturate(shadowFactor);
		
		// Apply lighting
		fragmentSpotShadows += isInsideFrustum * shadow;
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
		
				
		// Calculate shadow projection
		float3 normalOffset = SHADOWCUBE_NORMAL_OFFSET * length(toLight) * input.normal;
		float4 lightToPosDir = float4(toLight - normalOffset, currentPointlight);
		float lightDist = max(min(length(lightToPosDir.xyz), length(toLight)) - SHADOWCUBE_DEPTH_EPSILON, 0.0);
		lightToPosDir.xyz = normalize(lightToPosDir.xyz);
		lightToPosDir.x *= -1.0;
		
		float shadowFactor = PointShadowMaps.SampleCmpLevelZero(
			ShadowCubeSampler,
			lightToPosDir,
			1.0 - ((lightDist - light.nearZ) / (light.farZ - light.nearZ))
		);
		const float shadow = saturate(shadowFactor);
		
		// Apply lighting
		fragmentPointShadows += shadow;
	}
	
	float2 lightRemaining = 0.0.rr;
	if (fragmentSpotShadows > 0.0)
		lightRemaining.r = 1.0 - exp(-fragmentSpotShadows * EXP_CURVE);
	if (fragmentPointShadows > 0.0)
		lightRemaining.g = 1.0 - exp(-fragmentPointShadows * EXP_CURVE);
	
	return float4(compressedSurfaceColor, lightRemaining, 1.0);
}