#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#include "WellEngine/Source/Shaders/Headers/LightData.hlsli"
#else
#include "Headers/Common.hlsli"
#include "Headers/LightData.hlsli"
#endif


cbuffer DistortionSettings : register(b2)
{
	float3 distortion_source;
	float distortion_distance;
};

cbuffer InverseCameraMatrixBuffer : register(b4)
{
	matrix inverseProjectionMatrix;
	matrix inverseViewMatrix;
};

cbuffer FogSettings : register(b6)
{
	float thickness;
	float stepSize;
	int minSteps;
	int maxSteps;
};

Texture2D<float> DepthTexture : register(t32);


float4 SampleLight(float3 pos, LightTile lightTile)
{
	float3 totalDiffuseLight = float3(0, 0, 0);
	
	// Get spotlight data
	uint spotlightCount = lightTile.spotlightCount;
    
	uint spotWidth, spotHeight, _;
	SpotShadowMaps.GetDimensions(0, spotWidth, spotHeight, _, _);

	const float
		spotDX = 1.0 / (float) spotWidth,
		spotDY = 1.0 / (float) spotHeight;
	
	// Per-spotlight calculations
	for (uint spotlight_i = 0; spotlight_i < spotlightCount; spotlight_i++)
	{
		// Prerequisite variables
		uint currentSpotlight = lightTile.spotlights[spotlight_i];
		const SpotLight light = SpotLights[currentSpotlight];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
        
		const float projectedDist = dot(light.direction, -toLight);
		float inverseLightDistSqr = light.orthographic > 0 ?
			(projectedDist > 0.0 ? 1.0 : 0.0) * (1.0 / (1.0 + pow(projectedDist, 2))) : // If orthographic
			1.0 / (1.0 + (pow(toLight.x * light.falloff, 2.0) + pow(toLight.y * light.falloff, 2.0) + pow(toLight.z * light.falloff, 2.0))); // If perspective
		
		CutoffLight(light.color.xyz, inverseLightDistSqr);
		
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		float offsetAngle;
		if (light.orthographic > 0)
		{
			offsetAngle = saturate(1.0f - length(cross(light.direction, toLight)) / (light.angle * 0.5f));
		}
		else
		{
			float dot_prod = dot(-toLightDir, light.direction);
			float dot_min = cos(light.angle * 0.5f);
			offsetAngle = saturate((dot_prod - dot_min) / (1.0f - dot_min));
			offsetAngle = pow(offsetAngle, 1.5);
		}
		
		if (offsetAngle <= 0.0)
			continue;
		
		// Calculate shadow projection
		const float4 fragPosLightClip = mul(float4(pos, 1.0f), light.vp_matrix);
		const float3 fragPosLightNDC = fragPosLightClip.xyz / fragPosLightClip.w;
		
		const bool isInsideFrustum = (
			fragPosLightNDC.x > -1.0 && fragPosLightNDC.x < 1.0 &&
			fragPosLightNDC.y > -1.0 && fragPosLightNDC.y < 1.0
		);
		
		if (!isInsideFrustum)
			continue;
		
		const float3 spotUV = float3(mad(fragPosLightNDC.x, 0.5, 0.5), mad(fragPosLightNDC.y, -0.5, 0.5), currentSpotlight);
		float shadowFactor = SpotShadowMaps.SampleCmpLevelZero(
			ShadowSampler,
			spotUV,
			fragPosLightNDC.z
		);
		const float shadow = isInsideFrustum * saturate(offsetAngle * shadowFactor);
		
		// Apply lighting
		totalDiffuseLight += light.color.xyz * shadow * inverseLightDistSqr * light.fogStrength;
	}
	
	
	// Get simple spotlight data
	uint simpleSpotlightCount = lightTile.simpleSpotlightCount;
	
	// Per-simple spotlight calculations
	for (uint simpleSpotlight_i = 0; simpleSpotlight_i < simpleSpotlightCount; simpleSpotlight_i++)
	{
		// Prerequisite variables
		uint currentSimpleSpotlight = lightTile.simpleSpotlights[simpleSpotlight_i];
		const SimpleSpotLight light = SimpleSpotLights[currentSimpleSpotlight];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		const float projectedDist = dot(light.direction, -toLight);
		float inverseLightDistSqr = light.orthographic > 0 ?
			(projectedDist > 0.0 ? 1.0 : 0.0) * (1.0 / (1.0 + pow(projectedDist, 2.0))) : // If orthographic
			1.0 / (1.0 + (pow(toLight.x * light.falloff, 2.0) + pow(toLight.y * light.falloff, 2.0) + pow(toLight.z * light.falloff, 2.0))); // If perspective
		
		CutoffLight(light.color.xyz, inverseLightDistSqr);
		
		if (inverseLightDistSqr <= 0.0)
			continue;
		
        //const float offsetAngle = saturate(1.0f - (
		//	light.orthographic > 0 ?
		//	length(cross(light.direction, toLight)) :
		//	acos(dot(-toLightDir, light.direction))
		//	) / (light.angle * 0.5f)
		//);
		float offsetAngle;
		if (light.orthographic > 0)
		{
			offsetAngle = saturate(1.0f - length(cross(light.direction, toLight)) / (light.angle * 0.5f));
		}
		else
		{
			float dot_prod = dot(-toLightDir, light.direction);
			float dot_min = cos(light.angle * 0.5f);
			offsetAngle = saturate((dot_prod - dot_min) / (1.0f - dot_min));
			offsetAngle = pow(offsetAngle, 1.5);
		}
		
		if (offsetAngle <= 0.0)
			continue;
		
		// Apply lighting
		totalDiffuseLight += light.color.xyz * saturate(offsetAngle) * inverseLightDistSqr * light.fogStrength;
	}
	
    
	// Get pointlight data
	uint pointlightCount = lightTile.pointlightCount;
	uint pointWidth, pointHeight;
	PointShadowMaps.GetDimensions(0, pointWidth, pointHeight, _, _);

	const float
		pointDX = 1.0f / (float) pointWidth,
		pointDY = 1.0f / (float) pointHeight;
	
	// Per-pointlight calculations
	for (uint pointlight_i = 0; pointlight_i < pointlightCount; pointlight_i++)
	{
		// Prerequisite variables
		uint currentPointlight = lightTile.pointlights[pointlight_i];
		const PointLight light = PointLights[currentPointlight];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		float inverseLightDistSqr = 1.0 / (1.0 + (
			pow(toLight.x * light.falloff, 2.0) +
			pow(toLight.y * light.falloff, 2.0) +
			pow(toLight.z * light.falloff, 2.0)
		));
		
		CutoffLight(light.color.xyz, inverseLightDistSqr);
		
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		// Calculate shadow projection
		float4 lightToPosDir = float4(toLightDir, currentPointlight);
		lightToPosDir.x *= -1.0;
		float lightDist = max(length(toLight), 0.0);
				
		float shadowFactor = PointShadowMaps.SampleCmpLevelZero(
			ShadowCubeSampler,
			lightToPosDir,
			1.0 - ((lightDist - light.nearZ) / (light.farZ - light.nearZ))
		);
		const float shadow = saturate(shadowFactor);
        
		// Apply lighting
		totalDiffuseLight += light.color.xyz * shadow * inverseLightDistSqr * light.fogStrength;
	}
	
	
	// Get simple pointlight data
	uint simplePointlightCount = lightTile.simplePointlightCount;
	
	// Per-simple pointlight calculations
	for (uint simplePointlight_i = 0; simplePointlight_i < simplePointlightCount; simplePointlight_i++)
	{
		// Prerequisite variables
		uint currentSimplePointlight = lightTile.simplePointlights[simplePointlight_i];
		const SimplePointLight light = SimplePointLights[currentSimplePointlight];

		const float3
			toLight = light.position - pos,
			toLightDir = normalize(toLight);
		
		float inverseLightDistSqr = 1.0 / (1.0 + (
			pow(toLight.x * light.falloff, 2.0) +
			pow(toLight.y * light.falloff, 2.0) +
			pow(toLight.z * light.falloff, 2.0)
		));
		
		CutoffLight(light.color.xyz, inverseLightDistSqr);
		
		if (inverseLightDistSqr <= 0.0)
			continue;
		
		// Apply lighting
		totalDiffuseLight += light.color.xyz * inverseLightDistSqr * light.fogStrength;
	}
	
	float density = max(totalDiffuseLight.x, max(totalDiffuseLight.y, totalDiffuseLight.z));
	
	return float4(totalDiffuseLight, density);
}

