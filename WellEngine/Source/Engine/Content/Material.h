#pragma once

typedef unsigned int UINT;

// Managed by the Content class. This will allow draw batching to be done much more cheaply.
struct Material
{
	UINT
		textureID = -1,		// Texture
		normalID = -1,		// Texture
		specularID = -1,	// Texture
		glossinessID = -1,  // Texture
		ambientID = -1,		// Texture
		occlusionID = -1, 	// Texture
		reflectiveID = -1, 	// Texture
		samplerID = -1, 	// Sampler
		vsID = -1, 			// Shader
		psID = -1;			// Shader

	Material() = default;
	Material(const Material &material) = default;
	Material(UINT texID) : textureID(texID) {}

	bool operator==(const Material &other) const
	{
		return (textureID == other.textureID) &&
			(normalID == other.normalID) &&
			(specularID == other.specularID) &&
			(glossinessID == other.glossinessID) &&
			(ambientID == other.ambientID) &&
			(occlusionID == other.occlusionID) &&
			(reflectiveID == other.reflectiveID) &&
			(samplerID == other.samplerID) &&
			(vsID == other.vsID) &&
			(psID == other.psID);
	}

	bool operator<(const Material &other) const
	{
		if (psID != other.psID)
			return psID < other.psID;

		if (vsID != other.vsID)
			return vsID < other.vsID;

		if (textureID != other.textureID)
			return textureID < other.textureID;

		if (normalID != other.normalID)
			return normalID < other.normalID;

		if (specularID != other.specularID)
			return specularID < other.specularID;

		if (ambientID != other.ambientID)
			return ambientID < other.ambientID;

		if (occlusionID != other.occlusionID)
			return occlusionID < other.occlusionID;

		if (glossinessID != other.glossinessID)
			return glossinessID < other.glossinessID;

		if (reflectiveID != other.reflectiveID)
			return reflectiveID < other.reflectiveID;

		return samplerID < other.samplerID;
	}
};