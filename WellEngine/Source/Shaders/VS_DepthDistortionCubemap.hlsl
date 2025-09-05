#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

cbuffer WorldMatrixBuffer : register(b0)
{
	matrix worldMatrix;
	matrix inverseTransposeWorldMatrix;
};

cbuffer DistortionSettings : register(b2)
{
	float3 distortion_source;
	float distortion_distance;
};


struct VertexShaderInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 tex_coord : TEXCOORD;
};

float4 main(VertexShaderInput input) : SV_POSITION
{
	float4 worldPos = mul(float4(input.position, 1.0), worldMatrix);
	
	float distance = length(worldPos.xyz - distortion_source);
	float linearffectStrength = saturate(1.0 - distance / distortion_distance);
	
	float effectRoundStrength = pow(clamp(linearffectStrength - 0.5, 0.075, 0.5) * 0.4, 3.5) * 0.5;
	float effectNoiseStrength = pow(linearffectStrength * 1.6, 4.0) * 1.15;
	
	float2 uv = (worldPos.xz + (worldPos.y * 0.33 + (time + deltaTime * 60.0) * 16)) / 12.0;
	float4 noise = NoiseTexture.SampleLevel(Sampler, uv, 0.0);
	
	noise -= 0.5;
	noise *= 2.2;
	float4 noiseSign = sign(noise);
	noise = noiseSign * pow(abs(noise), 1.5);
	noise /= 2.2;
	
	float noiseW = noise.w;
	noise.w = 0.0;
	
	worldPos = linearffectStrength > 0.0 ?
		((round(worldPos / effectRoundStrength) * effectRoundStrength) + (noise * effectNoiseStrength)) :
		(worldPos);

	return worldPos;
}