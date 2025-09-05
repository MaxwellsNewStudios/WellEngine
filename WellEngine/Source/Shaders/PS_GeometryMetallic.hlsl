#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/LightSampling.hlsli"
//#include "Litet-Spelprojekt/Content/Shaders/Shared/ShaderData_PS_Lit.h"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
//#include "Shared/ShaderData_PS_Lit.h"
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

struct PixelShaderOutput
{
	float4 color : SV_Target0; // w is emissiveness
	float depth : SV_Target1; // in world units
	float3 emission : SV_Target2;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;

	const float4 diffuseColW = baseColor * Texture.Sample(Sampler, input.tex_coord);
	const float3 diffuseCol = diffuseColW.xyz;
	
	if (alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW.w - alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			output.color = float4(0, 0, 0, 0);
			output.depth = 0;
			output.emission = float3(0, 0, 0);
			return output;
		}
	}
	
	output.depth = length(input.world_position.xyz - cam_position.xyz);
    
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	
	const float3 bitangent = normalize(cross(input.normal, input.tangent));
	const float3 norm = (sampleNormal > 0)
		? normalize(mul(NormalMap.Sample(Sampler, input.tex_coord).xyz * 2.0 - float3(1.0, 1.0, 1.0), float3x3(input.tangent, bitangent, input.normal)))
        : normalize(input.normal);

	const float3 specularCol = (sampleSpecular > 0)
		? SpecularMap.Sample(Sampler, input.tex_coord).xyz
		: float3(0.0, 0.0, 0.0);
	
	const float glossiness = specularFactor * ((sampleGlossiness > 0)
		? GlossinessMap.Sample(Sampler, input.tex_coord).x
		: 1.0 - (1.0 / pow(max(0.0, specularExponent), 1.75)));

	const float3 ambientCol = (sampleAmbient > 0)
		? AmbientMap.Sample(Sampler, input.tex_coord).xyz
        : float3(0.0, 0.0, 0.0);

	const float reflective = reflectivity * ((sampleReflective > 0)
		? ReflectiveMap.Sample(Sampler, input.tex_coord).x
		: 1.0);

	const float occlusion = (sampleOcclusion > 0)
		? 0.15 + (OcclusionMap.Sample(Sampler, input.tex_coord).x * 0.85)
		: 1.0;
	
	float3 totalDiffuseLight, totalSpecularLight;
	CalculateLighting(
		pos, viewDir, // View
		input.normal, norm, specularCol, glossiness, // Surface
		totalDiffuseLight, totalSpecularLight, // Output
		false
	);
	
	float reflectStrength;
	float3 reflection = ComputeReflectionIntensity(
		norm, viewDir, 
		diffuseCol, saturate(1.0 - glossiness),
		reflectStrength
	);
	reflectStrength = saturate(reflectStrength * reflective);
	
	/*float3 totalLight = lerp(
		diffuseCol * (ambientCol + occlusion * totalDiffuseLight) + occlusion * totalSpecularLight, 
		occlusion * reflection, 
		reflectStrength
	);*/
	float3 totalLight = 
		diffuseCol * (ambientCol + occlusion * totalDiffuseLight) + 
		occlusion * totalSpecularLight +
		occlusion * reflective * reflection;
	
	float3 emissivenessFactor = lerp(0.1 * pow(max(totalLight, 0.0.rrr), 1.5) + 0.9 * totalSpecularLight, reflection, reflectStrength.rrr);
	float emissiveness = max(emissivenessFactor.x, max(emissivenessFactor.y, emissivenessFactor.z));
	float remappedEmissiveness = emissiveness * 0.15 - 0.97;
	emissiveness = 1.0 - pow(remappedEmissiveness, 2.0);
	
	output.color = float4(totalLight, emissiveness);
	output.emission = output.color.xyz * emissiveness;
	output.emission = pow(max(0.0, output.emission + 1.0), 1.5) - 1.0;
	
	// Apply far-plane depth fade out
	output.color.xyz = ApplyRenderDistanceFog(output.color.xyz, input.position.z);
	
	return output;
}
