#define SEGMENTS 12 // Number of segments for the half-cylinder curve (higher = smoother)
#define PI 3.14159265

cbuffer CameraData : register(b0)
{
	float4x4 view_projection;
	float4 cam_position;
};


// Function to check if the camera can see either cap of the cylinder
bool CanSeeCylinderCaps(float3 C0, float3 u, float h, float3 P, out bool top)
{
	top = false;
	
    // Compute the vector from the base center to the camera
	float3 v = P - C0;

    // Project v onto the cylinder's up axis
	float d = dot(v, u);

    // Check if the projection is outside the cylinder's height
	if (d < 0.0)
	{ // Camera is below the bottom cap
		return true;
	}
	else if (d > h)
	{ // Camera is above the top cap
		top = true;
		return true;
	}
	else
	{ // Camera cannot see either cap directly
		return false;
	}
}

struct LineIn
{
	float4 position : POSITION;
	float size : SIZE;
	float4 color : COLOR;
};

struct GeometryShaderOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

[maxvertexcount((SEGMENTS + 1) * 6)]
void main(line LineIn input[2], inout TriangleStream<GeometryShaderOutput> stream)
{
	LineIn vert0 = input[0];
	LineIn vert1 = input[1];
    
    // Calculate line parameters
	float3 pos0 = vert0.position.xyz;
	float3 pos1 = vert1.position.xyz;
	float3 line_dir = pos1 - pos0;
    
    // Skip degenerate lines
	float len = length(line_dir);
	if (len < 0.0001)
		return;
    
    // Calculate midpoint and camera vector
	float3 mid = (pos0 + pos1) * 0.5;
	float3 cam_to_mid = mid - cam_position.xyz;
    
    // Calculate perpendicular direction
	float3 line_dir_norm = normalize(line_dir);
	float3 cam_to_mid_norm = normalize(cam_to_mid);
	float3 perpendicular = cross(line_dir_norm, cam_to_mid_norm);
	   
    // Fallback if vectors are parallel
	if (length(perpendicular) < 0.0001)
	{
		perpendicular = cross(line_dir_norm, float3(0.0, 1.0, 0.0));
		if (length(perpendicular) < 0.0001)
			perpendicular = cross(line_dir_norm, float3(0.0, 0.0, 1.0));
	}
    
	// Calculate the 'breadth' vector for the cylinder
	float3 perpendicular0 = normalize(perpendicular) * vert0.size * 0.5;
	float3 perpendicular1 = normalize(perpendicular) * vert1.size * 0.5;
	
	// Calculate the 'up' vector for the cylinder
	float3 up0 = -normalize(cross(perpendicular0, line_dir_norm)) * vert0.size * 0.5;
	float3 up1 = -normalize(cross(perpendicular1, line_dir_norm)) * vert1.size * 0.5;
	
	// Transform to clip space
	float4 tPos0 = mul(float4(pos0, 1.0), view_projection);
	float4 tPos1 = mul(float4(pos1, 1.0), view_projection);
	float4 tPerp0 = mul(float4(perpendicular0, 0.0), view_projection);
	float4 tPerp1 = mul(float4(perpendicular1, 0.0), view_projection);
	float4 tUp0 = mul(float4(up0, 0.0), view_projection);
	float4 tUp1 = mul(float4(up1, 0.0), view_projection);
	
	// Check if a cylinder cap needs to be filled in
	bool topCap;
	bool drawCap = CanSeeCylinderCaps(pos0, line_dir_norm, len, cam_position.xyz, topCap);
		
	GeometryShaderOutput output;
		
	float4 prevPerp0 = tPerp0;
	float4 prevPerp1 = tPerp1;
	float4 prevUp0 = float4(0.0, 0.0, 0.0, 0.0);
	float4 prevUp1 = float4(0.0, 0.0, 0.0, 0.0);
	float4 prev0 = tPos0 + tPerp0;
	float4 prev1 = tPos1 + tPerp1;
	
	for (int i = 1; i < SEGMENTS - 3; i++)
	{
		// Calculate next points
		float t = (float(i) * PI * 2.0) / float(SEGMENTS);
		
		float4 nextPerp0 = (cos(t) * tPerp0);
		float4 nextPerp1 = (cos(t) * tPerp1);
		float4 nextUp0 = (sin(t) * tUp0);
		float4 nextUp1 = (sin(t) * tUp1);
		float4 next0 = tPos0 + nextPerp0 + nextUp0;
		float4 next1 = tPos1 + nextPerp1 + nextUp1;
		
		// Draw the quad
		output.position = prev0;
		output.color = vert0.color;
		stream.Append(output);
		
		output.position = next0;
		stream.Append(output);
		
		output.position = prev1;
		output.color = vert1.color;
		stream.Append(output);
		
		output.position = next1;
		stream.Append(output);
		
		stream.RestartStrip();
		
		// If camera is past the edge, draw the cap
		if (drawCap)
		{
			if (topCap)
			{ // Draw the top cap
				output.color = float4(vert1.color.xyz * 0.925, vert1.color.w);
				
				output.position = prev1;
				stream.Append(output);
				
				output.position = next1;
				stream.Append(output);
				
				output.position = prev1 - prevUp1 * 2.0;
				stream.Append(output);
				
				output.position = next1 - nextUp1 * 2.0;
				stream.Append(output);
				
				stream.RestartStrip();
			}
			else
			{ // Draw the bottom cap
				output.color = float4(vert0.color.xyz * 0.925, vert0.color.w);
				
				output.position = next0;
				stream.Append(output);
				
				output.position = prev0;
				stream.Append(output);
				
				output.position = next0 - nextUp0 * 2.0;
				stream.Append(output);
				
				output.position = prev0 - prevUp0 * 2.0;
				stream.Append(output);
				
				stream.RestartStrip();
			}
		}
		
		// Update previous points
		prevPerp0 = nextPerp0;
		prevPerp1 = nextPerp1;
		prevUp0 = nextUp0;
		prevUp1 = nextUp1;
		prev0 = next0;
		prev1 = next1;
	}
}