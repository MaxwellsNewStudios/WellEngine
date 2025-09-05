
static const float PI = 3.14159265;
static const float SHADOW_DEPTH_EPSILON = 0.0;
static const float SHADOW_NORMAL_OFFSET = 0.0005;
static const float SHADOWCUBE_DEPTH_EPSILON = 0.01; // 0.01
static const float SHADOWCUBE_NORMAL_OFFSET = 0.0035; // 0.0035

sampler Sampler : register(s0);
Texture2D NoiseTexture : register(t10);

TextureCube EnvironmentCubemap : register(t20);

cbuffer GlobalLight : register(b0)
{
	float4 ambient_light; // Use alpha channel for screen fade-out
};

cbuffer CameraData : register(b3)
{
	matrix proj_matrix;
	matrix view_proj_matrix;
	matrix inv_view_proj_matrix;
	matrix inv_proj_matrix;
	float4 cam_position;
	float4 cam_direction;
	float2 cam_planes;
	
	float _cameraData_padding[2];
};

cbuffer GeneralData : register(b5)
{
	float time;
	float deltaTime;
	int randInt;
	float randNorm;
	
	// Distance based fade-out
	float4 fadeoutColor;
	float fadeoutDepthBegin;
	float fadeoutExponent;
	
	float _generalData_padding[2];
};


uint NextRandom(inout uint state)
{
	state = state * 747796405u + 2891336453u;
	uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
	result = (result >> 22) ^ result;
	return result;
}

float RandomValue(inout uint state)
{
	return float(NextRandom(state)) / 4294967295.0;
}

