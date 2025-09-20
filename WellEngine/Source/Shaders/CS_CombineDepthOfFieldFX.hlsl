#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

Texture2D<unorm float4> SharpImage : register(t0);
Texture2D<float3> CircleOfConfusion : register(t1);
Texture2D<float3> FullBlur : register(t2);

RWTexture2D<unorm float4> BackBufferUAV : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float g_MaxCoC_ForBlend = 30.0f; // normalize CoC to [0,1] by dividing by this (e.g. 30)
    float g_SmoothRange = 4.0f;   
    
	float4 sharp = SharpImage[DTid.xy];
    float3 blur = FullBlur[DTid.xy];
    float coc = CircleOfConfusion[DTid.xy].r;

    float t = saturate(coc / g_MaxCoC_ForBlend);

    // optional smoothstep for softer transition
    float s = smoothstep(0.0, 1.0, t);
	
	BackBufferUAV[DTid.xy] = float4(lerp(sharp.rgb, blur, s), sharp.a);
}