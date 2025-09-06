#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/Common.hlsli"
#endif


float3 ComputeReflectionIntensity(
    float3 n, float3 v,
    float3 diffuse, float roughness,
	out float strength)
{
    // Reflection vector
	float3 r = normalize(reflect(v, n));

    // Base reflectivity
	float3 f0 = lerp(float3(0.04, 0.04, 0.04), diffuse, MatProp_metallic);
	
    // Get mip count and match it to highest mip index
	uint _, lodCount;
	EnvironmentCubemap.GetDimensions(0, _, _, lodCount);
	float maxMip = float(lodCount - 1);
	
    // Sample environment map
	float mipLevel = roughness * maxMip;
	float3 envColor = EnvironmentCubemap.SampleLevel(Sampler, r, mipLevel).rgb;

    // Fresnel-Schlick term
	float nDotV = saturate(dot(n, v));
	strength = pow(1.0 - nDotV, 3.5);
	float3 f = f0 + (1.0 - f0) * strength;

    // Combine
	float3 reflection = envColor * f;
	return reflection;
}


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
	bool sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion;
	GetSampleFlags(sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion);

	const float2 uv = input.tex_coord;
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 geoNormal = normalize(input.normal);
	
	const float diffuseColW = MatProp_baseColor.w * Texture.Sample(Sampler, uv).w;
	
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
		float3 geoBitangent = normalize(cross(geoNormal, geoTangent));
		
		float3 normalSample = float2(MatProp_normalFactor, 1.0).xxy * (NormalMap.Sample(Sampler, uv).xyz * 2.0 - 1.0.xxx);
		surfaceNormal = mul(normalSample, float3x3(geoTangent, geoBitangent, geoNormal));
		surfaceNormal = normalize(surfaceNormal);
	}
	else
	{
		surfaceNormal = geoNormal;
	}
	
	const float glossiness = MatProp_glossFactor * (sampleGlossiness
		? GlossinessMap.Sample(Sampler, uv)
		: 1.0 - (1.0 / pow(max(0.0, SpecBuf_specularExponent), 1.75)));

	const float reflective = MatProp_reflectFactor * (sampleReflective
		? ReflectiveMap.Sample(Sampler, uv)
		: 1.0);
			
	float reflectStrength;
	float3 reflection = ComputeReflectionIntensity(
		surfaceNormal, viewDir,
		0.0.rrr, saturate(1.0 - glossiness),
		reflectStrength
	);
	reflectStrength = saturate(reflectStrength * reflective);
	
	return float4(reflective * reflection, 1.0);
}