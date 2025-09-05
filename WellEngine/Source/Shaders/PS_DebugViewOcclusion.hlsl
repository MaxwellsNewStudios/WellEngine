#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
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
	const float occlusion = (sampleOcclusion > 0)
		? 0.15 + (OcclusionMap.Sample(Sampler, input.tex_coord).x * 0.85)
		: 1.0;
	
	return float4(occlusion, occlusion, occlusion, 1.0);
}