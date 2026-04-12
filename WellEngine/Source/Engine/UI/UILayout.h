#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#ifdef USE_IMGUI
class UILayout
{
public:
	static void GetLayoutNames(std::vector<std::string> &layouts);

	static void SaveLayout(const std::string &name);
	static bool LoadLayout(const std::string &name);
};
#endif // USE_IMGUI
