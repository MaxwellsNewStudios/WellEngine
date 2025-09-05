#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/BlurParams.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/BlurParams.hlsli"
#endif

RWTexture2D<float4> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	int2 inDim, outDim;
	Input.GetDimensions(inDim.x, inDim.y);
	Output.GetDimensions(outDim.x, outDim.y);
    
	int2 inCoord = int2(float2(DTid.xy) * (float2(inDim) / float2(outDim)));
       
	float3 blurResult = GaussianBlur(inCoord, int2(0, 1), inDim, true).rgb;
	Output[DTid.xy].rgba = float4(blurResult, Input[inCoord].a);
	//float4 blurResult = GaussianBlur(inCoord, int2(0, 1), inDim, true);
	//blurResult.a = lerp(blurResult.a, Input[inCoord].a, 0.75);
	//Output[DTid.xy].rgba = blurResult;
}
