#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/DefaultMaterial.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/LightSampling.hlsli"
#else
#include "Headers/DefaultMaterial.hlsli"
#include "Headers/LightSampling.hlsli"
#endif


struct PixelShaderInput
{
	float4 position			: SV_POSITION;
	float4 world_position	: POSITION;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float2 tex_coord		: TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{	
	float4 col = baseColor * Texture.Sample(Sampler, input.tex_coord);
	
	const float3 bitangent = normalize(cross(input.normal, input.tangent));
    const float3 normal = normalize((sampleNormal > 0)
		? mul(NormalMap.Sample(Sampler, input.tex_coord).xyz * 2.0f - float3(1.0f, 1.0f, 1.0f), float3x3(input.tangent, bitangent, input.normal))
		: input.normal);
	
    const float3 specularCol = (sampleSpecular > 0) ? 
		(SpecularMap.Sample(Sampler, input.tex_coord).xyz) : 
		float3(0.0f, 0.0f, 0.0f);

    const float3 ambientCol = (sampleAmbient > 0)
		? AmbientMap.Sample(Sampler, input.tex_coord).xyz
        : float3(0.0, 0.0, 0.0);

    const float3 pos = input.world_position.xyz;
    const float3 viewDir = normalize(cam_position.xyz - pos);
	
	float3 totalDiffuseLight = float3(0.0f, 0.0f, 0.0f);
	float3 totalSpecularLight = float3(0.0f, 0.0f, 0.0f);
	
    CalculateLighting(
		pos, viewDir,							// View
		input.normal, normal, specularCol, 0,	// Surface
		totalDiffuseLight, totalSpecularLight	// Output
	);
		
    float3 totalLight = col.xyz * (ambientCol + totalDiffuseLight) + totalSpecularLight;
	float3 acesLight = ACESFilm(totalLight);
	
	// Apply far-plane depth fade out
	totalLight = ApplyRenderDistanceFog(totalLight, input.position.z);
	
    return float4(saturate(totalLight), col.w);
}
