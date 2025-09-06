#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/BlurParams.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/BlurParams.hlsli"
#endif

RWTexture2D<float1> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	int2 inDim, outDim;
	Input.GetDimensions(inDim.x, inDim.y);
	Output.GetDimensions(outDim.x, outDim.y);
    
	int2 inCoord = int2(float2(DTid.xy) * (float2(inDim) / float2(outDim)));
       
	float currentValue = Input[inCoord].r;
	if (currentValue == 1.0)
	{
		Output[DTid.xy].r = 1.0;
		return;
	}
		
	float blurResult = GaussianBlur(inCoord, int2(0, 1), inDim, false).r;
	Output[DTid.xy].r = saturate(blurResult);
}
