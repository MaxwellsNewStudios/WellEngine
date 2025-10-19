
struct VertexShaderInput
{
	float4 position : POSITION;
	float4 color	: COLOR;
	float4 texcoord : TEXCOORD;
	float2 size		: SIZE;
};

struct VertexShaderOutput
{
	float4 position : POSITION;
	float4 color	: COLOR;
	float4 texcoord : TEXCOORD;
	float2 size		: SIZE;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	output.position = float4(input.position.xyz, 1.0f);
	output.color = input.color;
	output.texcoord = input.texcoord;
	output.size = input.size;
	return output;
}