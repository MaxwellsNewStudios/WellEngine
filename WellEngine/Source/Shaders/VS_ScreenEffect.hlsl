struct VSOutput
{
	float4 position : SV_POSITION;
	float2 tex_coord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
	VSOutput output;

    // Triangle covering the full screen
	float2 pos[3] = {
		float2(-1.0f, -1.0f), // bottom-left
        float2(-1.0f, 3.0f), // top-left (goes off screen to ensure full coverage)
        float2(3.0f, -1.0f), // bottom-right (also goes off screen)
	};

	float2 uv[3] = {
		float2(0.0f, 0.0f),
        float2(0.0f, 2.0f),
        float2(2.0f, 0.0f),
	};

	output.position = float4(pos[vertexID], 0.0f, 1.0f);
	output.tex_coord = uv[vertexID];
	return output;
}
