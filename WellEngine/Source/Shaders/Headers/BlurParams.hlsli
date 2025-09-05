static const float SAMPLE_STEP = 0.001f;

StructuredBuffer<float> GaussianWeights : register(t3);

Texture2D<float> Depth : register(t1);

Texture2D Input : register(t0);


float4 GaussianBlur(in int2 uv, in int2 direction, in int2 dimensions, in bool useDepth)
{
	float4 sum = 0.0;
	
	uint weightCount, _;
	GaussianWeights.GetDimensions(weightCount, _);
	
	if (useDepth)
	{
		float weightSum = 0.0;
		
		int2 depthDimensions;
		Depth.GetDimensions(depthDimensions.x, depthDimensions.y);
	
		float2 inputToDepth = float2(depthDimensions) / float2(dimensions);
		float depth = Depth[uv * inputToDepth];
	
		for (int i = -int(weightCount) + 1; i < int(weightCount); ++i)
		{
			int2 sampleUV = uv + i * direction;
			sampleUV = clamp(sampleUV, int2(0, 0), dimensions - 1);
		
			float sampleDepth = Depth[sampleUV * inputToDepth];
			float offset = abs(sampleDepth - depth);
		
			float weight = GaussianWeights[abs(i)] * lerp(1.0 / max(1.0, offset), 0.3, 1.0);
		
			weightSum += weight;
			sum += weight * Input[sampleUV];
		}
		
		sum /= weightSum;
	}
	else
	{	
		for (int i = -int(weightCount) + 1; i < int(weightCount); ++i)
		{
			int2 sampleUV = uv + i * direction;
			sampleUV = clamp(sampleUV, int2(0, 0), dimensions - 1);
				
			float weight = GaussianWeights[abs(i)];
		
			sum += weight * Input[sampleUV];
		}
	}
	
	return sum;
}

// Deprecated
float4 LinearBlur(int2 uv, int kernelSize, int2 direction, int2 dimensions)
{
	float4 sum = 0.0;
	
	for (int i = -kernelSize; i <= kernelSize; ++i)
	{
		int2 sampleUV = uv + i * direction;
		sampleUV = clamp(sampleUV, int2(0, 0), dimensions - 1);
		
		sum += Input[sampleUV];
	}
	
	return sum / float(2 * kernelSize + 1);
}
