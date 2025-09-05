#pragma once
#undef NDEBUG

#include "../Tests/TestUtils.h"

#include "Engine/EngineSettings.h"

#include "Debug/TrackedAlloc.h"
#include "Debug/DebugNew.h"

#define _USE_MATH_DEFINES

// C++ Standard Library
#include <vector>
#include <algorithm>
#include <queue>
#include <memory>
#include <iostream>
#include <fstream>
#include <variant>
#include <ctime>
#include <filesystem>
#include <cstdlib>
#include <functional>
#include <Windows.h>
#include <atomic>
#include <thread>
#include <execution>
#include <mutex>
#include <semaphore>
#include <omp.h>
#include <cassert>
#include <random>

// DirectX & SDL
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <DirectXTex.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <SDL3/SDL.h>
#include <winsdkver.h>
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <Audio.h>

namespace dx = DirectX;
using Microsoft::WRL::ComPtr;

// External Libraries
#ifdef USE_IMGUI
#include "tinyfiledialogs/tinyfiledialogs.h"
#include "ImGui/imconfig.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_sdl3.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_stdlib.h"
#include "ImPlot/implot.h"
#endif
#ifdef USE_IMGUIZMO
#include "ImGui/ImGuizmo.h"
#include "ImGui/ImSequencer.h"
#include "ImGui/ImZoomSlider.h"
#include "ImGui/ImCurveEdit.h"
#include "ImGui/GraphEditor.h"
#endif

#include "tracy-0.11.1/public/tracy/Tracy.hpp"
#include "tracy-0.11.1/public/tracy/TracyD3D11.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
namespace json = rapidjson;

// Engine
#include "Utils/UIDHelper.h"
#include "Utils/ReferenceHelper.h"
#include "Utils/StringUtils.h"
#include "Utils/SerializerUtils.h"
#include "Transform.h"
#include "Math/GameMath.h"
#include "Math/ConstRand.h"
#include "Content/Material.h"
#include "Rendering/RendererInfo.h"
#include "Rendering/RenderQueuer.h"
#include "Window/Window.h"
#include "Timing/TimeUtils.h"
#include "Input/Input.h"
#include "Input/InputBindings.h"
#include "Collision/ColliderShapes.h"
#include "Debug/ErrMsg.h"
#ifdef USE_IMGUI
#include "UI/UIDragDropHelpers.h"
#include "UI/ImGuiUtils.h"
#endif

#include "D3D/D3D11Helper.h"
#include "D3D/D3D11FormatData.h"
#include "D3D/ConstantBufferD3D11.h"
#include "D3D/DepthBufferD3D11.h"
#include "D3D/IndexBufferD3D11.h"
#include "D3D/InputLayoutD3D11.h"
#include "D3D/MeshD3D11.h"
#include "D3D/RenderTargetD3D11.h"
#include "D3D/SamplerD3D11.h"
#include "D3D/ShaderD3D11.h"
#include "D3D/ShaderResourceTextureD3D11.h"
#include "D3D/SimpleMeshD3D11.h"
#include "D3D/StructuredBufferD3D11.h"
#include "D3D/SubMeshD3D11.h"
#include "D3D/VertexBufferD3D11.h"


#define COUT_USAGE_WARNING abort(); static_assert(false, "Use Warn() or DbgMsg() instead of std::cout. They exist for a reason.")
#define CERR_USAGE_WARNING abort(); static_assert(false, "Use Warn() or ErrMsg() instead of std::cerr. They exist for a reason.")
#define cout COUT_USAGE_WARNING
#define cerr CERR_USAGE_WARNING
