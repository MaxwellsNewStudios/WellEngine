
cbuffer WorldMatrixBuffer : register(b0)
{
	matrix worldMatrix;
	matrix inverseTransposeWorldMatrix;
};

cbuffer ViewProjMatrixBuffer : register(b1)
{
	matrix viewProjMatrix;
};


struct VertexShaderInput
{
	float3 position			: POSITION;
    float3 normal			: NORMAL;
    float3 tangent			: TANGENT;
	float2 tex_coord		: TEXCOORD;
};

struct VertexShaderOutput
{
	float4 position			: SV_POSITION;
	float4 world_position	: POSITION;
    float3 normal			: NORMAL;
    float3 tangent			: TANGENT;
	float2 tex_coord		: TEXCOORD;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	
	output.world_position = mul(float4(input.position, 1.0f), worldMatrix);
	output.position = mul(output.world_position, viewProjMatrix);
	output.normal =  normalize(mul(float4(input.normal, 0.0f), inverseTransposeWorldMatrix).xyz);
	output.tangent =  normalize(mul(float4(input.tangent, 0.0f), inverseTransposeWorldMatrix).xyz);
	output.tex_coord = input.tex_coord;

	return output;
}