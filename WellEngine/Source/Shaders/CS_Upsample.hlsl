#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

Texture2D<float3> Input : register(t0);
RWTexture2D<float3> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 outDim;
    Output.GetDimensions(outDim.x, outDim.y);

    float2 uv = (DTid.xy + 0.5f) / float2(outDim);
    Output[DTid.xy] = Input.SampleLevel(Sampler, uv, 0);
}