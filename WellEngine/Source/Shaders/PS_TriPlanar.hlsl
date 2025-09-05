#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/LightSampling.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
#endif

cbuffer TriplanarSettingsBuffer : register(b4)
{
	float2 triplanar_tex_size;
	float triplanar_blend_sharpness;
	bool triplanar_flip_with_normal;
};

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float4 world_position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 tex_coord : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_Target0; // w is emissiveness
	float depth : SV_Target1; // in world units
	float3 emission : SV_Target2;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
		
	const float3 pos = input.world_position.xyz;
	
	// Calculate Tri-Planar UVs
	float2 invTexSize = 1.0 / triplanar_tex_size;
	float2 uvx = pos.yz * invTexSize;
	float2 uvy = pos.xz * invTexSize;
	float2 uvz = pos.xy * invTexSize;
	
	if (triplanar_flip_with_normal)
	{
		// Flip UVs to correct for normal direction
		uvx.x = input.normal.x > 0 ? 1.0 - uvx.x : uvx.x;
		uvx.y = 1.0 - uvx.y;
	
		uvy = float2(1.0, 1.0) - uvy;
		uvy.x = input.normal.y > 0 ? 1.0 - uvy.x : uvy.x;
	
		uvz.y = input.normal.z < 0 ? 1.0 - uvz.y : uvz.y;
	}
	
	float3 weights = pow(abs(input.normal), triplanar_blend_sharpness);
	weights /= weights.x + weights.y + weights.z;
	
	float3 uvWeightPair[3] = {
		float3(uvx, weights.x),
		float3(uvy, weights.y),
		float3(uvz, weights.z)
	};
	
	int i = 0;
	
	float4 diffuseColW = float4(0, 0, 0, 0);
	[unroll]
	for (i = 0; i < 3; ++i)
	{
		float2 uv = uvWeightPair[i].xy;
		float weight = uvWeightPair[i].z;
		
		if (weight > 0.001)
			diffuseColW += weight * Texture.Sample(Sampler, uv);
	}
	diffuseColW *= baseColor;
	
	if (alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW.w - alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			output.color = float4(0, 0, 0, 0);
			output.depth = 0;
			output.emission = float3(0, 0, 0);
			return output;
		}
	}
	
	output.depth = length(input.world_position.xyz - cam_position.xyz);
	
	const float3 diffuseCol = diffuseColW.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 bitangent = cross(input.normal, input.tangent);
	
	const float3x3 normalMat = float3x3(
		normalize(input.tangent), 
		normalize(bitangent),
		normalize(input.normal)
	);
	
	float3 norm = float3(0, 0, 0);
	if (sampleNormal > 0)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				norm += weight * (NormalMap.Sample(Sampler, uv).xyz * 2.0 - 1.0);
		}
		
		norm = normalize(mul(normalize(norm), normalMat));
	}
	else
	{
		norm = normalize(input.normal);
	}
	
	float3 specularCol = float3(0, 0, 0);
	if (sampleSpecular > 0)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				specularCol += weight * SpecularMap.Sample(Sampler, uv).xyz;
		}
	}
	
	float glossiness = 0.0;
	if (sampleGlossiness > 0)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				glossiness += weight * GlossinessMap.Sample(Sampler, uv).x;
		}
	}
	else
	{
		glossiness = 1.0 - (1.0 / pow(max(0.0, specularExponent), 1.75));
	}
	glossiness = specularFactor * glossiness;
		
	float3 ambientCol = float3(0, 0, 0);
	if (sampleAmbient > 0)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				ambientCol += weight * AmbientMap.Sample(Sampler, uv).xyz;
		}
	}
		
	float occlusion = 0;
	if (sampleOcclusion > 0)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				occlusion += weight * OcclusionMap.Sample(Sampler, uv).x;
		}
		occlusion = 0.15 + (occlusion * 0.85);
	}
	else
	{
		occlusion = 1.0;
	}
	
	
	// Shading
	float3 totalDiffuseLight, totalSpecularLight;
	
	CalculateLighting(
		pos, viewDir, // View
		input.normal, norm, specularCol, glossiness, // Surface
		totalDiffuseLight, totalSpecularLight // Output
	);
	
	float3 totalLight = diffuseCol * (occlusion * (ambientCol + totalDiffuseLight)) + occlusion * totalSpecularLight;
	
	float emissiveness = max(totalSpecularLight.x, max(totalSpecularLight.y, totalSpecularLight.z));
	float remappedEmissiveness = emissiveness * 0.15 - 0.97;
	emissiveness = 1.0 - pow(remappedEmissiveness, 2.0);
	
	output.color = float4(totalLight, emissiveness);
	output.emission = output.color.xyz * emissiveness;
	output.emission = pow(max(0.0, output.emission + 1.0), 1.5) - 1.0;
	
	// Apply far-plane depth fade out
	output.color.xyz = ApplyRenderDistanceFog(output.color.xyz, input.position.z);
	
	return output;
}