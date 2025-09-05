#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
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

float1 main(PixelShaderInput input) : SV_Target0
{
	const float1 textureAlpha = Texture.Sample(Sampler, input.tex_coord).a;
	
	if (alphaCutoff > 0.0)
	{
		clip(mad(textureAlpha, baseColor.a, -alphaCutoff));
		clip(textureAlpha > 0.0 ? 1.0 : -1.0);
	}
	
	return 1.0;
}
