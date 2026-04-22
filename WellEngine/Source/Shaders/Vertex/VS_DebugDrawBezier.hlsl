
struct VertexShaderInOutput
{
	float4 position : POSITION;
	float3 control : CONTROL;
	float tessFactor : TESSFACTOR;
	float4 color : COLOR;
};

VertexShaderInOutput main(VertexShaderInOutput input)
{
	return input;
}