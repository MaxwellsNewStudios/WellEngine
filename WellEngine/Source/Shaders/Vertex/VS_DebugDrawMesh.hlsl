#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "../Headers/Common.hlsli"
#endif

cbuffer ViewProjMatrixBuffer : register(b0)
{
	matrix viewProjMatrix;
};

cbuffer InstanceBuffer : register(b1)
{
	matrix worldMatrix;
	float4 color;
};

struct VertexShaderInput
{
	float4 position : POSITION;
	float4 normal : NORMAL;
};

struct VertexShaderOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	
	float4 worldPos = mul(input.position, worldMatrix);
	output.position = mul(worldPos, viewProjMatrix);
	
	float4 normal = normalize(mul(input.normal, worldMatrix));
	float lighting = Remap(dot(normal, cam_direction), -1.0, 1.0, 0.0, 1.0);
	
	output.color = color;
	output.color.rgb *= lighting;
	
	return output;
}