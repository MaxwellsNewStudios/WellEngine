#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

// TODO: Figure out input and output formats.
// Inputs: 
//     Camera
//	   Lights (all types)
// Outputs:
//     Light Tile Frustum Culling results

//Texture2D<float3> Input : register(t0);
//RWTexture2D<float3> Output : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	uint2 tileID = DTid.xy;
	uint threadID = GI;
	
	// TODO: Calculate the light tile frustum in one thread.
	if (threadID == 0)
	{
		
	}
	
	DeviceMemoryBarrier();
	
}