
cbuffer CameraViewProj : register(b0)
{
	float4x4 view_projection;
};

cbuffer CameraDir : register(b1)
{
	float4 cam_direction;
};


struct SpriteIn
{
	float4 position : POSITION;
	float4 color	: COLOR;
	float4 texcoord : TEXCOORD;
	float2 size		: SIZE;
};

struct GeometryShaderOutput
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
	float4 texcoord	: TEXCOORD;
};

[maxvertexcount(6)]
void main(
	point SpriteIn input[1],
	inout TriangleStream<GeometryShaderOutput> output)
{
	float3
		frontVec = normalize(cam_direction.xyz),
		rightVec = normalize(cross(frontVec, float3(0.0, 1.0, 0.0))),
		upVec = normalize(cross(frontVec, rightVec));

	rightVec *= input[0].size.x;
	upVec *= input[0].size.y;

	GeometryShaderOutput newVertex;
	newVertex.color = input[0].color;
	
	float4 botLeftPos = mul(float4(input[0].position.xyz - rightVec - upVec, 1.0), view_projection);
	float4 topLeftPos = mul(float4(input[0].position.xyz - rightVec + upVec, 1.0), view_projection);
	float4 botRightPos = mul(float4(input[0].position.xyz + rightVec - upVec, 1.0), view_projection);
	float4 topRightPos = mul(float4(input[0].position.xyz + rightVec + upVec, 1.0), view_projection);
	
	// Top left
	newVertex.position = topLeftPos;
	newVertex.texcoord = float4(input[0].texcoord.xy, 0.0, 0.0);
	output.Append(newVertex);
	
	// Bottom right
	newVertex.position = botRightPos;
	newVertex.texcoord = float4(input[0].texcoord.zw, 0.0, 0.0);
	output.Append(newVertex);

	// Bottom left
	newVertex.position = botLeftPos;
	newVertex.texcoord = float4(input[0].texcoord.xw, 0.0, 0.0);
	output.Append(newVertex);

	output.RestartStrip(); // Done with first triangle
	
	// Top left
	newVertex.position = topLeftPos;
	newVertex.texcoord = float4(input[0].texcoord.xy, 0.0, 0.0);
	output.Append(newVertex);

	// Top right
	newVertex.position = topRightPos;
	newVertex.texcoord = float4(input[0].texcoord.zy, 0.0, 0.0);
	output.Append(newVertex);

	// Bottom right
	newVertex.position = botRightPos;
	newVertex.texcoord = float4(input[0].texcoord.zw, 0.0, 0.0);
	output.Append(newVertex);
}