void RayStepping(
	float Length, float3 Start, float3 Dir, LightTile lightTile, // View params
	float thickness, float Step, float MinSteps, float MaxSteps, // Setting params
	out float3 Color, out float Density)
{
	Density = 1.0;
	MaxSteps = clamp(MaxSteps, 0.0, 1024.0); // Clamp to prevent harmful edge cases
	
	Step = (Length / MaxSteps);
	
	float3 samplePos = Start;
	float4 lightSample = SampleLight(samplePos, lightTile);

	float3 prevStepColor = lightSample.xyz;
	float prevStepDensity = lightSample.w;
	float prevLength = 0.0;
	
	float3 thisStepColor;
	float thisStepDensity;
	float thisLength = 0.0;
	
	float deltaStep = 0.0;
	float stepDensity = 0.0;
	
	uint seed = abs(randInt + (Length * 642616.742947315) + (Dir.x * -4657647.1751857) + (Dir.y * 133451.6456564) + (Dir.z * 5685732.4565677));
	
	float distortionStaticRadius = pow(RandomValue(seed), 6.0);
	float distortionDistInv = 1.0 / (distortion_distance * 0.2);
	float distortionStaticRadiusInv = 1.0 / (distortion_distance * 0.5 * distortionStaticRadius);
	
	int i = 0;
	int resampleRate = 1;
	for (float dist = Step; dist < Length; dist += Step)
	{
		float randOffset = (RandomValue(seed) - 0.5) * Step;
		
		thisLength = dist + randOffset;
		deltaStep = (thisLength - prevLength);
		
		samplePos = Start + thisLength * Dir;
		lightSample = SampleLight(samplePos, lightTile);
		
		thisStepColor = lightSample.xyz;
		thisStepDensity = lightSample.w;
		
		Color += (prevStepColor + thisStepColor) * 0.5 * deltaStep;
		stepDensity = (prevStepDensity + thisStepDensity) * 0.5 * deltaStep;
		Density *= exp(-stepDensity);
		
		prevStepColor = thisStepColor;
		prevStepDensity = thisStepDensity;
		prevLength = thisLength;
		
		// Sample Distortion
		float distToSource = length(samplePos - distortion_source);
		float distortion = Step * pow(1.0 - saturate(distToSource * distortionDistInv), 1.25);
		float dS = Step * pow(1.0 - saturate(distToSource * distortionStaticRadiusInv), 0.75);
		
		Color -= 0.1 * mad(0.1, distortion, 0.1 * dS);
		
		i++;
		if (i > MaxSteps) // Prevent infinite loop
			return;
	}
	
	deltaStep = (Length - prevLength);
	
	samplePos = mad(Length, Dir, Start);
	lightSample = SampleLight(samplePos, lightTile);
	thisStepColor = lightSample.xyz;
	thisStepDensity = lightSample.w;
	
	Color += (prevStepColor + thisStepColor) * 0.5 * deltaStep;
	stepDensity = (prevStepDensity + thisStepDensity) * 0.5 * deltaStep;
	Density *= exp(-stepDensity);
}


struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 tex_coord : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_Target0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	
	if (thickness == 0.0)
	{
		output.color = float4(0, 0, 0, 0);
		return output;
	}
	
	float2 uv = input.tex_coord;
	
	// HACK
	uv.x = Remap(uv.x, 0, 0.25, 0.0, 1.0);
	uv.y = Remap(uv.y, 0.75, 1.0, 0.0, 1.0);
	uv.y = 1.0 - uv.y;
	
	float depth = DepthTexture.Sample(Sampler, uv);
   
	float2 clipSpace = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 viewRay = float4(clipSpace, 1.0, 1.0);
	viewRay = mul(inverseProjectionMatrix, viewRay);
	viewRay /= viewRay.w;

	float3 worldDir = mul(inverseViewMatrix, float4(viewRay.xyz, 0.0)).xyz;
	worldDir = normalize(worldDir);
	    
	// Calculate light tile position
	const float4 screenPosClip = mul(float4(cam_position.xyz + worldDir, 1.0f), view_proj_matrix);
	const float3 screenPosNDC = screenPosClip.xyz / screenPosClip.w;
	const float2 screenUV = float2((screenPosNDC.x * 0.5f) + 0.5f, (screenPosNDC.y * 0.5f) + 0.5f);
			
	uint lightTiles, lightTileDim, _;
	LightTileBuffer.GetDimensions(lightTiles, _);
	lightTileDim = sqrt(lightTiles);
	
	uint2 lightTileCoord = uint2(screenUV * float(lightTileDim));
	int lightTileIndex = lightTileCoord.x + (lightTileCoord.y * lightTileDim);
	
	LightTile lightTile = LightTileBuffer[lightTileIndex];
	
	float3 currStep = cam_position.xyz;
	float3 stepDir = worldDir;
    
	float3 color = float3(0.0, 0.0, 0.0);
	float density = 0.0;
    
	RayStepping(
        depth, currStep, stepDir, lightTile, // View
        thickness, stepSize, minSteps, maxSteps, // Setting
        color, density // Output
    );
	
	color = max(0.0, color * 0.1);
	
	float transmittance = 1.0 - density;
	transmittance *= thickness;
	output.color = float4(color, transmittance);
	
	return output;
}

