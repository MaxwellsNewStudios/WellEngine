
sampler Sampler : register(s0);
Texture2D Texture : register(t0);

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
	float4 texcoord : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 result = Texture.Sample(Sampler, input.texcoord.xy) * input.color;
	//clip(result.a - 0.001f);
	return result;
}
