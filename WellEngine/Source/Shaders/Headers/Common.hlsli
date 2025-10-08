#ifdef RECOMPILE
#include "../WellEngine/Source/Shaders/Headers/Helpers.hlsli"
#else
#include "Helpers.hlsli"
#endif

static const float SHADOW_DEPTH_EPSILON = 0.0;
static const float SHADOW_NORMAL_OFFSET = 0.0005;
static const float SHADOWCUBE_DEPTH_EPSILON = 0.01;
static const float SHADOWCUBE_NORMAL_OFFSET = 0.0035;


sampler Sampler : register(s0);
sampler EnvSampler : register(s3);

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

// Generic color-clamping algorithm
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

float3 ApplyRenderDistanceFog(float3 color, float zDepth)
{
	float linearDepth = LinearizeDepth(zDepth);
	float remappedDepth = max(0.0, Remap(linearDepth, fadeoutDepthBegin, 1.0, 0.0, 1.0));
	
	float3 result = lerp(color, fadeoutColor.rgb, saturate(pow(remappedDepth, fadeoutExponent) * fadeoutColor.a));
	return result;
}
