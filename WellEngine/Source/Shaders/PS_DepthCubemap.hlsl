
cbuffer CameraCubeBuffer : register(b1)
{
	float4 CamCube_position;
	float CamCube_nearZ;
	float CamCube_farZ;
	
	float CamCube_padding[2];
};

struct PixelShaderInput
{
	float4 Pos : SV_POSITION;
	float4 WorldPos : POSITION;
	uint RTIndex : SV_RenderTargetArrayIndex;
};

float main(PixelShaderInput input) : SV_Depth
{
	//return 1.0 - (length(CamCube_position.xyz - input.WorldPos.xyz) / CamCube_farZ);
	return 1.0 - ((length(CamCube_position.xyz - input.WorldPos.xyz) - CamCube_nearZ) / (CamCube_farZ - CamCube_nearZ));
}