#pragma once
#undef NDEBUG

#include "Tests/TestUtils.h"

#include "Source/Engine/EngineSettings.h"
#include "Source/Engine/Debug/TrackedAlloc.h"
#include "Source/Engine/Debug/DebugNew.h"

#define _USE_MATH_DEFINES

// C++ Standard Library
#include <vector>
#include <algorithm>
#include <queue>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <variant>
#include <ctime>
#include <cmath>
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
#include <float.h>
#include <intsafe.h>

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

// Dependencies
#ifdef USE_IMGUI
#include "Dependencies/tinyfiledialogs/tinyfiledialogs.h"
#include "Dependencies/ImGui/imconfig.h"
#include "Dependencies/ImGui/imgui.h"
#include "Dependencies/ImGui/imgui_impl_sdl3.h"
#include "Dependencies/ImGui/imgui_impl_dx11.h"
#include "Dependencies/ImGui/imgui_stdlib.h"
#include "Dependencies/ImPlot/implot.h"
#endif

#ifdef USE_IMGUIZMO
#include "Dependencies/ImGui/ImGuizmo.h"
#include "Dependencies/ImGui/ImSequencer.h"
#include "Dependencies/ImGui/ImZoomSlider.h"
#include "Dependencies/ImGui/ImCurveEdit.h"
#include "Dependencies/ImGui/GraphEditor.h"
#endif

#include "Dependencies/tracy-0.11.1/public/tracy/Tracy.hpp"
#include "Dependencies/tracy-0.11.1/public/tracy/TracyD3D11.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
namespace json = rapidjson;


// Math
#include "Source/Math/GameMath.h"
#include "Source/Math/ConstRand.h"

// Engine
#include "Source/Engine/Utils/UIDHelper.h"
#include "Source/Engine/Utils/ReferenceHelper.h"
#include "Source/Engine/Utils/StringUtils.h"
#include "Source/Engine/Utils/SerializerUtils.h"
#include "Source/Engine/Content/Material.h"
#include "Source/Engine/Rendering/RendererInfo.h"
#include "Source/Engine/Rendering/RenderQueuer.h"
#include "Source/Engine/Window/Window.h"
#include "Source/Engine/Timing/TimeUtils.h"
#include "Source/Engine/Input/Input.h"
#include "Source/Engine/Input/InputBindings.h"
#include "Source/Engine/Collision/ColliderShapes.h"
#include "Source/Engine/Collision/Raycast.h"
#include "Source/Engine/Debug/ErrMsg.h"

#include "Source/Engine/D3D/D3D11Helper.h"
#include "Source/Engine/D3D/D3D11FormatData.h"
#include "Source/Engine/D3D/ConstantBufferD3D11.h"
#include "Source/Engine/D3D/DepthBufferD3D11.h"
#include "Source/Engine/D3D/IndexBufferD3D11.h"
#include "Source/Engine/D3D/InputLayoutD3D11.h"
#include "Source/Engine/D3D/MeshD3D11.h"
#include "Source/Engine/D3D/RenderTargetD3D11.h"
#include "Source/Engine/D3D/SamplerD3D11.h"
#include "Source/Engine/D3D/ShaderD3D11.h"
#include "Source/Engine/D3D/ShaderResourceTextureD3D11.h"
#include "Source/Engine/D3D/SimpleMeshD3D11.h"
#include "Source/Engine/D3D/StructuredBufferD3D11.h"
#include "Source/Engine/D3D/SubMeshD3D11.h"
#include "Source/Engine/D3D/VertexBufferD3D11.h"

#ifdef USE_IMGUI
#include "Source/Engine/UI/UIDragDropHelpers.h"
#include "Source/Engine/UI/ImGuiUtils.h"
#endif

// Game
#include "Source/Game/Transform.h"





#define COUT_USAGE_WARNING abort(); static_assert(false, "Use Warn() or DbgMsg() instead of std::cout. They exist for a reason.")
#define CERR_USAGE_WARNING abort(); static_assert(false, "Use Warn() or ErrMsg() instead of std::cerr. They exist for a reason.")
#define cout COUT_USAGE_WARNING
#define cerr CERR_USAGE_WARNING
