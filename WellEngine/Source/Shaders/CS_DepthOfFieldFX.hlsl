#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/BlurParams.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/BlurParams.hlsli"
#endif

RWTexture2D<unorm float4> Output : register(u0);

cbuffer DepthOfFiledSettings : register(b6)
{
	float focalPlane;
    float aperture;
    float imageDistance;

	float  _dof_padding[1];
};

float3 BlurGather(int2 coord, float coc, Texture2D Input, int2 inDim)
{
    float2 texelSize = 1.0 / float2(inDim);
    float3 sum = 0.0;
    float weightSum = 0.0;

    // simple 2D box blur based on CoC radius
    int radius = (int)ceil(coc);
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            float2 uv = (float2(coord + int2(x,y)) + 0.5) / float2(inDim);
            uv = saturate(uv);
            float3 sample = Input.SampleLevel(Sampler, uv, 0);
            
            // simple Gaussian weight
            float w = exp(-0.5 * (x*x + y*y) / (coc*coc + 1e-5));
            sum += sample * w;
            weightSum += w;
        }
    }

    return sum / weightSum;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	int2 inDim, outDim;
	Input.GetDimensions(inDim.x, inDim.y);
	Output.GetDimensions(outDim.x, outDim.y);
    
	int2 inCoord = int2(float2(DTid.xy) * (float2(inDim) / float2(outDim)));

    float focalLength = 1 / ((1 / focalPlane) + (1 / imageDistance));
    float depth = Depth[inCoord];

    float coc = abs(aperture * ((focalLength * (focalPlane - depth)) / (depth * (focalPlane - focalLength))));

    int radius = min((int)ceil(coc), 20);
    float3 blurred = BlurGather(inCoord, radius, Input, inDim);
    Output[DTid.xy] = float4(blurred, 1.0f);
}
