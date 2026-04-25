
cbuffer ViewProjMatrixBuffer : register(b0)
{
	matrix viewProjMatrix;
};


struct VertexShaderOutput
{
	float4 position : POSITION;
	float3 control : CONTROL;
	float tessFactor : TESSFACTOR;
	float4 color : COLOR;
};

struct HullShaderOutput
{
	float EdgeTess[2] : SV_TessFactor;
};

struct DomainShaderOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

[domain("isoline")]
DomainShaderOutput main(HullShaderOutput input, const OutputPatch<VertexShaderOutput, 2> patch, float2 uv : SV_DomainLocation)
{
	DomainShaderOutput output;

	float t = uv.x;

	float3 p0 = patch[0].position.xyz;
	float3 p1 = patch[0].control;
	float3 p2 = patch[1].control;
	float3 p3 = patch[1].position.xyz;

	float u = 1.0 - t;

    // Cubic Bezier formula
	float t2 = t * t;
	float u2 = u * u;
	float3 pos =
        u2 * u * p0 +
        3.0 * u2 * t * p1 +
        3.0 * u * t2 * p2 +
        t2 * t * p3;
	
	output.position = mul(float4(pos, 1.0), viewProjMatrix);
	output.color = lerp(patch[0].color, patch[1].color, t);

	return output;
}