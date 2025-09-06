
Texture2D<float4> Texture		: register(t0);
Texture2D<float4> NormalMap		: register(t1);
Texture2D<float4> SpecularMap	: register(t2);
Texture2D<float1> ReflectiveMap : register(t3);
Texture2D<float4> AmbientMap	: register(t4);
Texture2D<float1> OcclusionMap	: register(t8);
Texture2D<float1> GlossinessMap : register(t9);

cbuffer SpecularBuffer : register(b1)
{
	float SpecBuf_specularExponent;
	float _SpecBuf_padding[3];
};

cbuffer MaterialProperties : register(b2)
{
	float4 MatProp_baseColor;
	float MatProp_alphaCutoff;
    float MatProp_specularFactor;
	float MatProp_normalFactor;
	float MatProp_glossFactor;
    float MatProp_occlusionFactor;
    float MatProp_reflectFactor;
    float MatProp_metallic;
	uint MatProp_sampleFlags;
	
	//float _MatProp_padding[1];
};

void GetSampleFlags(
	out bool sampleNormal, out bool sampleSpecular, out bool sampleGlossiness,
	out bool sampleReflective, out bool sampleAmbient, out bool sampleOcclusion)
{
	sampleNormal		= (MatProp_sampleFlags & 1) > 0;
	sampleSpecular		= (MatProp_sampleFlags & 2) > 0;
	sampleGlossiness	= (MatProp_sampleFlags & 4) > 0;
	sampleReflective	= (MatProp_sampleFlags & 8) > 0;
	sampleAmbient		= (MatProp_sampleFlags & 16) > 0;
	sampleOcclusion		= (MatProp_sampleFlags & 32) > 0;
}
