#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/LightSampling.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
#endif


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
	
	const float4 diffuseColW = baseColor * Texture.Sample(Sampler, input.tex_coord);
	const float3 diffuseCol = diffuseColW.xyz;
		
	output.depth = length(input.world_position.xyz - cam_position.xyz);
    
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	
	const float3 bitangent = normalize(cross(input.normal, input.tangent));
	const float3 norm = (sampleNormal > 0)
		? normalize(mul(NormalMap.Sample(Sampler, input.tex_coord).xyz * 2.0 - float3(1.0, 1.0, 1.0), float3x3(input.tangent, bitangent, input.normal)))
        : normalize(input.normal);

	const float3 specularCol = (sampleSpecular > 0)
		? SpecularMap.Sample(Sampler, input.tex_coord).xyz
		: float3(0.0, 0.0, 0.0);
	
	const float glossiness = specularFactor * ((sampleGlossiness > 0)
		? GlossinessMap.Sample(Sampler, input.tex_coord).x
		: 1.0 - (1.0 / pow(max(0.0, specularExponent), 1.75)));
		
	float4 reflection = EnvironmentCubemap.Sample(Sampler, normalize(reflect(viewDir, norm)));
	
	float emissiveness = saturate(Remap(reflection.x + reflection.y + reflection.z, 0.0, 3.0, -3.0, 1.0));
	
	float remappedEmissiveness = emissiveness * 0.15 - 0.95;
	emissiveness = 1.0 - pow(remappedEmissiveness, 2.0);
	
	output.color = float4(reflection.rgb, emissiveness);
	output.emission = output.color.xyz * emissiveness;
	output.emission = pow(max(0.0, output.emission + 1.0), 1.5) - 1.0;
	
	// Apply far-plane depth fade out
	output.color.xyz = ApplyRenderDistanceFog(output.color.xyz, input.position.z);
	
	return output;
}
