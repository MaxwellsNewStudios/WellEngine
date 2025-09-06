#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/Common.hlsli"
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
	bool sampleSpecular, _;
	GetSampleFlags(_, sampleSpecular, _, _, _, _);
    
	const float2 uv = input.tex_coord;
	
	const float3 specularCol = sampleSpecular
		? MatProp_specularFactor * SpecularMap.Sample(Sampler, uv).xyz
		: float3(0.0, 0.0, 0.0);
    
	return float4(specularCol, 1.0);
}