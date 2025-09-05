#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/LightData.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightData.hlsli"
#endif


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
    const float3 pos = input.world_position.xyz;
	
    const float3 bitangent = normalize(cross(input.normal, input.tangent));
    const float3 norm = (sampleNormal > 0)
		? normalize(mul(NormalMap.Sample(Sampler, input.tex_coord).xyz * 2.0f - float3(1.0, 1.0, 1.0), float3x3(input.tangent, bitangent, input.normal)))
        : normalize(input.normal);
    const float3 viewDir = normalize(cam_position.xyz - pos);

    const float3 specularCol = (sampleSpecular > 0)
		? SpecularMap.Sample(Sampler, input.tex_coord).xyz
		: float3(0.0, 0.0, 0.0);
        
    float3 totalDiffuseLight = float3(0.0, 0.0, 0.0);
    float3 totalSpecularLight = float3(0.0, 0.0, 0.0);
    uint spotlightCount, spotWidth, spotHeight, _u;
    SpotLights.GetDimensions(spotlightCount, _u);
    SpotShadowMaps.GetDimensions(0, spotWidth, spotHeight, _u, _u);

    const float
		spotDX = 1.0 / (float) spotWidth,
		spotDY = 1.0 / (float) spotHeight;

	// Per-spotlight calculations
    for (uint spotlight_i = 0; spotlight_i < spotlightCount; spotlight_i++)
    {
		// Prerequisite variables
        const SpotLight light = SpotLights[spotlight_i];

        const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
        const float
			projectedDist = dot(light.direction, -toLight),
			inverseLightDistSqr = light.orthographic > 0 ?
			(projectedDist > 0.0f ? 1.0f : 0.0f) * (1.0f / (1.0f + pow(projectedDist, 2))) :
			1.0f / (1.0f + (pow(toLight.x * light.falloff, 2) + pow(toLight.y * light.falloff, 2) + pow(toLight.z * light.falloff, 2)));

        const float offsetAngle = saturate(1.0f - (
			light.orthographic > 0 ?
			length(cross(light.direction, toLight)) :
			acos(dot(-toLightDir, light.direction))
			) / (light.angle * 0.5f));


        float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, norm, light.color.xyz, specularExponent, diffuseLightCol, specularLightCol);
        specularLightCol *= specularCol;


		// Calculate shadow projection
		const float4 fragPosLightClip = mul(float4(pos + SHADOW_NORMAL_OFFSET * norm, 1.0f), light.vp_matrix);
        const float3 fragPosLightNDC = fragPosLightClip.xyz / fragPosLightClip.w;
		
        const bool isInsideFrustum = (
			fragPosLightNDC.x > -1.0f && fragPosLightNDC.x < 1.0f &&
			fragPosLightNDC.y > -1.0f && fragPosLightNDC.y < 1.0f
		);
		
		const float3 spotUV = float3(mad(fragPosLightNDC.x, 0.5, 0.5), mad(fragPosLightNDC.y, -0.5, 0.5), spotlight_i);
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
	
	
	uint simpleSpotlightCount;
	SimpleSpotLights.GetDimensions(simpleSpotlightCount, _u);

	// Per-simple spotlight calculations
	for (uint simpleSpotlight_i = 0; simpleSpotlight_i < simpleSpotlightCount; simpleSpotlight_i++)
	{
		// Prerequisite variables
		const SimpleSpotLight light = SimpleSpotLights[simpleSpotlight_i];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		const float
			projectedDist = dot(light.direction, -toLight),
			inverseLightDistSqr = light.orthographic > 0 ?
			(projectedDist > 0.0f ? 1.0f : 0.0f) * (1.0f / (1.0f + pow(projectedDist, 2))) :
			1.0f / (1.0f + (pow(toLight.x * light.falloff, 2) + pow(toLight.y * light.falloff, 2) + pow(toLight.z * light.falloff, 2)));

		const float offsetAngle = saturate(1.0f - (
			light.orthographic > 0 ?
			length(cross(light.direction, toLight)) :
			acos(dot(-toLightDir, light.direction))
			) / (light.angle * 0.5f));


		float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, norm, light.color.xyz, specularExponent, diffuseLightCol, specularLightCol);
		specularLightCol *= specularCol;
		
		// Apply lighting
		totalDiffuseLight += diffuseLightCol * saturate(offsetAngle) * inverseLightDistSqr;
		totalSpecularLight += specularLightCol * saturate(offsetAngle) * inverseLightDistSqr;
	}
	
	
    uint pointlightCount, pointWidth, pointHeight;
    PointLights.GetDimensions(pointlightCount, _u);
    PointShadowMaps.GetDimensions(0, pointWidth, pointHeight, _u, _u);

    const float
		pointDX = 1.0f / (float) pointWidth,
		pointDY = 1.0f / (float) pointHeight;

	// Per-pointlight calculations
    for (uint pointlight_i = 0; pointlight_i < pointlightCount; pointlight_i++)
    {
		// Prerequisite variables
        const PointLight light = PointLights[pointlight_i];

        const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
        const float inverseLightDistSqr = 1.0f / (1.0f + (
			pow(toLight.x * light.falloff, 2.0) +
			pow(toLight.y * light.falloff, 2.0) +
			pow(toLight.z * light.falloff, 2.0)
		));
		

        float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, norm, light.color.xyz, specularExponent, diffuseLightCol, specularLightCol);
        specularLightCol *= specularCol;

		
		// Calculate shadow projection
		float3 normalOffset = SHADOWCUBE_NORMAL_OFFSET * length(toLight) * input.normal;
		float4 lightToPosDir = float4(toLight - normalOffset, pointlight_i);
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
        totalDiffuseLight += diffuseLightCol * shadow * inverseLightDistSqr;
        totalSpecularLight += specularLightCol * shadow * inverseLightDistSqr;
    }
	
	
	uint simplePointlightCount;
	SimplePointLights.GetDimensions(simplePointlightCount, _u);

	// Per-simple pointlight calculations
	for (uint simplePointlight_i = 0; simplePointlight_i < simplePointlightCount; simplePointlight_i++)
	{
		// Prerequisite variables
		const SimplePointLight light = SimplePointLights[simplePointlight_i];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		const float inverseLightDistSqr = 1.0f / (1.0f + (
			pow(toLight.x * light.falloff, 2) +
			pow(toLight.y * light.falloff, 2) +
			pow(toLight.z * light.falloff, 2)
		));
		
		float3 diffuseLightCol, specularLightCol;
		BlinnPhong(toLightDir, viewDir, norm, light.color.xyz, specularExponent, diffuseLightCol, specularLightCol);
		specularLightCol *= specularCol;
		
		// Apply lighting
		totalDiffuseLight += diffuseLightCol * inverseLightDistSqr;
		totalSpecularLight += specularLightCol * inverseLightDistSqr;
	}
	
	
    const float3 result = ACESFilm(totalDiffuseLight + totalSpecularLight);
	return float4(result, 1.0f);
}