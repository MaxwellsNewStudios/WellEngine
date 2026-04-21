#pragma once
#undef NDEBUG

#include "Tests/TestUtils.h"

#include "Engine/EngineSettings.h"
#include "Engine/Debug/TrackedAlloc.h"
#include "Engine/Debug/DebugNew.h"

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
#include <cstdarg>
#include <functional>
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
#include "Jolt/Jolt.h"
JPH_SUPPRESS_WARNING_PUSH
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
JPH_SUPPRESS_WARNING_POP

#ifdef USE_IMGUI
#include "Dependencies/tinyfiledialogs/tinyfiledialogs.h"
#include "Dependencies/ImGui/imconfig.h"
#include "Dependencies/ImGui/imgui.h"
#include "Dependencies/ImGui/imgui_impl_sdl3.h"
#include "Dependencies/ImGui/imgui_impl_dx11.h"
#include "Dependencies/ImGui/imgui_stdlib.h"
#include "Dependencies/ImPlot/implot.h"

#include "Engine/UI/ImGuiExtensions.h"
#include "Engine/UI/Fonts/IconsCodicons.h"
#include "Engine/UI/Fonts/IconsFontaudio.h"
#include "Engine/UI/Fonts/IconsFontAwesome6.h"
#include "Engine/UI/Fonts/IconsForkAwesome.h"
#include "Engine/UI/Fonts/IconsLucide.h"
#include "Engine/UI/Fonts/IconsMaterialDesignIcons.h"
#include "Engine/UI/Fonts/IconsMaterialSymbols.h"
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


// Engine
#include "Engine/EngineDefinitions.h"
#include "Engine/Math/GameMath.h"
#include "Engine/Math/ConstRand.h"
#include "Engine/Math/Primitives.h"
#include "Engine/Math/Curves.h"
#include "Engine/Utils/UIDHelper.h"
#include "Engine/Utils/ReferenceHelper.h"
#include "Engine/Utils/StringUtils.h"
#include "Engine/Utils/SerializerUtils.h"
#include "Engine/Utils/RepeatTracker.h"
#include "Engine/Content/Material.h"
#include "Engine/Content/DirectoryManager.h"
#include "Engine/Content/ContentRegistry.h"
#include "Engine/Rendering/RendererInfo.h"
#include "Engine/Rendering/RenderQueuer.h"
#include "Engine/Window/Window.h"
#include "Engine/Timing/TimeUtils.h"
#include "Engine/Input/Input.h"
#include "Engine/Input/InputBindings.h"
#include "Engine/Collision/ColliderShapes.h"
#include "Engine/Collision/Raycast.h"
#include "Engine/Debug/ErrMsg.h"
#include "Engine/D3D/D3D11Helper.h"
#include "Engine/D3D/D3D11FormatData.h"
#include "Engine/D3D/ConstantBufferD3D11.h"
#include "Engine/D3D/DepthBufferD3D11.h"
#include "Engine/D3D/IndexBufferD3D11.h"
#include "Engine/D3D/InputLayoutD3D11.h"
#include "Engine/D3D/MeshD3D11.h"
#include "Engine/D3D/RenderTargetD3D11.h"
#include "Engine/D3D/SamplerD3D11.h"
#include "Engine/D3D/ShaderD3D11.h"
#include "Engine/D3D/ShaderResourceTextureD3D11.h"
#include "Engine/D3D/SimpleMeshD3D11.h"
#include "Engine/D3D/StructuredBufferD3D11.h"
#include "Engine/D3D/SubMeshD3D11.h"
#include "Engine/D3D/VertexBufferD3D11.h"

#ifdef USE_IMGUI
#include "Engine/UI/UIDragDropHelpers.h"
#include "Engine/UI/ImGuiUtils.h"
#endif

// Game
#include "Game/Transform.h"

#undef min
#undef max

using namespace WellEngine;






#define COUT_USAGE_WARNING abort(); static_assert(false, "Use Warn() or DbgMsg() instead of std::cout. They exist for a reason.")
#define CERR_USAGE_WARNING abort(); static_assert(false, "Use Warn() or ErrMsg() instead of std::cerr. They exist for a reason.")
#define cout COUT_USAGE_WARNING
#define cerr CERR_USAGE_WARNING
