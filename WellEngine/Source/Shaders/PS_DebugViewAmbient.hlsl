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
	bool _, sampleAmbient;
	GetSampleFlags(_, _, _, _, sampleAmbient, _);
    
    const float3 ambientCol = (sampleAmbient > 0)
		? AmbientMap.Sample(Sampler, input.tex_coord).xyz
        : float3(0.0, 0.0, 0.0);
    
    return float4(ambientCol, 1.0);
}