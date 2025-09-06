#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

struct PixelShaderInput
{
	float4 position : SV_POSITION;
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
	
	float2 ndc = input.tex_coord * 2.0 - 1.0;
	float4 clipPos = float4(ndc, 1.0, 1.0);

	float4 worldPosH = mul(clipPos, inv_view_proj_matrix);
	worldPosH /= worldPosH.w;

	float3 worldDir = normalize(worldPosH.xyz - cam_position.xyz);	
	
	output.color = float4(worldDir * 0.5 + 0.5, 0.0);
	output.emission = float3(0, 0, 0);
	output.depth = 0;
	return output;
}
