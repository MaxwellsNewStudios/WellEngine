#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "../Headers/Common.hlsli"
#endif

Texture2D<float4> AccumColor : register(t0);
Texture2D<float1> RevealColor : register(t1);

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 tex_coord : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_Target0
{
	float2 uv = float2(input.tex_coord.x, 1.0 - input.tex_coord.y);
	
	float4 accum = AccumColor.Sample(Sampler, uv);
	float reveal = RevealColor.Sample(Sampler, uv);
	
	if (EqualEst(reveal, 1.0, 0.00001))
		clip(-1.0);
	
	return float4(accum.rgb / max(accum.a, 1e-5), reveal);
}
