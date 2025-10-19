
cbuffer ViewProjMatrixBuffer : register(b0)
{
	matrix viewProjMatrix;
};


struct VertexShaderInput
{
	float4 position : POSITION;
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
	output.position = mul(input.position, viewProjMatrix);
	output.color = input.color;
	return output;
}