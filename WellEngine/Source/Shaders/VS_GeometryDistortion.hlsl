#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

cbuffer WorldMatrixBuffer : register(b0)
{
	matrix worldMatrix;
	matrix inverseTransposeWorldMatrix;
};

cbuffer ViewProjMatrixBuffer : register(b1)
{
	matrix viewProjMatrix;
};

cbuffer DistortionSettings : register(b2)
{
	float3 distortion_source;
	float distortion_distance;
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
	
	output.world_position = mul(float4(input.position, 1.0), worldMatrix);
	
	float distance = length(output.world_position.xyz - distortion_source);
	float linearffectStrength = saturate(1.0 - distance / distortion_distance);
	
    float effectRoundStrength = pow(clamp(linearffectStrength - 0.5, 0.075, 0.5) * 0.4, 3.5) * 0.5;
	float effectNoiseStrength = pow(linearffectStrength * 1.6, 4.0) * 1.15;
	float effectUVStrength = pow(saturate(linearffectStrength - 0.1) * 0.6, 2.5);
	
	float2 uv = (output.world_position.xz + (output.world_position.y * 0.33 + (time + deltaTime * 60.0) * 16)) / 12.0;
	float4 noise = NoiseTexture.SampleLevel(Sampler, uv, 0.0);
	
	noise -= 0.5;
	noise *= 2.2;
	float4 noiseSign = sign(noise);
	noise = noiseSign * pow(abs(noise), 1.5);
	noise /= 2.2;
	
	float noiseW = noise.w;
	noise.w = 0.0; 
	
	output.world_position = linearffectStrength > 0.0 ?
		((round(output.world_position / effectRoundStrength) * effectRoundStrength) + (noise * effectNoiseStrength)) :
		(output.world_position);
	
	output.position = mul(output.world_position, viewProjMatrix);
	
	output.normal = normalize(mul(float4(input.normal, 0.0), inverseTransposeWorldMatrix).xyz);
	output.tangent = normalize(mul(float4(input.tangent, 0.0), inverseTransposeWorldMatrix).xyz);
	
	output.tex_coord = input.tex_coord;
	
    int seed = time * 10.0;
    float2 randUVOffset = float2(RandomValue(seed), RandomValue(seed)) - 0.5;
    randUVOffset *= 0.5;
	
	output.tex_coord = effectUVStrength > 0.0 ?
		input.tex_coord + (float2(noiseW + noise.z + randUVOffset.x, noise.y - noise.x + randUVOffset.y) * effectUVStrength) :
		input.tex_coord;
	
	return output;
}