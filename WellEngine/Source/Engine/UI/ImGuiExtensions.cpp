#include "stdafx.h"
#include "ImGuiExtensions.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace ImGui;

void ImDrawCallback_ImplDX11_SetSampler(const ImDrawList *parent_list, const ImDrawCmd *cmd)
{
	ImGui_ImplDX11_RenderState *state = (ImGui_ImplDX11_RenderState *)GetPlatformIO().Renderer_RenderState;
	ID3D11SamplerState *sampler = cmd->UserCallbackData ? (ID3D11SamplerState *)cmd->UserCallbackData : state->SamplerDefault;
	state->DeviceContext->PSSetSamplers(0, 1, &sampler);
}
