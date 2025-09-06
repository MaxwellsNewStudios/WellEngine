#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightSampling.hlsli"
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
	
	bool sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion;
	GetSampleFlags(sampleNormal, sampleSpecular, sampleGlossiness, sampleReflective, sampleAmbient, sampleOcclusion);
	
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	const float3 geoNormal = normalize(input.normal);
	
	// Calculate Tri-Planar UVs
	float2 invTexSize = 1.0 / triplanar_tex_size;
	float2 uvx = pos.yz * invTexSize;
	float2 uvy = pos.xz * invTexSize;
	float2 uvz = pos.xy * invTexSize;
	
	if (triplanar_flip_with_normal)
	{
		// Flip UVs to correct for normal direction
		uvx.x = geoNormal.x > 0 ? 1.0 - uvx.x : uvx.x;
		uvx.y = 1.0 - uvx.y;
	
		uvy = float2(1.0, 1.0) - uvy;
		uvy.x = geoNormal.y > 0 ? 1.0 - uvy.x : uvy.x;
	
		uvz.y = geoNormal.z < 0 ? 1.0 - uvz.y : uvz.y;
	}
	
	float3 weights = pow(abs(geoNormal), triplanar_blend_sharpness);
	weights /= weights.x + weights.y + weights.z;
	
	float3 uvWeightPair[3] = {
		float3(uvx, weights.x),
		float3(uvy, weights.y),
		float3(uvz, weights.z)
	};
	
	int i = 0;
	
	float4 diffuseColW = 0.0.rrrr;
	[unroll]
	for (i = 0; i < 3; ++i)
	{
		float2 uv = uvWeightPair[i].xy;
		float weight = uvWeightPair[i].z;
		
		if (weight > 0.001)
			diffuseColW += weight * Texture.Sample(Sampler, uv);
	}
	diffuseColW *= MatProp_baseColor;
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW.w - MatProp_alphaCutoff;
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
	
	float3 surfaceNormal = 0.0.rrr;
	if (sampleNormal)
	{
		const float3 geoTangent = normalize(input.tangent);
		const float3 geoBitangent = cross(geoNormal, geoTangent);
	
		const float3x3 normalMat = float3x3(geoTangent, geoBitangent, geoNormal);
		
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				surfaceNormal += weight * float2(MatProp_normalFactor, 1.0).xxy * (NormalMap.Sample(Sampler, uv).xyz * 2.0 - 1.0.xxx);
		}
		
		surfaceNormal = normalize(mul(surfaceNormal, normalMat));
	}
	else
	{
		surfaceNormal = geoNormal;
	}
	
	float3 specularCol = 0.0.rrr;
	if (sampleSpecular)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				specularCol += weight * SpecularMap.Sample(Sampler, uv).xyz;
		}
		specularCol = MatProp_specularFactor * specularCol;
	}
	
	float glossiness = 0.0;
	if (sampleGlossiness)
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
		glossiness = 1.0 - (1.0 / pow(max(0.0, SpecBuf_specularExponent), 1.75));
	}
	glossiness = MatProp_glossFactor * glossiness;
		
	float3 ambientCol = 0.0.rrr;
	if (sampleAmbient)
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
	if (sampleOcclusion)
	{
		[unroll]
		for (i = 0; i < 3; ++i)
		{
			float2 uv = uvWeightPair[i].xy;
			float weight = uvWeightPair[i].z;
		
			if (weight > 0.001)
				occlusion += weight * OcclusionMap.Sample(Sampler, uv).x;
		}
		occlusion = Remap(occlusion, 0.0, 1.0, 1.0 - MatProp_occlusionFactor, 1.0);
	}
	else
	{
		occlusion = 1.0;
	}
	
	
	// Shading
	float3 totalDiffuseLight, totalSpecularLight;
	CalculateLighting(
		pos, viewDir, // View
		geoNormal, surfaceNormal, specularCol, glossiness, // Surface
		totalDiffuseLight, totalSpecularLight // Output
	);
	
	float3 totalLight = diffuseCol * (occlusion * (ambientCol + totalDiffuseLight)) + totalSpecularLight;
	
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