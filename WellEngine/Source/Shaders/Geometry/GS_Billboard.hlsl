
cbuffer CameraData : register(b0)
{
	float4x4 view_projection;
	float4 cam_position;
};

struct GeometryShaderOutput
{
	float4 position : SV_POSITION;
	float4 world_position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 tex_coord : TEXCOORD;
};


struct ParticleIn
{
	float3 position : POSITION;
	float4 color : COLOR;
	float size : SIZE;
};

[maxvertexcount(6)]
void main(
	point ParticleIn input[1], 
	inout TriangleStream<GeometryShaderOutput> output)
{
	float3
		frontVec = normalize(input[0].position - cam_position.xyz),
		rightVec = normalize(cross(frontVec, float3(0, 1, 0))),
		upVec = normalize(cross(frontVec, rightVec));

	rightVec *= input[0].size;
	upVec *= input[0].size;

	GeometryShaderOutput newVertex;
	///newVertex.position = float4(input[0].position, 0);
	newVertex.normal = -frontVec;
	newVertex.color = input[0].color;

	// Top left
	newVertex.world_position = float4(input[0].position - rightVec + upVec, 1.0f); 
	newVertex.position = mul(newVertex.world_position, view_projection);
	newVertex.world_position = float4(input[0].position, 0);
	newVertex.tex_coord = float2(1, 0);
	output.Append(newVertex);

	// Bottom right
	newVertex.world_position = float4(input[0].position + rightVec - upVec, 1.0f); 
	newVertex.position = mul(newVertex.world_position, view_projection);
	newVertex.world_position = float4(input[0].position, 0);
	newVertex.tex_coord = float2(0, 1);
	output.Append(newVertex);

	// Bottom left
	newVertex.world_position = float4(input[0].position - rightVec - upVec, 1.0f); 
	newVertex.position = mul(newVertex.world_position, view_projection);
	newVertex.world_position = float4(input[0].position, 0);
	newVertex.tex_coord = float2(1, 1);
	output.Append(newVertex);

	output.RestartStrip(); // Done with first triangle
	
	// Top left
	newVertex.world_position = float4(input[0].position - rightVec + upVec, 1.0f); 
	newVertex.position = mul(newVertex.world_position, view_projection);
	newVertex.world_position = float4(input[0].position, 0);
	newVertex.tex_coord = float2(1, 0);
	output.Append(newVertex);

	// Top right
	newVertex.world_position = float4(input[0].position + rightVec + upVec, 1.0f); 
	newVertex.position = mul(newVertex.world_position, view_projection);
	newVertex.world_position = float4(input[0].position, 0);
	newVertex.tex_coord = float2(0, 0);
	output.Append(newVertex);

	// Bottom right
	newVertex.world_position = float4(input[0].position + rightVec - upVec, 1.0f); 
	newVertex.position = mul(newVertex.world_position, view_projection);
	newVertex.world_position = float4(input[0].position, 0);
	newVertex.tex_coord = float2(0, 1);
	output.Append(newVertex);
}