#pragma once

#include "Engine/Content/Material.h"

namespace WellEngine
{

	struct RendererInfo
	{
		bool includeTransparent = true;
		bool shadowCamera = false; 
	};

	struct ResourceGroup
	{
		const Material *material = nullptr;
		UINT meshID = CONTENT_NULL;
		UINT blendStateID = CONTENT_NULL;
		bool shadowCaster = true;
		bool overlay = false;
		bool shadowsOnly = false;

		ResourceGroup(UINT meshID, const Material *material, bool shadowCaster, bool overlay, bool shadowsOnly = false, UINT blendStateID = CONTENT_NULL) :
			meshID(meshID), material(material), shadowCaster(shadowCaster), overlay(overlay), shadowsOnly(shadowsOnly), blendStateID(blendStateID) {
		}

		bool operator==(const ResourceGroup &other) const
		{
			return (material == other.material) && (meshID == other.meshID);
		}
		bool operator<(const ResourceGroup &other) const
		{
			if (material != other.material)
				return (*material) < (*other.material);

			return meshID < other.meshID;
		}
	};
}
