#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightSampling.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
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

struct PixelShaderOutput
{
	float4 color : SV_Target0; // w is emissiveness
	float depth : SV_Target1; // in world units
	float3 emission : SV_Target2;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	
	bool sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion;
	GetSampleFlags(sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion);

	const float2 uv = input.tex_coord;
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 geoNormal = normalize(input.normal);
	
	const float4 diffuseColW = MatProp_baseColor * Texture.Sample(Sampler, uv);
	const float3 diffuseCol = diffuseColW.rgb;
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW.w - MatProp_alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			output.color = 0.0.rrrr;
			output.depth = 0.0;
			output.emission = 0.0.rrr;
			return output;
		}
	}
	
	output.depth = length(input.world_position.xyz - cam_position.xyz);
    
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
	
	float reflectStrength;
	float3 reflection = ComputeReflectionIntensity(
		surfaceNormal, viewDir,
		diffuseCol, saturate(1.0 - glossiness),
		reflectStrength
	);
	reflectStrength = saturate(reflectStrength * reflective);
	
	float3 totalLight = 
		diffuseCol * (occlusion * (ambientCol + totalDiffuseLight)) + 
		/*occlusion **/ totalSpecularLight +
		/*occlusion **/ reflective * reflection;
	
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
