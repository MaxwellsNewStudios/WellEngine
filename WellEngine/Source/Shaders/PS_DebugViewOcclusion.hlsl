#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/DefaultMaterial.hlsli"
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
	bool sampleOcclusion, _;
	GetSampleFlags(_, _, _, _, _, sampleOcclusion);
	
	const float2 uv = input.tex_coord;
	
	const float occlusion = sampleOcclusion
		? Remap(OcclusionMap.Sample(Sampler, uv), 0.0, 1.0, 1.0 - MatProp_occlusionFactor, 1.0)
		: 1.0;
	
	return float4(occlusion.xxx, 1.0);
}