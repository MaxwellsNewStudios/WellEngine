
Texture2D<float3> Input : register(t0);
RWTexture2D<float3> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint2 inDim;
	Input.GetDimensions(inDim.x, inDim.y);
	
    // Compute corresponding input top-left pixel
	const uint2 inBase = DTid.xy * 2;
	
    // Gather 2x2 pixels with clamping for odd sizes
	uint2 inCoord0 = min(inBase, inDim - 1);
	uint2 inCoord1 = min(inBase + uint2(1, 0), inDim - 1);
	uint2 inCoord2 = min(inBase + uint2(0, 1), inDim - 1);
	uint2 inCoord3 = min(inBase + uint2(1, 1), inDim - 1);

	float3 c0 = Input.Load(int3(inCoord0, 0));
	float3 c1 = Input.Load(int3(inCoord1, 0));
	float3 c2 = Input.Load(int3(inCoord2, 0));
	float3 c3 = Input.Load(int3(inCoord3, 0));

    // Box filter average
	float3 result = (c0 + c1 + c2 + c3) * 0.25;

	Output[DTid.xy] = result;
}