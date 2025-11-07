#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "../Headers/Common.hlsli"
#endif

static const float EPS = 1e-5;

Texture2D<float4> AccumColor : register(t0);
Texture2D<float1> RevealColor : register(t1);

RWTexture2D<unorm float4> OpaqueColor : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	int2 outDim;
	OpaqueColor.GetDimensions(outDim.x, outDim.y);
    
	float2 uv = (float2(DTid.xy) + float2(0.5, 0.5)) / float2(outDim);
	
	float4 opaque = OpaqueColor[DTid.xy];
	float4 accum = AccumColor.SampleLevel(Sampler, uv, 0);
	float reveal = RevealColor.SampleLevel(Sampler, uv, 0);
	
	if (EqualEst(reveal, 1.0, 0.00001))
		return;
	
    // Avoid divide by zero
	float invAlphaSum = 1.0 / max(reveal, EPS);

	float3 avgColor = accum.rgb * invAlphaSum; // Decode weighted color

	float transAlpha = 1.0 - reveal; // Final alpha of combined transparent layers

	float3 opaqueColor = opaque.rgb;

    // Final composite: transparent layers over opaque: C = avgColor * transAlpha + opaqueColor * (1 - transAlpha)
	float3 final = avgColor * transAlpha + opaqueColor * (1.0 - transAlpha);

	OpaqueColor[DTid.xy] = float4(final, 1.0);
}
