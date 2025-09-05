#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

Texture2D<float3> Input : register(t0);
RWTexture2D<float3> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 inDim, outDim;
	Input.GetDimensions(inDim.x, inDim.y);
	Output.GetDimensions(outDim.x, outDim.y);
	
	uint2 inCoord = uint2(float2(DTid.xy) * (float2(inDim) / float2(outDim)));
    
	uint2 sampleCoverage = inDim / outDim;
	float3 result = Input.SampleLevel(Sampler, float2(inCoord + uint2(sampleCoverage.x * 0.5, sampleCoverage.y * 0.5)) / float2(inDim), 0);
	Output[DTid.xy] = result;
}