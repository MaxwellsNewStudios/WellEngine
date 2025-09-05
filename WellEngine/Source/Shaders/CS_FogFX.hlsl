#ifdef RECOMPILE
#include "Litet-Spelprojekt/Content/Shaders/Headers/Common.hlsli"
#include "Litet-Spelprojekt/Content/Shaders/Headers/LightData.hlsli"
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


Texture2D<float> Input : register(t0);

RWTexture2D<float4> Output : register(u0);

float4 SampleLight(float3 pos, LightTile lightTile)
{
	float3 totalDiffuseLight = float3(0, 0, 0);
	
	// Get spotlight data
	uint spotlightCount = lightTile.spotlightCount;
    
    uint spotWidth, spotHeight, _;
    SpotShadowMaps.GetDimensions(0, spotWidth, spotHeight, _, _);

	const float
		spotDX = 1.0 / (float)spotWidth,
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
		
		// Calculate shadow projection
		const float4 fragPosLightClip = mul(float4(pos, 1.0f), light.vp_matrix);
		const float3 fragPosLightNDC = fragPosLightClip.xyz / fragPosLightClip.w;
		
		const bool isInsideFrustum = (
			fragPosLightNDC.x > -1.0 && fragPosLightNDC.x < 1.0 &&
			fragPosLightNDC.y > -1.0 && fragPosLightNDC.y < 1.0
		);
		
		if (!isInsideFrustum)
			continue;
		
		/*const float3 spotUV = float3((fragPosLightNDC.x * 0.5) + 0.5, (fragPosLightNDC.y * -0.5) + 0.5, currentSpotlight);
		const float spotDepth = SpotShadowMaps.SampleLevel(ShadowCubeSampler, spotUV, 0);
		const float spotResult = spotDepth < fragPosLightNDC.z ? 1.0 : 0.0;
		
		const float shadow = isInsideFrustum * saturate(offsetAngle * spotResult);*/
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
		pointDX = 1.0f / (float)pointWidth,
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
		float lightDist = max(length(toLight), 0.0);
		float4 lightToPosDir = float4(toLightDir, currentPointlight);
		lightToPosDir.x *= -1.0;
				
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
	
	//float density = pow(length(totalDiffuseLight), 0.75) * 0.1;
	//float density = (totalDiffuseLight.x + totalDiffuseLight.y + totalDiffuseLight.z) * 0.01;
	//float density = RGBtoHSV(totalDiffuseLight).z;
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
	
	// Traverse path backwards for better accumulation
	//Start = Start + (Dir * Length);
	//Dir = -Dir;
	
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
		//Density = mad(0.1, mad(6.0, distortion, 12.0 * dS), Density);
		
		//dist *= mad(0.075, RandomValue(seed), 1.0);
		//dist = mad(0.0075 * Density * Step, abs(RandomValue(seed)), dist);
		
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
	//Density += (prevStepDensity + thisStepDensity) * 0.5 * deltaStep;
	stepDensity = (prevStepDensity + thisStepDensity) * 0.5 * deltaStep;
	Density *= exp(-stepDensity);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	if (thickness <= 0.0)
	{
		Output[DTid.xy] = float4(0, 0, 0, 0);
		return;
	}
	
	int2 inDim, outDim;
	Input.GetDimensions(inDim.x, inDim.y);
	Output.GetDimensions(outDim.x, outDim.y);
    
	float2 uv = (float2(DTid.xy) + float2(0.5, 0.5)) / float2(outDim);
	
	float2 inPixelSize = float2(outDim.xy) / float2(inDim.xy);
	float2 uvPixelSize = inPixelSize / float2(outDim);
	
	float
		depth00 = Input.SampleLevel(Sampler, uv + float2(-uvPixelSize.x, -uvPixelSize.y), 0),
		depth01 = Input.SampleLevel(Sampler, uv + float2(uvPixelSize.x, -uvPixelSize.y), 0),
		depth10 = Input.SampleLevel(Sampler, uv + float2(-uvPixelSize.x, uvPixelSize.y), 0),
		depth11 = Input.SampleLevel(Sampler, uv + float2(uvPixelSize.x, uvPixelSize.y), 0);
    
	float depth = (depth00 + depth01 + depth10 + depth11) / 4.0;
	//float depth = Input.SampleLevel(Sampler, uv, 0);
    
	float2 clipSpace = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float4 viewRay = float4(clipSpace, 1.0, 1.0);
	viewRay = mul(inverseProjectionMatrix, viewRay);
	viewRay /= viewRay.w;

	float3 rayWorldDir = mul(inverseViewMatrix, float4(viewRay.xyz, 0.0)).xyz;
	rayWorldDir = normalize(rayWorldDir);
	
    
	// Calculate light tile position
	const float4 screenPosClip = mul(float4(cam_position.xyz + rayWorldDir, 1.0f), view_proj_matrix);
	const float3 screenPosNDC = screenPosClip.xyz / screenPosClip.w;
	const float2 screenUV = float2((screenPosNDC.x * 0.5f) + 0.5f, (screenPosNDC.y * 0.5f) + 0.5f);
		
	uint lightTiles, lightTileDim, _;
	LightTileBuffer.GetDimensions(lightTiles, _);
	lightTileDim = sqrt(lightTiles);
	
	uint2 lightTileCoord = uint2(screenUV * float(lightTileDim));
	int lightTileIndex = lightTileCoord.x + (lightTileCoord.y * lightTileDim);
	
	LightTile lightTile = LightTileBuffer[lightTileIndex];
	
	float3 currStep = cam_position.xyz;
	float3 stepDir = rayWorldDir;
    
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
	
	Output[DTid.xy] = float4(color, transmittance);
}