
cbuffer ViewProjMatrixBuffer : register(b1)
{
	matrix viewProjMatrix[6];
};


struct GS_CUBEMAP_IN
{
	float4 Pos : SV_POSITION; // World position
};

struct PS_CUBEMAP_IN
{
	float4 Pos : SV_POSITION; // Projection coord
	float4 WorldPos : POSITION;
	uint RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void main(triangle GS_CUBEMAP_IN input[3], inout TriangleStream<PS_CUBEMAP_IN> CubeMapStream)
{
	// TODO: Implement this for shadowcasting pointlights.
	// See https://github.com/microsoft/DirectX-SDK-Samples/tree/main/C%2B%2B/Direct3D10/CubeMapGS
	
	for (int f = 0; f < 6; ++f)
	{
		PS_CUBEMAP_IN output;
		output.RTIndex = f;
		for (int v = 0; v < 3; ++v)
		{
			output.WorldPos = input[v].Pos;
			output.Pos = mul(input[v].Pos, viewProjMatrix[f]);
			CubeMapStream.Append(output);
		}
		CubeMapStream.RestartStrip();
	}
}
