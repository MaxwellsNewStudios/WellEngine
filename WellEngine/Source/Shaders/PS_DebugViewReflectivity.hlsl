#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/DefaultMaterial.hlsli"
#endif


float3 ComputeReflectionIntensity(
    float3 n, float3 v,
    float3 diffuse, float roughness,
	out float strength)
{
    // Reflection vector
	float3 r = normalize(reflect(v, n));

    // Base reflectivity
	float3 f0 = lerp(float3(0.04, 0.04, 0.04), diffuse, metallic);
	
    // Sample environment map
	float3 envColor = 1.0.rrr;

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
	const float diffuseColW = baseColor.w * Texture.Sample(Sampler, input.tex_coord).w;
	
	if (alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW - alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			return 0.0.rrrr;
		}
	}
	
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	
	const float3 bitangent = normalize(cross(input.normal, input.tangent));
	const float3 norm = (sampleNormal > 0)
		? normalize(mul(NormalMap.Sample(Sampler, input.tex_coord).xyz * 2.0 - float3(1.0, 1.0, 1.0), float3x3(input.tangent, bitangent, input.normal)))
        : normalize(input.normal);
	
	const float reflective = reflectivity * ((sampleReflective > 0)
		? ReflectiveMap.Sample(Sampler, input.tex_coord).x
		: 1.0);
			
	float reflectStrength;
	float3 reflection = ComputeReflectionIntensity(
		norm, viewDir,
		0.0.rrr, 0.0,
		reflectStrength
	);
	reflectStrength = saturate(reflectStrength * reflective);
	
	return float4(reflectStrength.rrr, 1.0);
}