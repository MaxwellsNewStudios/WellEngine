
SamplerComparisonState ShadowSampler : register(s1);
SamplerComparisonState ShadowCubeSampler : register(s2);
sampler HQSampler : register(s4);

struct SpotLight
{
	float4x4 vp_matrix;
	
	float3 position;
	float angle;
	
	float3 direction;
	float falloff;
	
	float3 color;
	int orthographic;
	
	float fogStrength;
	float padding[3];
};
StructuredBuffer<SpotLight> SpotLights : register(t11);
Texture2DArray<float> SpotShadowMaps : register(t5);

struct SimpleSpotLight
{
	float3 position;
	float angle;
	
	float3 direction;
	float falloff;
	
	float3 color;
	int orthographic;
	
	float fogStrength;
	float padding[3];
};
StructuredBuffer<SimpleSpotLight> SimpleSpotLights : register(t12);

struct PointLight
{
	float3 position;
	float falloff;
	
	float3 color;
	float fogStrength;
	
	float nearZ;
	float farZ;
	
	float padding[2];
};
StructuredBuffer<PointLight> PointLights : register(t6);
TextureCubeArray<float> PointShadowMaps : register(t7);

struct SimplePointLight
{
	float3 position;
	float falloff;
	
	float3 color;
	float fogStrength;
};
StructuredBuffer<SimplePointLight> SimplePointLights : register(t13);

static const uint MAX_LIGHTS = 7;
struct LightTile
{
	uint spotlights[MAX_LIGHTS];
	uint spotlightCount;
	
	uint pointlights[MAX_LIGHTS];
	uint pointlightCount;
	
	uint simpleSpotlights[MAX_LIGHTS];
	uint simpleSpotlightCount;
	
	uint simplePointlights[MAX_LIGHTS];
	uint simplePointlightCount;
};

StructuredBuffer<LightTile> LightTileBuffer : register(t14);

/// Adds a smoothed cutoff point to lights, beyond which their intensity becomes zero.
/// This is done to prevent harsh edges between light tiles.
/// This cutoff is matched by lightBehaviours using CalculateLightReach() in GameMath.h.
void CutoffLight(float3 color, inout float invSquare)
{
	// The intensity at which the light is considered to have no effect.
	// Must match the value of the same name in CalculateLightReach().
	float intensityCutoff = 0.02;
	
	float strength = max(color.x, max(color.y, color.z));
	float correction = strength / (strength - intensityCutoff);
	
	float intensity = strength * invSquare;
	
	intensity = max(0.0, correction * (intensity - intensityCutoff));
	
	invSquare = intensity / strength;
}

