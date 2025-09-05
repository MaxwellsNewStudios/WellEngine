
Texture2D Texture : register(t0);
Texture2D NormalMap : register(t1);
Texture2D SpecularMap : register(t2);
Texture2D ReflectiveMap : register(t3);
Texture2D AmbientMap : register(t4);
Texture2D OcclusionMap : register(t8);
Texture2D GlossinessMap : register(t9);

cbuffer SpecularBuffer : register(b1)
{
	float specularExponent;
	float _specularBufferPadding[3];
};

cbuffer MaterialProperties : register(b2)
{
	int sampleNormal;		// Use normal map if greater than zero.
	int sampleSpecular;		// Use specular map if greater than zero.
	int sampleGlossiness;	// Use glossiness map if greater than zero.
	int sampleReflective;	// Use reflective map if greater than zero.
	int sampleAmbient;		// Use ambient map if greater than zero.
    int sampleOcclusion;	// Use occlusion map if greater than zero.
	
    float alphaCutoff;
    float specularFactor;
	float4 baseColor;
    float metallic;
    float reflectivity;
	
	float _materialPropertiesPadding[2];
};