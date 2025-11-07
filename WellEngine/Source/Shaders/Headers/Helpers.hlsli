
static const float PI = 3.14159265;

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
	{
		return outMin; // Avoid division by zero
	}
	else
	{
		float1 t = (value - inMin) / (inMax - inMin);
		return outMin + t * (outMax - outMin);
	}
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

bool EqualEst(float a, float b, float epsilon)
{
	return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * epsilon;
}