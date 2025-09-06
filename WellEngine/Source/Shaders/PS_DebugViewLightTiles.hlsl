#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightData.hlsli"
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

struct PixelShaderOutput
{
    float4 color : SV_Target0; // w is emissiveness
    float depth : SV_Target1; // in world units
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	
	uint lightsCounted = 0;
	float3 tileColor = float3(0.0, 0.0, 0.0);
	
	bool sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion;
	GetSampleFlags(sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion);
	
	const float2 uv = input.tex_coord;
    const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 geoNormal = normalize(input.normal);

	const float4 diffuseColW = MatProp_baseColor * Texture.Sample(Sampler, uv);
    const float3 diffuseCol = diffuseColW.xyz;
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW.w - MatProp_alphaCutoff;
		clip(clipVal);
		
		if (clipVal < 0.0)
		{
			output.color = float4(0, 0, 0, 0);
			output.depth = 0;
			return output;
		}
	}
	
	output.depth = length(input.world_position.xyz - cam_position.xyz);
	
	float3 surfaceNormal;
	if (sampleNormal)
	{
		float3 geoTangent = normalize(input.tangent);
		float3 geoBitangent = normalize(cross(geoNormal, geoTangent));
		
		float3 normalSample = float2(MatProp_normalFactor, 1.0).xxy * (NormalMap.Sample(Sampler, uv).xyz * 2.0 - 1.0.xxx);
		
		surfaceNormal = mul(normalSample, float3x3(geoTangent, geoBitangent, geoNormal));
		surfaceNormal = normalize(surfaceNormal);
	}
	else
	{
		surfaceNormal = geoNormal;
	}
	
	const float3 specularCol = sampleSpecular
		? MatProp_specularFactor * SpecularMap.Sample(Sampler, uv).xyz
		: float3(0.0, 0.0, 0.0);
	
	const float glossiness = MatProp_glossFactor * (sampleGlossiness
		? GlossinessMap.Sample(Sampler, uv)
		: 1.0 - (1.0 / pow(max(0.0, SpecBuf_specularExponent), 1.75)));

	const float3 ambientCol = sampleAmbient
		? AmbientMap.Sample(Sampler, uv).xyz
        : float3(0.0, 0.0, 0.0);

	const float occlusion = sampleOcclusion
		? Remap(OcclusionMap.Sample(Sampler, uv), 0.0, 1.0, 1.0 - MatProp_occlusionFactor, 1.0)
		: 1.0;
	
    float3 totalDiffuseLight = ambient_light.xyz;
    float3 totalSpecularLight = float3(0.0, 0.0, 0.0);
    {
		float3 normal = surfaceNormal;
        float3 specularColor = specularCol;
	
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
		
            const float projectedDist = dot(light.direction, -toLight);
            float inverseLightDistSqr = light.orthographic > 0 ?
				(projectedDist > 0.0 ? 1.0 : 0.0) * (1.0 / (1.0 + pow(projectedDist, 2))) : // If orthographic
				1.0 / (1.0 + (pow(toLight.x * light.falloff, 2.0) + pow(toLight.y * light.falloff, 2.0) + pow(toLight.z * light.falloff, 2.0))); // If perspective
		
			CutoffLight(light.color.xyz, inverseLightDistSqr);
					
            const float offsetAngle = saturate(1.0 - (
				light.orthographic > 0 ?
				length(cross(light.direction, toLight)) :
				acos(dot(-toLightDir, light.direction))
				) / (light.angle * 0.5)
			);


            float3 diffuseLightCol, specularLightCol;
            BlinnPhong(toLightDir, viewDir, normal, light.color.xyz, glossiness, diffuseLightCol, specularLightCol);
            specularLightCol *= specularColor;


			// Calculate shadow projection
			const float4 fragPosLightClip = mul(float4(pos + normal * SHADOW_NORMAL_OFFSET, 1.0f), light.vp_matrix);
            const float3 fragPosLightNDC = fragPosLightClip.xyz / fragPosLightClip.w;
		
            const bool isInsideFrustum = (
				fragPosLightNDC.x > -1.0f && fragPosLightNDC.x < 1.0f &&
				fragPosLightNDC.y > -1.0f && fragPosLightNDC.y < 1.0f
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
			
	
			uint seed = 112 + currentSpotlight * 773;
			float3 lightTileColor = float3(RandomValue(seed) * 1.0, RandomValue(seed) * 0.6, RandomValue(seed) * 0.1);
			tileColor += lightTileColor;
			lightsCounted++;
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
		
            const float projectedDist = dot(light.direction, -toLight);
            float inverseLightDistSqr = light.orthographic > 0 ?
				(projectedDist > 0.0 ? 1.0 : 0.0) * (1.0 / (1.0 + pow(projectedDist, 2))) : // If orthographic
				1.0 / (1.0 + (pow(toLight.x * light.falloff, 2) + pow(toLight.y * light.falloff, 2) + pow(toLight.z * light.falloff, 2))); // If perspective
		
			CutoffLight(light.color.xyz, inverseLightDistSqr);
					
            const float offsetAngle = saturate(1.0 - (
				light.orthographic > 0 ?
				length(cross(light.direction, toLight)) :
				acos(dot(-toLightDir, light.direction))
				) / (light.angle * 0.5)
			);


            float3 diffuseLightCol, specularLightCol;
            BlinnPhong(toLightDir, viewDir, normal, light.color.xyz, glossiness, diffuseLightCol, specularLightCol);
            specularLightCol *= specularColor;
		
			// Apply lighting
            totalDiffuseLight += diffuseLightCol * saturate(offsetAngle) * inverseLightDistSqr;
            totalSpecularLight += specularLightCol * saturate(offsetAngle) * inverseLightDistSqr;
			
			
			uint seed = 672 + currentSimpleSpotlight * 1861;
			float3 lightTileColor = float3(RandomValue(seed) * 0.3, RandomValue(seed) * 0.7, RandomValue(seed) * 0.3);
			tileColor += lightTileColor;
			lightsCounted++;
		}
	
	
		// Get pointlight data
        uint pointlightCount = lightTile.pointlightCount;
        uint pointWidth, pointHeight;
        PointShadowMaps.GetDimensions(0, pointWidth, pointHeight, _, _);

        const float
		pointDX = 1.0f / (float) pointWidth,
		pointDY = 1.0f / (float) pointHeight;
	
		// Per-pointlight calculations
        for (uint pointlight_i = 0; pointlight_i < pointlightCount; pointlight_i++)
        {
			// Prerequisite variables
            uint currentPointlight = lightTile.pointlights[pointlight_i];
            const PointLight light = PointLights[currentPointlight];
		
            const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
            const float inverseLightDistSqr = 1.0f / (1.0f + (
				pow(toLight.x * light.falloff, 2.0) +
				pow(toLight.y * light.falloff, 2.0) +
				pow(toLight.z * light.falloff, 2.0)
			));
		
            float3 diffuseLightCol, specularLightCol;
            BlinnPhong(toLightDir, viewDir, normal, light.color.xyz, glossiness, diffuseLightCol, specularLightCol);
            specularLightCol *= specularColor;
		
		
			// Calculate shadow projection
			float4 lightToPosDir = float4(toLight - -SHADOWCUBE_NORMAL_OFFSET * input.normal, currentPointlight);
			float lightDist = max(length(lightToPosDir.xyz) - SHADOWCUBE_DEPTH_EPSILON, 0.0);
			lightToPosDir.xyz = -lightToPosDir.xyz / lightDist;
		
			float shadowFactor = PointShadowMaps.SampleCmpLevelZero(
				ShadowCubeSampler,
				lightToPosDir,
			1.0 - ((lightDist - light.nearZ) / (light.farZ - light.nearZ))
			);
			const float shadow = saturate(shadowFactor);
			
			
			// Apply lighting
            totalDiffuseLight += diffuseLightCol * shadow * inverseLightDistSqr;
            totalSpecularLight += specularLightCol * shadow * inverseLightDistSqr;
			
			
			uint seed = 274 + currentPointlight * 8789;
			float3 lightTileColor = float3(RandomValue(seed) * 0.1, RandomValue(seed) * 0.6, RandomValue(seed) * 1.0);
			tileColor += lightTileColor;
			lightsCounted++;
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
		
            float inverseLightDistSqr = 1.0 / (1.0 + (
				pow(toLight.x * light.falloff, 2.0) +
				pow(toLight.y * light.falloff, 2.0) +
				pow(toLight.z * light.falloff, 2.0)
			));
		
			CutoffLight(light.color.xyz, inverseLightDistSqr);
					
            float3 diffuseLightCol, specularLightCol;
            BlinnPhong(toLightDir, viewDir, normal, light.color.xyz, glossiness, diffuseLightCol, specularLightCol);
            specularLightCol *= specularColor;
				
			// Apply lighting
            totalDiffuseLight += diffuseLightCol * inverseLightDistSqr;
            totalSpecularLight += specularLightCol * inverseLightDistSqr;
			
			
			uint seed = 867 + currentSimplePointlight * 48654;
            float3 lightTileColor = float3(RandomValue(seed) * 0.7, RandomValue(seed) * 0.3, RandomValue(seed) * 0.3);
			tileColor += lightTileColor;
			lightsCounted++;
		}
    }
	
    float3 totalLight = diffuseCol * (occlusion * (ambientCol + totalDiffuseLight)) + occlusion * totalSpecularLight;
	
	float maxChannel = max(tileColor.x, max(tileColor.y, tileColor.z));
	tileColor = (maxChannel <= 1.0) ? (tileColor) : (tileColor / maxChannel);
	
	float tileImportance = 0.75 * (1.0 - 1.0 / (float)(1 << lightsCounted));
	
	output.color = float4(lerp(totalLight, tileColor, tileImportance), 1.0);
    return output;
}