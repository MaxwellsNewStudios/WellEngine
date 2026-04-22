
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

HullShaderOutput CalcHSPatchConstants(InputPatch<VertexShaderOutput, 2> patch, uint patchID : SV_PrimitiveID)
{
	HullShaderOutput output;

	float tess = patch[0].tessFactor;

	output.EdgeTess[0] = tess;
	output.EdgeTess[1] = tess;
	return output;
}

[domain("isoline")]
[partitioning("integer")]
[outputtopology("line")]
[outputcontrolpoints(2)]
[patchconstantfunc("CalcHSPatchConstants")]
VertexShaderOutput main(InputPatch<VertexShaderOutput, 2> patch, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
	return patch[i];
}
