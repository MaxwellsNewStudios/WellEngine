#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "../WellEngine/Source/Shaders/Headers/DefaultMaterial.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/DefaultMaterial.hlsli"
#endif

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float4 world_position : POSITION;
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
	
	const float2 uv = input.tex_coord;
	const float3 pos = input.world_position.xyz;
	const float3 viewDir = normalize(cam_position.xyz - pos);
	
	const float4 diffuseColW = MatProp_baseColor * Texture.Sample(Sampler, uv);
	const float3 diffuseCol = diffuseColW.rgb;
	
	if (MatProp_alphaCutoff > 0.0)
	{
		float clipVal = diffuseColW.w - MatProp_alphaCutoff;
		clip(clipVal);
	
		if (clipVal < 0.0)
		{
			output.color = 0.0.rrrr;
			output.depth = 0.0;
			output.emission = 0.0.rrr;
			
			//output.color = float4(uv.xy, 0.0, 0.0);
			return output;
		}
	}
	
	output.color = float4(diffuseCol, 0.0);
	output.depth = length(input.world_position.xyz - cam_position.xyz);
	output.emission = 0.0.rrr;
	
	return output;
}