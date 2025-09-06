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
	float linearDepth = LinearizeDepth(input.position.z);
    return float4(linearDepth.rrr, 1.0);
}