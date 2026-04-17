#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "../Headers/Common.hlsli"
#endif

Texture2D<float3> SourceMip : register(t0);
RWTexture2D<float3> TargetMip : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 outDim;
	TargetMip.GetDimensions(outDim.x, outDim.y);

	if (DTid.x >= outDim.x || DTid.y >= outDim.y)
		return;

	float2 uv = (float2(DTid.xy) + 0.5f) / float2(outDim);
	float3 source = SourceMip.SampleLevel(Sampler, uv, 0);
	TargetMip[DTid.xy] += source;
}
