#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "../WellEngine/Source/Shaders/Headers/LightSampling.hlsli"
#else
#include "../Headers/DefaultMaterial.hlsli"
#include "../Headers/LightSampling.hlsli"
#endif

cbuffer ColorBuffer : register(b0)
{
	float4 ColorBuffer_col;
};

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float4 world_position : POSITION;
	float2 tex_coord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
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
	
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);

	output.color = ColorBuffer_col;
	output.depth = length(pos - cam_position.xyz);
	output.emission = 0.0.rrr;
	
	return output;
}