// Generic color-clamping algorithm, not mine but it looks good
float3 ACESFilm(const float3 x)
{
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float DistributionGGX(float3 normal, float3 halfway, float roughness)
{
	float a2 = roughness * roughness;
	float NdotH = max(dot(normal, halfway), 0.0);
	float NdotH2 = NdotH * NdotH;
	
	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return nom / denom;
}

void BlinnPhong(float3 toLightDir, float3 viewDir, float3 normal, float3 lightCol, float specularity, out float3 diffuse, out float3 specular, in bool fallbackReflectionModel = false)
{
	const float3 halfwayDir = normalize(toLightDir + viewDir);
		
	float directionScalar = max(dot(normal, toLightDir), 0.0);
	diffuse = lightCol * directionScalar;
	
	float specFactor;
	if (fallbackReflectionModel)
		specFactor = pow(max(dot(normal, halfwayDir), 0.0), specularity);
	else
		specFactor = DistributionGGX(normal, halfwayDir, 1.0 - specularity);
	
	specular = lightCol * directionScalar * smoothstep(0.0, 1.0, specFactor);
}

float LinearizeDepth(float zDepth)
{
	return proj_matrix._34 / (zDepth - proj_matrix._33) / (cam_planes.x - cam_planes.y);
}

// Convert depth from [0, 1] to [near, far]
float WorldDepth(float depthNDC)
{
	float linearDepth = LinearizeDepth(depthNDC);
	float near = cam_planes.x;
	float far = cam_planes.y;
	return near + linearDepth * (far - near);
}

float3 UVWToWorld(float3 uvw)
{
    // Assumes uvw.xy are screen-space UVs in [0,1], uvw.z is depth in [0,1]
    // Reconstruct NDC position
    float2 ndc = uvw.xy * 2.0 - 1.0;
    float ndcDepth = uvw.z;

    // Reconstruct clip space position
    float4 clipPos = float4(ndc, ndcDepth, 1.0);

    // Inverse project to world space
    float4 worldPos = mul(clipPos, inv_proj_matrix);
    worldPos /= worldPos.w;

	return cam_position.xyz + worldPos.xyz;
}

float3 RGBtoHSV(float3 rgb)
{
	float
		r = rgb.x,
		g = rgb.y,
		b = rgb.z;

	float maxC = max(r, max(g, b));
	float minC = min(r, min(g, b));
	float diff = maxC - minC;
	float invDiff = 1.0 / diff;

	float h = 0.0, s, v;

	if (maxC == minC)
		h = 0.0;
	else if (maxC == r)
		h = fmod((60.0 * ((g - b) / invDiff) + 360.0), 360.0);
	else if (maxC == g)
		h = fmod((60.0 * ((b - r) / invDiff) + 120.0), 360.0);
	else if (maxC == b)
		h = fmod((60.0 * ((r - g) / invDiff) + 240.0), 360.0);
	else
		h = 0.0;

	s = (maxC == 0.0) ? (0.0) : ((diff / maxC) * 1.0);
	v = maxC;

	return float3(h, s, v);
}
float3 HSVtoRGB(float3 hsv)
{
	float
		r = 0.0,
		g = 0.0,
		b = 0.0;

	if (hsv.y == 0.0)
	{
		r = hsv.z;
		g = hsv.z;
		b = hsv.z;
	}
	else
	{
		int i;
		float f, p, q, t;

		if (hsv.x == 360.0)
			hsv.x = 0.0;
		else
			hsv.x = hsv.x * 0.016666666666666666667;

		i = (int) trunc(hsv.x);
		f = hsv.x - i;

		p = hsv.z * (1.0 - hsv.y);
		q = hsv.z * (1.0 - (hsv.y * f));
		t = hsv.z * (1.0 - (hsv.y * (1.0 - f)));

		if (i == 0)
		{
			r = hsv.z;
			g = t;
			b = p;
		}
		else if (i == 1)
		{
			r = q;
			g = hsv.z;
			b = p;
		}
		else if (i == 2)
		{
			r = p;
			g = hsv.z;
			b = t;
		}
		else if (i == 3)
		{
			r = p;
			g = q;
			b = hsv.z;
		}
		else if (i == 4)
		{
			r = t;
			g = p;
			b = hsv.z;
		}
		else
		{
			r = hsv.z;
			g = p;
			b = q;
		}
	}

	return float3(r, g, b);
}

float GetUnprojectedLength(float3 dir, float3 projectOnto, float lengthAlongProjection)
{
	float3 unitProjectionDir = normalize(projectOnto);
	float dotProduct = dot(dir, unitProjectionDir);

    // Avoid division by zero
	if (abs(dotProduct) < 0.000001)
		return 0.0;
	
	return lengthAlongProjection / dotProduct;
}

// Framerate independent exponential decay function
// a: start value
// b: end value
// d: decay rate (common range: 1 - 25)
// dT: delta time
float ExpDecay(float a, float b, float d, float dT)
{
	return (b + (a - b) * exp(-d * dT));
}

float1 Remap(float1 value, float1 inMin, float1 inMax, float1 outMin, float1 outMax)
{
	if (inMin == inMax)
		return outMin; // Avoid division by zero
	
	float1 t = (value - inMin) / (inMax - inMin);
	float1 result = outMin + t * (outMax - outMin);
	
	return result;
}
float2 Remap(float2 value, float2 inMin, float2 inMax, float2 outMin, float2 outMax)
{
	float2 t = (value - inMin) / (inMax - inMin);
	float2 result = outMin + t * (outMax - outMin);
	
	result = ((inMin == inMax) * outMin) + ((inMin != inMax) * result); // Avoid division by zero
	
	return result;
}
float3 Remap(float3 value, float3 inMin, float3 inMax, float3 outMin, float3 outMax)
{
	float3 t = (value - inMin) / (inMax - inMin);
	float3 result = outMin + t * (outMax - outMin);
	
	result = ((inMin == inMax) * outMin) + ((inMin != inMax) * result); // Avoid division by zero
	
	return result;
}
float4 Remap(float4 value, float4 inMin, float4 inMax, float4 outMin, float4 outMax)
{
	float4 t = (value - inMin) / (inMax - inMin);
	float4 result = outMin + t * (outMax - outMin);
	
	result = ((inMin == inMax) * outMin) + ((inMin != inMax) * result); // Avoid division by zero
	
	return result;
}

float3 ApplyRenderDistanceFog(float3 color, float zDepth)
{
	float linearDepth = LinearizeDepth(zDepth);
	float remappedDepth = max(0.0, Remap(linearDepth, fadeoutDepthBegin, 1.0, 0.0, 1.0));
	
	float3 result = lerp(color, fadeoutColor.rgb, saturate(pow(remappedDepth, fadeoutExponent) * fadeoutColor.a));
	return result;
}
