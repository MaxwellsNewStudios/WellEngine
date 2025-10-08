#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Helpers.hlsli"
#include "../WellEngine/Source/Shaders/Headers/LightData.hlsli"
#else
#include "Headers/Helpers.hlsli"
#include "Headers/LightData.hlsli"
#endif

#define TILE_SIZE 16

struct Frustum
{
	float3 origin; // Origin of the frustum (and projection).
	float4 orientation; // Quaternion representing rotation.
	float rightSlope; // Positive X (X/Z)
	float leftSlope; // Negative X
	float topSlope; // Positive Y (Y/Z)
	float bottomSlope; // Negative Y
	float near; // Z of the near plane.
	float far; // Z of the far plane.
	
	float padding;
};

struct Sphere
{
	float3 center;
	float radius;
};


bool FrustumFrustumIntersect(in const Frustum a, in const Frustum b)
{
	// TODO: Implement this.
	
	return false;
}



cbuffer Camera : register(b0)
{
	Frustum viewFrustum;
};

StructuredBuffer<Frustum> SpotLightBounds : register(t0);
StructuredBuffer<Frustum> SimpleSpotLightBounds : register(t1);
StructuredBuffer<Sphere> PointLightBounds : register(t2);
StructuredBuffer<Sphere> SimplePointLightBounds : register(t3);

// Each xy coordinate is a light tile & each z slice is a light index.
// The channel denotes the light type: (SpotLight, SimpleSpotLight, PointLight, SimplePointLight)
// z = 0 is always the count of that light in that tile.
RWTexture3D<uint4> LightTileWriteBuffer : register(u4);


// TODO: Change to run on a depth texture, use that to calculate the near/far planes of each tile.

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	uint2 tileID = DTid.xy;
	uint threadID = GI;
	
	// Calculate the light tile frustum.
	Frustum tileFrustum = viewFrustum;
	
	// Calculate the slopes of the tile.
	
	
}