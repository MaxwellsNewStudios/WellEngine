#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "../WellEngine/Source/Shaders/Headers/LightSampling.hlsli"
#else
#include "../Headers/DefaultMaterial.hlsli"
#include "../Headers/LightSampling.hlsli"
#endif

inline float ComputeWeight(float alpha, float depth)
{
	float a = min(1.0, alpha) * 8.0 + 0.01;
	float b = -depth * 0.95 + 1.0;
	float w = a * a * a * 1e8 * b * b * b;
	return saturate(w) * 1.0;
}

struct PixelShaderInput
{
	float4 position			: SV_POSITION;
	float4 world_position	: POSITION;
	float2 tex_coord		: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

struct PixelShaderOutput
{
	float4 accum : SV_Target0;
	float reveal : SV_Target1;
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
	
	const float4 col = MatProp_baseColor * Texture.Sample(Sampler, uv);
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = col.w - MatProp_alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			output.accum = 0.0.rrrr;
			output.reveal = 0.0;
			return output;
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
		: 0.0.rrr;
	
	const float glossiness = MatProp_glossFactor * (sampleGlossiness
		? GlossinessMap.Sample(Sampler, uv)
		: 1.0 - (1.0 / pow(max(0.0, SpecBuf_specularExponent), 1.75)));

	const float3 ambientCol = sampleAmbient
		? AmbientMap.Sample(Sampler, uv).xyz
        : 0.0.rrr;

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
	
	float depth = input.position.z;
	float alpha = col.w;
	
	// Apply far-plane depth fade out
	totalLight = ApplyRenderDistanceFog(totalLight, depth);
	
	//float weight = clamp(pow(min(1.0, alpha * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - depth * 0.9, 3.0), 1e-2, 3e3);
	float weight = ComputeWeight(alpha, depth);
	weight = clamp(weight, 1e-2, 3e2); // clamp to avoid float overflow
	
	output.accum = float4(totalLight * alpha, alpha) * weight;
	output.reveal = alpha;
	
	return output;
}
