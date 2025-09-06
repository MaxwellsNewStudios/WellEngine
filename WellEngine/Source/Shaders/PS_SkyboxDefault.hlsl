#ifdef RECOMPILE
#include "WellEngine/Source/Shaders/Headers/Common.hlsli"
#else
#include "Headers/Common.hlsli"
#endif

struct PixelShaderInput
{
	float4 position : SV_POSITION;
	float2 tex_coord : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_Target0; // w is emissiveness
	float depth : SV_Target1; // in world units
	float3 emission : SV_Target2;
};


// Simple saturation function
float3 AdjustSaturation(float3 color, float sat)
{
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
	return lerp(float3(luminance, luminance, luminance), color, sat);
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	
	float2 ndc = input.tex_coord * 2.0 - 1.0;
	float4 clipPos = float4(ndc, 1.0, 1.0);

	float4 worldPosH = mul(clipPos, inv_view_proj_matrix);
	worldPosH /= worldPosH.w;

	float3 worldDir = normalize(worldPosH.xyz - cam_position.xyz);
	
	// Simple atmospheric gradient based on vertical direction
	if (true)
	{
		float3	sun_direction = normalize(float3(6.78, 10.0, -3.53)); // normalized direction of the sun
		float	sun_size = 4.0; // angular size of the sun disk
		float3	sun_color = float3(1.0, 0.9, 0.67); // color of the sun
		float	sun_intensity = 1.5; // brightness factor
		float	halo_intensity = 0.1; // brightness of the halo
		float	halo_falloff = 0.95; // controls the halo spread
		float	saturation = 1.3; // extra saturation boost (e.g., 1.1 - 1.3)
		
		// --- Sky gradient ---
		float t = saturate(worldDir.y * 0.5f + 0.5f);
    
		float3 zenithColor = float3(0.15f, 0.35f, 0.8f);
		float3 horizonColor = float3(0.6f, 0.8f, 0.9f);
    
		float3 skyColor = lerp(horizonColor, zenithColor, t);

		// --- Sun disk ---
		float sunAngularSize = cos(radians(sun_size));
		float sunDot = dot(worldDir, normalize(sun_direction));
		float sunMask = smoothstep(sunAngularSize, sunAngularSize + 0.002f, sunDot);
		float3 sunLight = sun_color * sun_intensity * sunMask;

		// --- Sun halo ---
		float halo = exp((sunDot - 1.0f) * halo_falloff);
		halo = saturate(halo) * halo_intensity;
		float3 haloLight = sun_color * halo;

		// --- Combine sky, sun, and halo ---
		float3 finalColor = skyColor + sunLight + haloLight;

		// --- Adjust saturation ---
		finalColor = AdjustSaturation(finalColor, saturation);

		// --- Apply ACES tone mapping ---
		finalColor = ACESFilm(finalColor);

		output.color = float4(finalColor, 1.0f);
		output.emission = ACESFilm(sunLight + haloLight); // emission: only sun + halo
	}
	
	output.depth = 0;
	return output;
}
