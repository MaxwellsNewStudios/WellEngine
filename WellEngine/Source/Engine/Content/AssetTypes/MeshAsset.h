#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "../Asset.h"
#include "Source/Engine/D3D/MeshD3D11.h"

using Microsoft::WRL::ComPtr;


class MeshAsset : public Asset
{
	private:
		ComPtr<MeshD3D11> _mesh = nullptr;
};
