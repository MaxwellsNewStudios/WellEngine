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
	
	uint seed = (uint)((NoiseTexture.Sample(Sampler, input.tex_coord) * 10000.0 + time) * 100.0);
	float randomValue = RandomValue(seed);
	
	if (MatProp_alphaCutoff > 0.0)
		clip(MatProp_alphaCutoff - randomValue);
	
	output.depth = length(input.world_position.xyz - cam_position.xyz) * 1.1f;
	output.color = MatProp_baseColor;
	output.emission = float3(0.0, 0.0, 0.0);
	return output;
}