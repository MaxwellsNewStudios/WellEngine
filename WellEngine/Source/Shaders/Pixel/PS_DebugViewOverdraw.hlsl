struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float4 world_position : POSITION;
	float2 tex_coord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	return float4(1.0.rrr, 0.5);
}