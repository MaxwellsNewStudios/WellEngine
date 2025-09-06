#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightSampling.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
#endif


struct PixelShaderInput
{
	float4 position			: SV_POSITION;
	float4 world_position	: POSITION;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float2 tex_coord		: TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	bool sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion;
	GetSampleFlags(sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion);

	const float2 uv = input.tex_coord;
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 geoNormal = normalize(input.normal);
	
	const float4 col = MatProp_baseColor * Texture.Sample(Sampler, uv);
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = col.w - MatProp_alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
			return 0.0.rrrr;
	}
	
	float3 surfaceNormal;
	if (sampleNormal)
	{
		float3 geoTangent = normalize(input.tangent);
		float3 geoBitangent = cross(geoNormal, geoTangent);
		
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
		: 0.0.rrr;
	
	const float glossiness = MatProp_glossFactor * (sampleGlossiness
		? GlossinessMap.Sample(Sampler, uv)
		: 1.0 - (1.0 / pow(max(0.0, SpecBuf_specularExponent), 1.75)));

	const float3 ambientCol = sampleAmbient
		? AmbientMap.Sample(Sampler, uv).xyz
        : 0.0.rrr;

	const float reflective = MatProp_reflectFactor * (sampleReflective
		? ReflectiveMap.Sample(Sampler, uv)
		: 1.0);

	const float occlusion = sampleOcclusion
		? Remap(OcclusionMap.Sample(Sampler, uv), 0.0, 1.0, 1.0 - MatProp_occlusionFactor, 1.0)
		: 1.0;
	
	float3 totalDiffuseLight, totalSpecularLight;
	CalculateLighting(
		pos, viewDir, // View
		geoNormal, surfaceNormal, specularCol, glossiness, // Surface
		totalDiffuseLight, totalSpecularLight, // Output
		false
	);
		
	float3 totalLight = col.xyz * (ambientCol + occlusion * totalDiffuseLight) + totalSpecularLight;
	
	// Apply far-plane depth fade out
	totalLight = ApplyRenderDistanceFog(totalLight, input.position.z);
	
	float3 acesLight = ACESFilm(totalLight);
    return float4(saturate(totalLight), col.w);
}
