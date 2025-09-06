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
	
	const float diffuseColW = MatProp_baseColor.w * Texture.Sample(Sampler, uv).w;
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW - MatProp_alphaCutoff;
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
		
	float3 reflection = EnvironmentCubemap.Sample(Sampler, normalize(reflect(viewDir, surfaceNormal))).rgb;
			
	float3 emissivenessFactor = reflection;
	float emissiveness = max(emissivenessFactor.x, max(emissivenessFactor.y, emissivenessFactor.z));
	float remappedEmissiveness = emissiveness * 0.15 - 0.97;
	emissiveness = 1.0 - pow(remappedEmissiveness, 2.0);
	
	output.color = float4(reflection, emissiveness);
	output.emission = output.color.xyz * emissiveness;
	output.emission = pow(max(0.0, output.emission + 1.0), 1.5) - 1.0;
	
	// Apply far-plane depth fade out
	output.color.xyz = ApplyRenderDistanceFog(output.color.xyz, input.position.z);
	
	return output;
}
