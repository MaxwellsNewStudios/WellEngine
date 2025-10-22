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
	
	float4 worldPos = mul(float4(input.position.xyz, 1.0), worldMatrix);	
	output.position = mul(worldPos, viewProjMatrix);
	
	float3 normal = normalize(mul(input.normal, transpose(worldMatrix)).xyz);
	float lighting = Remap(dot(normal, normalize(cam_position.xyz - worldPos.xyz)), -1.0, 1.0, 0.0, 1.0);
	
	output.color = color;
	output.color.rgb = saturate(output.color.rgb * lighting);
	
	return output;
}