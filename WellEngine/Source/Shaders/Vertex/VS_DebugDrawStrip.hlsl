
cbuffer ViewProjMatrixBuffer : register(b0)
{
	matrix viewProjMatrix;
};


struct VertexShaderInput
{
	float3 position : POSITION;
	float size		: SIZE;
	float4 color	: COLOR;
};

struct VertexShaderOutput
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	output.position = mul(float4(input.position, 1.0f), viewProjMatrix);
	output.color = input.color;
	return output;
}