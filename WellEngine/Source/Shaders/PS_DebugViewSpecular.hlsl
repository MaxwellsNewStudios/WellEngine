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

float4 main(PixelShaderInput input) : SV_Target
{
    const float3 pos = input.world_position.xyz;

    const float3 diffuseCol = Texture.Sample(Sampler, input.tex_coord).xyz;
	
    const float3 bitangent = normalize(cross(input.normal, input.tangent));
    const float3 norm = (sampleNormal > 0)
		? normalize(mul(NormalMap.Sample(Sampler, input.tex_coord).xyz * 2.0 - float3(1.0, 1.0, 1.0), float3x3(input.tangent, bitangent, input.normal)))
        : normalize(input.normal);
    const float3 viewDir = normalize(cam_position.xyz - pos);

    const float3 specularCol = (sampleSpecular > 0)
		? SpecularMap.Sample(Sampler, input.tex_coord).xyz
		: float3(0.0, 0.0, 0.0);
    
	const float glossiness = specularFactor * ((sampleGlossiness > 0)
		? GlossinessMap.Sample(Sampler, input.tex_coord).x
		: 1.0 - (1.0 / pow(max(0.0, specularExponent), 1.75)));
    
    float3 totalDiffuseLight, totalSpecularLight;
	
    CalculateLighting(
		pos, viewDir, // View
		input.normal, norm, specularCol, glossiness, // Surface
		totalDiffuseLight, totalSpecularLight // Output
	);
	
    float3 totalLight = totalSpecularLight;
    float3 acesLight = ACESFilm(totalLight);
		
	return float4(totalLight, 1.0);
}