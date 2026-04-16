#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "../Headers/Common.hlsli"
#endif

cbuffer SkyColorBuffer : register(b4)
{
	float4 color;
};


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

	output.color = float4(color.rgb, 1.0);
	output.emission = color.rgb * color.w;
	output.depth = cam_planes.x;
	return output;
}
