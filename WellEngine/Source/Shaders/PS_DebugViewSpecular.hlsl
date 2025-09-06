#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightSampling.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
#endif


struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 world_position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_Target
{
	bool sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion;
	GetSampleFlags(sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion);

	const float2 uv = input.tex_coord;
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 geoNormal = normalize(input.normal);
    
	const float diffuseColW = MatProp_baseColor * Texture.Sample(Sampler, uv).w;
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW - MatProp_alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			return float4(0.0.xxx, 1.0);
		}
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
		: float3(0.0, 0.0, 0.0);
	
	const float glossiness = MatProp_glossFactor * (sampleGlossiness
		? GlossinessMap.Sample(Sampler, uv)
		: 1.0 - (1.0 / pow(max(0.0, SpecBuf_specularExponent), 1.75)));
	
    float3 totalDiffuseLight, totalSpecularLight;
    CalculateLighting(
		pos, viewDir, // View
		geoNormal, surfaceNormal, specularCol, glossiness, // Surface
		totalDiffuseLight, totalSpecularLight // Output
	);
	
    float3 totalLight = totalSpecularLight;
    float3 acesLight = ACESFilm(totalLight);
		
	return float4(totalLight, 1.0);
}