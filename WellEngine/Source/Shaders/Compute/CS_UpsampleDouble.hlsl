#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "../Headers/Common.hlsli"
#endif

Texture2D<float3> Input : register(t0);
RWTexture2D<float3> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 outDim;
	Output.GetDimensions(outDim.x, outDim.y);
	
	const uint2 outCoord = DTid.xy;
	
    // Map output pixel to normalized UV space
	float2 uv = (float2(outCoord) + 0.5) / float2(outDim);

    // Sample input with bilinear filtering
	float3 color = Input.SampleLevel(Sampler, uv, 0);

	Output[outCoord] = color;
}