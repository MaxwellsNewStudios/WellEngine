#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

RWTexture2D<unorm float4> Output : register(u0);
Texture3D LUT : register(t0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float4 inColor = Output[DTid.xy];
	float4 lutColor = LUT.SampleLevel(Sampler, inColor.rgb, 0);
	Output[DTid.xy] = float4(lutColor.rgb, inColor.a);
}