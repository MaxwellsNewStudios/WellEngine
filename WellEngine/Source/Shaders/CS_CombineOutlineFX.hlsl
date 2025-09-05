#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

Texture2D<float4> SceneColor : register(t0);
Texture2D<float3> Emission : register(t1);
Texture2D<float4> Fog : register(t2);
Texture2D<float> Outline : register(t3);

RWTexture2D<unorm float4> BackBufferUAV : register(u0);

cbuffer EmissionSettings : register(b6)
{
	float emission_strength;
	float emission_exponent;
	float emission_threshold;
	
	float _emission_padding;
};

cbuffer OutlineSettings : register(b7)
{
	float4 outline_color;
	float outline_strength;
	float outline_exponent;
	float  outline_smoothing;
	
	float  _outline_padding[1];
};

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	int2 outDim;
	BackBufferUAV.GetDimensions(outDim.x, outDim.y);
	float2 uv = float2(DTid.xy) / float2(outDim);
	
	float4 sceneCol = SceneColor.SampleLevel(Sampler, uv, 0);
	float3 emission = (pow(abs(Emission.SampleLevel(Sampler, uv, 0) + 1.0), emission_exponent) - emission_threshold) * emission_strength;
	float4 fog = Fog.SampleLevel(Sampler, uv, 0);
	
	float3 result = sceneCol.xyz + max(emission, float3(0, 0, 0));
	result = ACESFilm(result + fog.xyz * fog.w);
	result = lerp(result, float3(0.0, 0.0, 0.0), ambient_light.w);
	
	float outlineColor = Outline.SampleLevel(Sampler, uv, 0);
	float outlineValue = lerp(0.0, outline_strength, outline_color.a);
	outlineValue = lerp(0.0, outlineValue, outlineColor * saturate(Remap(outlineColor, 1.0 - outline_smoothing, 1.0, 1.0, 0.0)));
	result = lerp(result, outline_color.rgb, pow(saturate(outlineValue), outline_exponent).rrr);
	
	BackBufferUAV[DTid.xy] = float4(result, 1.0);
}