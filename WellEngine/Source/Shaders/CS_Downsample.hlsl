#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
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
    
	uint2 pixelsToSample = inDim / outDim;
	float3 result = float3(0.0, 0.0, 0.0);
	
	for (uint y = 0; y <= pixelsToSample.y; y++)
	{
		for (uint x = 0; x <= pixelsToSample.x; x++)
		{
			result += Input.SampleLevel(Sampler, float2(inCoord + uint2(x, y)) / float2(inDim), 0);
		}
	}
	
	Output[DTid.xy] = result / (float)(pixelsToSample.x * pixelsToSample.y);
}