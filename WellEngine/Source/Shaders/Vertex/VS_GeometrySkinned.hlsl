
#define MAX_BONES 128

cbuffer WorldMatrixBuffer : register(b0)
{
	matrix worldMatrix;
	matrix inverseTransposeWorldMatrix;
};

cbuffer ViewProjMatrixBuffer : register(b1)
{
	matrix viewProjMatrix;
};

cbuffer BoneMatrixBuffer : register(b2)
{
	matrix boneMatrices[MAX_BONES];
};


struct VertexShaderInput
{
	float3 position		: POSITION;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float2 tex_coord	: TEXCOORD0;
	int4 boneIDs		: BLENDINDICES;
	float4 boneWeights	: BLENDWEIGHT;
};

struct VertexShaderOutput
{
	float4 position			: SV_POSITION;
	float4 world_position	: POSITION;
	float2 tex_coord		: TEXCOORD;
    float3 normal			: NORMAL;
    float3 tangent			: TANGENT;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	
	// Skinning matrix
	matrix skinMatrix = 0.0;
	
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		int boneIndex = input.boneIDs[i];

		if (boneIndex >= 0)
		{
			skinMatrix += boneMatrices[boneIndex] * input.boneWeights[i];
		}
	}
	
	// Skin vertex position
	float4 localPosition = mul(float4(input.position, 1.0), skinMatrix);

	output.world_position = mul(localPosition, worldMatrix);
	output.position = mul(output.world_position, viewProjMatrix);
	
	// Skin normal and tangent
	float3 skinnedNormal = mul(float4(input.normal, 0.0), skinMatrix).xyz;
	float3 skinnedTangent = mul(float4(input.tangent, 0.0), skinMatrix).xyz;

	output.normal = normalize(mul(float4(skinnedNormal, 0.0), inverseTransposeWorldMatrix).xyz);
	output.tangent = normalize(mul(float4(skinnedTangent, 0.0), inverseTransposeWorldMatrix).xyz);

	// Tex coords
	output.tex_coord = input.tex_coord;
	
	return output;
}