#include "stdafx.h"
#include "Debug/DebugDrawer.h"
#include <Windows.h>
#include <algorithm>
#include "Math/GameMath.h"
#include "Debug/ErrMsg.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;
using namespace DebugDraw;

bool DebugDrawer::Setup(dx::XMUINT2 screenSize, dx::XMUINT2 sceneSize, ID3D11Device *device, ID3D11DeviceContext *context, Content *content)
{
#ifndef DEBUG_DRAW
	return true;
#endif
	ZoneScopedXC(RandomUniqueColor());


	_device = device;
	_context = context;
	_content = content != nullptr ? content : _content; // Use the one already provided if none given.

	if (!CreateOverlayDepthStencilTexture(sceneSize.x, sceneSize.y))
	{
		ErrMsg("Failed to create overlay depth stencil texture!");
		return false;
	}

	if (!CreateDepthStencilState())
	{
		ErrMsg("Failed to create depth stencil state!");
		return false;
	}
	
	if (!CreateBlendState())
	{
		ErrMsg("Failed to create blend state!");
		return false;
	}

	if (!CreateRasterizerStates())
	{
		ErrMsg("Failed to create rasterizer states!");
		return false;
	}

	{
		XMMATRIX vMat = XMMatrixLookAtLH(
			XMVectorSet(screenSize.x * 0.5f, screenSize.y * 0.5f, 0, 1), // position
			XMVectorSet(screenSize.x * 0.5f, screenSize.y * 0.5f, -1, 0), // forward
			XMVectorSet(0, -1, 0, 0)  // up
		);

		XMMATRIX pMat = XMMatrixOrthographicLH((float)screenSize.x, (float)screenSize.y, 1.0f, 2.0f);

		XMFLOAT4X4A vpMat = { };
		Store(vpMat, XMMatrixTranspose(vMat * pMat));

		if (_screenViewProjBuffer.GetBuffer())
		{
			// Update
			if (!_screenViewProjBuffer.UpdateBuffer(_context, &vpMat))
				Warn("Failed to update screen-space view projection VS buffer!");
		}
		else
		{
			// Initialize
			if (!_screenViewProjBuffer.Initialize(device, sizeof(XMFLOAT4X4A), &vpMat))
				Warn("Failed to initialize screen-space view projection VS buffer!");
		}

		dx::XMFLOAT4 camDir = { 0, 0, -1, 0 };
		if (_screenDirBuffer.GetBuffer())
		{
			// Update
			if (!_screenDirBuffer.UpdateBuffer(_context, &camDir))
				Warn("Failed to update screen-space direction GS buffer!");
		}
		else
		{
			// Initialize
			if (!_screenDirBuffer.Initialize(device, sizeof(dx::XMFLOAT4), &camDir))
				Warn("Failed to initialize screen-space direction GS buffer!");
		}

		if (_camera)
			camDir = To4(_camera->GetTransform()->GetForward(World));

		if (_camDirBuffer.GetBuffer())
		{
			// Update
			if (!_camDirBuffer.UpdateBuffer(_context, &camDir))
				Warn("Failed to update camera direction GS buffer!");
		}
		else
		{
			// Initialize
			if (!_camDirBuffer.Initialize(device, sizeof(dx::XMFLOAT4), &camDir))
				Warn("Failed to initialize camera direction GS buffer!");
		}
	}

	return true;
}

void DebugDrawer::Shutdowm()
{
#ifndef DEBUG_DRAW
	return;
#endif
	_overlayDST.Reset();
	_overlayDSV.Reset();
	_dss.Reset();
	_blendState.Reset();
	_defaultRasterizer.Reset();
	_wireframeRasterizer.Reset();
	_screenViewProjBuffer.Reset();
	_camDirBuffer.Reset();
	_screenDirBuffer.Reset();
	_sceneLineMesh.Reset();
	_overlayLineMesh.Reset();
	_sceneTriMesh.Reset();
	_overlayTriMesh.Reset();
	_screenTriMesh.Reset();
	_sceneSpriteMeshes.clear(); 
	_overlaySpriteMeshes.clear();
	_screenSpriteMeshes.clear();
}

bool DebugDrawer::CreateOverlayDepthStencilTexture(const UINT width, const UINT height)
{
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D16_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	if (FAILED(_device->CreateTexture2D(&textureDesc, nullptr, _overlayDST.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create overlay depth stencil texture!");
		return false;
	}

	if (FAILED(_device->CreateDepthStencilView(_overlayDST.Get(), nullptr, _overlayDSV.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create overlay depth stencil view!");
		return false;
	}

	return true;
}
bool DebugDrawer::CreateDepthStencilState()
{
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = { };
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

	if (FAILED(_device->CreateDepthStencilState(&depthStencilDesc, _dss.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create screen-space depth stencil state!");
		return false;
	}

	return true;
}
bool DebugDrawer::CreateBlendState()
{
	D3D11_BLEND_DESC blendDesc = { };
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;

	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	if (FAILED(_device->CreateBlendState(&blendDesc, _blendState.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create blend state!");
		return false;
	}

	return true;
}
bool DebugDrawer::CreateRasterizerStates()
{
	D3D11_RASTERIZER_DESC rasterizerDesc = { };
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.ScissorEnable = false;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.AntialiasedLineEnable = false;

	if (FAILED(_device->CreateRasterizerState(&rasterizerDesc, &_defaultRasterizer)))
	{
		ErrMsg("Failed to create default rasterizer state!");
		return false;
	}

	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.AntialiasedLineEnable = true;

	if (FAILED(_device->CreateRasterizerState(&rasterizerDesc, &_wireframeRasterizer)))
	{
		ErrMsg("Failed to create wireframe rasterizer state!");
		return false;
	}

	return true;
}

bool DebugDrawer::HasSceneLineDraws() const
{
	return _sceneLineList.size() > 0;
}
bool DebugDrawer::HasOverlayLineDraws() const
{
	return _overlayLineList.size() > 0;
}
bool DebugDrawer::HasSceneTriDraws() const
{
	return _sceneTriList.size() > 0;
}
bool DebugDrawer::HasOverlayTriDraws() const
{
	return _overlayTriList.size() > 0;
}
bool DebugDrawer::HasScreenTriDraws() const
{
	return _screenTriList.size() > 0;
}
bool DebugDrawer::HasSceneSpriteDraws() const
{
	return !_sceneSpriteLists.empty();
}
bool DebugDrawer::HasOverlaySpriteDraws() const
{
	return !_overlaySpriteLists.empty();
}
bool DebugDrawer::HasScreenSpriteDraws() const
{
	return !_screenSpriteLists.empty();
}

void DebugDrawer::Clear()
{
#ifndef DEBUG_DRAW
	return;
#endif
	ZoneScopedXC(RandomUniqueColor());

	UINT sceneLineCount = static_cast<UINT>(_sceneLineList.size());
	_sceneLineList.clear();
	_sceneLineList.reserve(sceneLineCount);

	UINT overlayLineCount = static_cast<UINT>(_overlayLineList.size());
	_overlayLineList.clear();
	_overlayLineList.reserve(overlayLineCount);

	UINT sceneTriCount = static_cast<UINT>(_sceneTriList.size());
	_sceneTriList.clear();
	_sceneTriList.reserve(sceneTriCount);

	UINT overlayTriCount = static_cast<UINT>(_overlayTriList.size());
	_overlayTriList.clear();
	_overlayTriList.reserve(overlayTriCount);

	_sceneSpriteLists.clear();
	_overlaySpriteLists.clear();
}
void DebugDrawer::ClearScreenSpace()
{
#ifndef DEBUG_DRAW
	return;
#endif
	ZoneScopedXC(RandomUniqueColor());

	UINT screenTriCount = static_cast<UINT>(_screenTriList.size());
	_screenTriList.clear();
	_screenTriList.reserve(screenTriCount);

	_screenSpriteLists.clear();
}

bool DebugDrawer::Render(ID3D11RenderTargetView *targetRTV, 
	ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport)
{
#ifndef DEBUG_DRAW
	return true;
#endif
	ZoneScopedC(RandomUniqueColor());

	if (!_camera)
	{
		ErrMsg("Camera not set!");
		return false;
	}

	bool hasSceneLineDraws = HasSceneLineDraws();
	bool hasOverlayLineDraws = HasOverlayLineDraws();

	bool hasSceneTriDraws = HasSceneTriDraws();
	bool hasOverlayTriDraws = HasOverlayTriDraws();

	bool hasSceneSpriteDraws = HasSceneSpriteDraws();
	bool hasOverlaySpriteDraws = HasOverlaySpriteDraws();

	if (!hasSceneLineDraws && !hasOverlayLineDraws && 
		!hasSceneTriDraws && !hasOverlayTriDraws &&
		!hasSceneSpriteDraws && !hasOverlaySpriteDraws)
		return true;

	ID3D11BlendState *prevBlendState;
	UINT prevSampleMask = 0;
	FLOAT prevBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	_context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

	constexpr float transparentBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	_context->OMSetBlendState(_blendState.Get(), transparentBlendFactor, 0xffffffff);

	if (!_camera->BindDebugDrawBuffers())
	{
		ErrMsg("Failed to bind debug draw buffers!");
		return false;
	}

	dx::XMFLOAT4 camDir = To4(_camera->GetTransform()->GetForward(World));
	if (!_camDirBuffer.UpdateBuffer(_context, &camDir))
		Warn("Failed to update camera direction GS buffer!");

	ID3D11Buffer *const camDirBuffer = _camDirBuffer.GetBuffer();
	_context->GSSetConstantBuffers(1, 1, &camDirBuffer);

	if (hasSceneLineDraws || hasSceneTriDraws || hasSceneSpriteDraws)
	{
		// Draw with depth
		_context->OMSetRenderTargets(1, &targetRTV, targetDSV);
		_context->RSSetViewports(1, targetViewport);

		if (hasSceneLineDraws)
		{
			if (!RenderLines(&_sceneLineList, &_sceneLineMesh))
			{
				ErrMsg("Failed to render lines in scene!");
				return false;
			}
		}

		if (hasSceneTriDraws)
		{
			if (!RenderTris(&_sceneTriList, &_sceneTriMesh))
			{
				ErrMsg("Failed to render tris in scene!");
				return false;
			}
		}

		if (hasSceneSpriteDraws)
		{
			if (!RenderSprites(&_sceneSpriteLists, &_sceneSpriteMeshes))
			{
				ErrMsg("Failed to render sprites in scene!");
				return false;
			}
		}
	}

	if (hasOverlayLineDraws || hasOverlayTriDraws || hasOverlaySpriteDraws)
	{
		// Draw Overlay
		_context->ClearDepthStencilView(_overlayDSV.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);
		_context->OMSetRenderTargets(1, &targetRTV, _overlayDSV.Get());
		_context->RSSetViewports(1, targetViewport);

		ID3D11DepthStencilState *prevDepthStencilState;
		_context->OMGetDepthStencilState(&prevDepthStencilState, 0);
		_context->OMSetDepthStencilState(_dss.Get(), 0);

		if (hasOverlayLineDraws)
		{
			if (!RenderLines(&_overlayLineList, &_overlayLineMesh))
			{
				ErrMsg("Failed to render lines in scene!");
				return false;
			}
		}

		if (hasOverlayTriDraws)
		{
			if (!RenderTris(&_overlayTriList, &_overlayTriMesh))
			{
				ErrMsg("Failed to render tris in scene!");
				return false;
			}
		}

		if (hasOverlaySpriteDraws)
		{
			if (!RenderSprites(&_overlaySpriteLists, &_overlaySpriteMeshes))
			{
				ErrMsg("Failed to render sprites in scene!");
				return false;
			}
		}

		// Reset depth stencil state
		_context->OMSetDepthStencilState(prevDepthStencilState, 0);
		prevDepthStencilState->Release();
	}

	// Reset blend state
	_context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);

	// Unbind render target
	static ID3D11RenderTargetView *const nullRTV = nullptr;
	_context->OMSetRenderTargets(1, &nullRTV, nullptr);

	return true;
}
bool DebugDrawer::RenderScreenSpace(ID3D11RenderTargetView *targetRTV, 
	ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport)
{
#ifndef DEBUG_DRAW
	return true;
#endif
	ZoneScopedC(RandomUniqueColor());

	bool hasScreenTriDraws = HasScreenTriDraws();
	bool hasScreenSpriteDraws = HasScreenSpriteDraws();

	if (!hasScreenTriDraws && !hasScreenSpriteDraws)
		return true;

	ID3D11BlendState *prevBlendState;
	UINT prevSampleMask = 0;
	FLOAT prevBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	_context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

	constexpr float transparentBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	_context->OMSetBlendState(_blendState.Get(), transparentBlendFactor, 0xffffffff);

	// Bind screen camera buffers
	ID3D11Buffer *const camViewProjBuffer = _screenViewProjBuffer.GetBuffer();
	_context->VSSetConstantBuffers(0, 1, &camViewProjBuffer);

	ID3D11Buffer *const camGeoBuffers[2] = {camViewProjBuffer, _screenDirBuffer.GetBuffer()};
	_context->GSSetConstantBuffers(0, 2, camGeoBuffers);

	// Draw Screen-Space
	{
		//_context->ClearDepthStencilView(targetDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
		//_context->OMSetRenderTargets(1, &targetRTV, targetDSV);
		_context->OMSetRenderTargets(1, &targetRTV, nullptr);
		_context->RSSetViewports(1, targetViewport);

		ID3D11DepthStencilState *prevDepthStencilState;
		_context->OMGetDepthStencilState(&prevDepthStencilState, 0);
		_context->OMSetDepthStencilState(_dss.Get(), 0);

		if (hasScreenTriDraws)
		{
			if (!RenderTris(&_screenTriList, &_screenTriMesh))
			{
				ErrMsg("Failed to render screen-space tris!");
				return false;
			}
		}

		if (hasScreenSpriteDraws)
		{
			if (!RenderSprites(&_screenSpriteLists, &_screenSpriteMeshes))
			{
				ErrMsg("Failed to render screen-space sprites!");
				return false;
			}
		}

		// Reset depth stencil state
		_context->OMSetDepthStencilState(prevDepthStencilState, 0);
		prevDepthStencilState->Release();
	}

	// Unbind screen camera buffers
	ID3D11Buffer *const nullBuffers[2] = { nullptr, nullptr };
	_context->VSSetConstantBuffers(0, 1, nullBuffers);
	_context->GSSetConstantBuffers(0, 2, nullBuffers);

	// Reset blend state
	_context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);

	// Unbind render target
	static ID3D11RenderTargetView *const nullRTV = nullptr;
	_context->OMSetRenderTargets(1, &nullRTV, nullptr);

	return true;
}

bool DebugDrawer::RenderLines(std::vector<Line> *lineList, SimpleMeshD3D11 *mesh)
{
	ZoneScopedC(RandomUniqueColor());

	if (lineList->size() <= 0)
		return true;

	if (!CreateMesh(lineList, mesh))
	{
		ErrMsg("Failed to create mesh!");
		return false;
	}

	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	_context->RSSetState(_defaultRasterizer.Get());

	_context->IASetInputLayout(_content->GetInputLayout("DebugDraw")->GetInputLayout());

	if (!_content->GetShader("VS_DebugDraw")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug vertex shader!");
		return false;
	}

	if (!_content->GetShader("GS_DebugLine")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug line geometry shader!");
		return false;
	}

	if (!_content->GetShader("PS_DebugDraw")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug pixel shader!");
		return false;
	}

	mesh->BindMeshBuffers(_context);
	mesh->PerformDrawCall(_context);

	_context->GSSetShader(nullptr, nullptr, 0);

	return true;
}
bool DebugDrawer::RenderTris(std::vector<Tri> *triList, SimpleMeshD3D11 *mesh)
{
	ZoneScopedC(RandomUniqueColor());

	if (triList->size() <= 0)
		return true;

	if (!CreateMesh(triList, mesh, triList == &_screenTriList))
	{
		ErrMsg("Failed to create mesh!");
		return false;
	}

	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_context->RSSetState(_defaultRasterizer.Get());

	_context->IASetInputLayout(_content->GetInputLayout("DebugDrawTri")->GetInputLayout());

	if (!_content->GetShader("VS_DebugDrawTri")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug triangle vertex shader!");
		return false;
	}

	if (!_content->GetShader("PS_DebugDraw")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug pixel shader!");
		return false;
	}

	mesh->BindMeshBuffers(_context);
	mesh->PerformDrawCall(_context);

	return true;
}
bool DebugDrawer::RenderSprites(std::map<UINT, std::vector<Sprite>> *spriteLists, std::map<UINT, SimpleMeshD3D11> *meshes)
{
	ZoneScopedC(RandomUniqueColor());

	if (spriteLists->empty())
		return true;

	bool screenSpace = spriteLists == &_screenSpriteLists;

	for (auto it = spriteLists->begin(); it != spriteLists->end(); ++it)
	{
		UINT texID = it->first;
		auto spriteList = &it->second;
		auto mesh = &(*meshes)[texID];

		if (spriteList->empty())
			continue;

		if (!CreateMesh(spriteList, mesh, screenSpace))
		{
			ErrMsg("Failed to create mesh!");
			return false;
		}
	}

	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	_context->RSSetState(_defaultRasterizer.Get());

	_context->IASetInputLayout(_content->GetInputLayout("DebugDrawSprite")->GetInputLayout());

	if (!_content->GetShader("VS_DebugDrawSprite")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug vertex shader!");
		return false;
	}

	if (!_content->GetShader("GS_DebugDrawSprite")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug sprite geometry shader!");
		return false;
	}

	if (!_content->GetShader("PS_DebugDrawSprite")->BindShader(_context))
	{
		ErrMsg("Failed to bind debug pixel shader!");
		return false;
	}

	static UINT defaultSamplerID = _content->GetSamplerID("Fallback");
	ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
	_context->PSSetSamplers(0, 1, &ss);

	for (auto it = spriteLists->begin(); it != spriteLists->end(); ++it)
	{
		UINT texID = it->first;
		auto spriteList = &it->second;
		auto mesh = &(*meshes)[texID];

		if (spriteList->empty())
			continue;

		auto *tex = _content->GetTexture(texID);
		if (!tex)
		{
			WarnF("Texture with ID {} not found!", texID);
			continue;
		}

		ID3D11ShaderResourceView *srv = tex->GetSRV();
		_context->PSSetShaderResources(0, 1, &srv);

		mesh->BindMeshBuffers(_context);
		mesh->PerformDrawCall(_context);
	}

	_context->GSSetShader(nullptr, nullptr, 0);

	ID3D11ShaderResourceView *nullSrv = nullptr;
	_context->PSSetShaderResources(0, 1, &nullSrv);

	return true;
}

inline static float ReturnClosest(float a, float b)
{
	return a < b ? a : b;
}
inline static float ReturnFarthest(float a, float b)
{
	return a < b ? b : a;
}
inline static float ReturnCenter(float a, float b)
{
	return (a + b) * 0.5f;
}

void DebugDrawer::SortLines(std::vector<Line> *lineList, const XMFLOAT3A &direction)
{
	ZoneScopedXC(RandomUniqueColor());

	XMVECTOR dir = Load(direction);

	// Sort points based on their projection along the direction
	std::sort(lineList->begin(), lineList->end(),
		[&dir](const Line &a, const Line &b) { // Calculate dot products and compare scalar results

			float aDist = ReturnCenter(
				XMVectorGetX(XMVector3Dot(Load(a.start.position), dir)), 
				XMVectorGetX(XMVector3Dot(Load(a.end.position), dir))
			);

			float bDist = ReturnCenter(
				XMVectorGetX(XMVector3Dot(Load(b.start.position), dir)), 
				XMVectorGetX(XMVector3Dot(Load(b.end.position), dir))
			);

			/*
			XMVECTOR aMid = (
				Load(a.start.position) + 
				Load(a.end.position)
			) * 0.5f;

			XMVECTOR bMid = (
				Load(b.start.position) + 
				Load(b.end.position)
			) * 0.5f;

			float aDist = XMVectorGetX(XMVector3LengthEst(aMid - orig));
			float bDist = XMVectorGetX(XMVector3LengthEst(bMid - orig));
			*/

			return aDist > bDist;
		}
	);
}
void DebugDrawer::SortTris(std::vector<Tri> *triList, const XMFLOAT3A &direction)
{
	ZoneScopedXC(RandomUniqueColor());

	XMVECTOR dir = Load(direction);

	// Sort points based on their projection along the direction
	std::sort(triList->begin(), triList->end(),
		[&dir](const Tri &a, const Tri &b) { // Calculate dot products and compare scalar results

			float aDist = (
				XMVectorGetX(XMVector3Dot(Load(a.v0.position), dir)) + 
				XMVectorGetX(XMVector3Dot(Load(a.v1.position), dir)) + 
				XMVectorGetX(XMVector3Dot(Load(a.v2.position), dir))
			) * (1.0f / 3.0f);

			float bDist = (
				XMVectorGetX(XMVector3Dot(Load(b.v0.position), dir)) +
				XMVectorGetX(XMVector3Dot(Load(b.v1.position), dir)) +
				XMVectorGetX(XMVector3Dot(Load(b.v2.position), dir))
			) * (1.0f / 3.0f);

			/*
			XMVECTOR aMid = (
				Load(a.v0.position) + 
				Load(a.v1.position) + 
				Load(a.v2.position)
			) * (1.0f / 3.0f);

			XMVECTOR bMid = (
				Load(b.v0.position) + 
				Load(b.v1.position) + 
				Load(b.v2.position)
			) * (1.0f / 3.0f);

			float aDist = XMVectorGetX(XMVector3LengthEst(aMid - orig));
			float bDist = XMVectorGetX(XMVector3LengthEst(bMid - orig));
			*/

			return aDist > bDist;
		}
	);
}
void DebugDrawer::SortSprites(std::vector<Sprite> *spriteList, const dx::XMFLOAT3A &direction)
{
	ZoneScopedXC(RandomUniqueColor());

	XMVECTOR dir = Load(direction);

	// Sort points based on their projection along the direction
	std::sort(spriteList->begin(), spriteList->end(),
		[&dir](const Sprite &a, const Sprite &b) { // Calculate dot products and compare scalar results

			float aDist = XMVectorGetX(XMVector3Dot(Load(a.position), dir));
			float bDist = XMVectorGetX(XMVector3Dot(Load(b.position), dir));

			return aDist > bDist;
		}
	);
}

bool DebugDrawer::CreateMesh(std::vector<Line> *lineList, SimpleMeshD3D11 *mesh)
{
	ZoneScopedXC(RandomUniqueColor());

#ifdef DEBUG_DRAW_SORT
	SortLines(lineList, _camera->GetTransform()->GetForward(World));
#endif

	UINT vertCount = static_cast<UINT>(lineList->size() * 2);
	SimpleVertex *verticeData = new SimpleVertex[vertCount];

	std::memcpy(
		verticeData,
		lineList->data(),
		sizeof(SimpleVertex) * vertCount
	);

	SimpleMeshData simpleMeshData;
	simpleMeshData.vertexInfo.sizeOfVertex = sizeof(SimpleVertex);
	simpleMeshData.vertexInfo.nrOfVerticesInBuffer = vertCount;
	simpleMeshData.vertexInfo.vertexData = &(verticeData[0].x);

	if (!mesh->Initialize(_device, simpleMeshData))
	{
		ErrMsg("Failed to initialize simple mesh!");
		return false;
	}

	return true;
}
bool DebugDrawer::CreateMesh(std::vector<Tri> *triList, SimpleMeshD3D11 *mesh, bool screenSpace)
{
	ZoneScopedXC(RandomUniqueColor());

#ifdef DEBUG_DRAW_SORT
	SortTris(
		triList,
		screenSpace ? XMFLOAT3A(0, 0, -1) : _camera->GetTransform()->GetForward(World)
	);
#endif

	UINT vertCount = static_cast<UINT>(triList->size() * 3);
	SimpleVertex *verticeData = new SimpleVertex[vertCount];

	std::memcpy(
		verticeData,
		triList->data(),
		sizeof(SimpleVertex) * vertCount
	);

	SimpleMeshData simpleMeshData;
	simpleMeshData.vertexInfo.sizeOfVertex = sizeof(SimpleVertex);
	simpleMeshData.vertexInfo.nrOfVerticesInBuffer = vertCount;
	simpleMeshData.vertexInfo.vertexData = &(verticeData[0].x);

	if (!mesh->Initialize(_device, simpleMeshData))
	{
		ErrMsg("Failed to initialize simple mesh!");
		return false;
	}

	return true;
}
bool DebugDrawer::CreateMesh(std::vector<Sprite> *spriteList, SimpleMeshD3D11 *mesh, bool screenSpace)
{
	ZoneScopedXC(RandomUniqueColor());

#ifdef DEBUG_DRAW_SORT
	SortSprites(
		spriteList,
		screenSpace ? XMFLOAT3A(0, 0, -1) : _camera->GetTransform()->GetForward(World)
	);
#endif

	UINT vertCount = static_cast<UINT>(spriteList->size());
	SimpleSpritePoint *verticeData = new SimpleSpritePoint[vertCount];

	std::memcpy(
		verticeData,
		spriteList->data(),
		sizeof(SimpleSpritePoint) * vertCount
	);

	SimpleMeshData simpleMeshData;
	simpleMeshData.vertexInfo.sizeOfVertex = sizeof(SimpleSpritePoint);
	simpleMeshData.vertexInfo.nrOfVerticesInBuffer = vertCount;
	simpleMeshData.vertexInfo.vertexData = (float*)verticeData;

	if (!mesh->Initialize(_device, simpleMeshData))
	{
		ErrMsg("Failed to initialize simple mesh!");
		return false;
	}

	return true;
}

void DebugDrawer::SetCamera(CameraBehaviour *camera)
{
#ifndef DEBUG_DRAW
	return;
#endif

	_camera = camera;
}


#pragma region Draw Functions
void DebugDrawer::DrawLine(const Line &line, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &lineList = useDepth ? _sceneLineList : _overlayLineList;
	lineList.emplace_back(line);
}
void DebugDrawer::DrawLine(const LineSection & start, const LineSection & end, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawLine(Line(start, end), useDepth);
}
void DebugDrawer::DrawLine(const XMFLOAT3 &start, const XMFLOAT3 &end, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawLine({ {start, size, color}, {end, size, color} }, useDepth);
}
void DebugDrawer::DrawLines(const std::vector<Line> &lines, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &lineList = useDepth ? _sceneLineList : _overlayLineList;

	lineList.insert(std::end(lineList), std::begin(lines), std::end(lines));
}
void DebugDrawer::DrawLineStrip(const std::vector<LineSection> &lineStrip, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &lineList = useDepth ? _sceneLineList : _overlayLineList;

	if (lineStrip.size() <= 1)
		return;

	Line line;
	const LineSection *prevLine = &lineStrip[0];

	for (UINT i = 1; i < lineStrip.size(); i++)
	{
		line = { *prevLine, lineStrip[i] };
		prevLine = &lineStrip[i];
		lineList.emplace_back(line);
	}
}
void DebugDrawer::DrawLineStrip(const std::vector<XMFLOAT3> &points, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	std::vector<LineSection> lineStrip;
	lineStrip.reserve(points.size());

	for (UINT i = 0; i < points.size(); i++)
		lineStrip.emplace_back(points[i], size, color);

	DrawLineStrip(lineStrip, useDepth);
}
void DebugDrawer::DrawLineStrip(const XMFLOAT3 *points, const UINT length, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	std::vector<LineSection> lineStrip;
	lineStrip.reserve(length);

	for (UINT i = 0; i < length; i++)
		lineStrip.emplace_back(points[i], size, color);

	DrawLineStrip(lineStrip, useDepth);
}
void DebugDrawer::DrawRay(const XMFLOAT3 &origin, const XMFLOAT3 &dir, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	XMFLOAT3 end;
	Store(end, Load(origin) + Load(dir));
	DrawLine({ {origin, size, color}, {end, size, color} }, useDepth);
}

void DebugDrawer::DrawLineThreadSafe(const Line &line, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &lineList = useDepth ? _sceneLineList : _overlayLineList;

#pragma omp critical
	{
		lineList.emplace_back(line);
	}
}
void DebugDrawer::DrawLineThreadSafe(const XMFLOAT3 &start, const XMFLOAT3 &end, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawLineThreadSafe({ {start, size, color}, {end, size, color} }, useDepth);
}
void DebugDrawer::DrawLineThreadSafe(const LineSection &start, const LineSection &end, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawLineThreadSafe({ start, end }, useDepth);
}
void DebugDrawer::DrawLinesThreadSafe(const std::vector<Line> &lines, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &lineList = useDepth ? _sceneLineList : _overlayLineList;

#pragma omp critical
	{
		lineList.insert(std::end(lineList), std::begin(lines), std::end(lines));
	}
}
void DebugDrawer::DrawLineStripThreadSafe(const std::vector<LineSection> &lineStrip, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &lineList = useDepth ? _sceneLineList : _overlayLineList;

	if (lineStrip.size() <= 1)
		return;

	Line line;
	const LineSection *prevLine = &lineStrip[0];

#pragma omp critical
	{
		for (UINT i = 1; i < lineStrip.size(); i++)
		{
			line = { *prevLine, lineStrip[i] };
			prevLine = &lineStrip[i];

			lineList.emplace_back(line);
		}
	}
}
void DebugDrawer::DrawLineStripThreadSafe(const std::vector<XMFLOAT3> &points, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	std::vector<LineSection> lineStrip;
	lineStrip.reserve(points.size());

	for (UINT i = 0; i < points.size(); i++)
		lineStrip.emplace_back(points[i], size, color);

	DrawLineStripThreadSafe(lineStrip, useDepth);
}
void DebugDrawer::DrawLineStripThreadSafe(const XMFLOAT3 *points, const UINT length, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	std::vector<LineSection> lineStrip;
	lineStrip.reserve(length);

	for (UINT i = 0; i < length; i++)
		lineStrip.emplace_back(points[i], size, color);

	DrawLineStripThreadSafe(lineStrip, useDepth);
}
void DebugDrawer::DrawRayThreadSafe(const XMFLOAT3 &origin, const XMFLOAT3 &dir, float size, const XMFLOAT4 &color, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	XMFLOAT3 end;
	Store(end, Load(origin) + Load(dir));
	DrawLineThreadSafe({ {origin, size, color}, {end, size, color} }, useDepth);
}


void DebugDrawer::DrawTri(const Tri &tri, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	auto &triList = useDepth ? _sceneTriList : _overlayTriList;
	triList.emplace_back(tri);

	if (twoSided)
		triList.emplace_back(Tri(tri.v2, tri.v1, tri.v0));
}
void DebugDrawer::DrawTri(const Shape::Tri &tri, const dx::XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawTri(Tri(
		Vertex(tri.v0, color),
		Vertex(tri.v1, color),
		Vertex(tri.v2, color)
	), useDepth, twoSided);
}
void DebugDrawer::DrawTri(const Vertex &v0, const Vertex &v1, const Vertex &v2, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawTri(Tri(v0, v1, v2), useDepth, twoSided);
}
void DebugDrawer::DrawTri(const XMFLOAT3 &v0, const XMFLOAT3 &v1, const XMFLOAT3 &v2, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawTri(Tri(Vertex(To3(v0), color), Vertex(To3(v1), color), Vertex(To3(v2), color)), useDepth, twoSided);
}
void DebugDrawer::DrawTriPlanar(const XMFLOAT3 &v0, const XMFLOAT3 &toV1, const XMFLOAT3 &toV2, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	XMFLOAT3 v1, v2;
	XMVECTOR v0Vec = Load(v0);
	Store(v1, v0Vec + Load(toV1));
	Store(v2, v0Vec + Load(toV2));

	DrawTri(v0, v1, v2, color, useDepth, twoSided);
}
void DebugDrawer::DrawTris(const Tri *tris, int count, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	for (int i = 0; i < count; i++)
		DrawTri(tris[i], useDepth, twoSided);
}
void DebugDrawer::DrawTriStrip(const Vertex *verts, int count, bool useDepth, bool twoSided)
{
	// TODO: Implement
	Warn("DrawTriStrip not implemented yet!");
}
void DebugDrawer::DrawTriStrip(const XMFLOAT3 *points, int count, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
	// TODO: Implement
	Warn("DrawTriStrip not implemented yet!");
}


void DebugDrawer::DrawQuad(const XMFLOAT3 &center, const XMFLOAT3 &right, const XMFLOAT3 &up, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	// Draw two triangles to form a quad
	XMVECTOR 
		centerVec = Load(center), 
		rightVec = Load(right),
		upVec = Load(up);

	XMFLOAT3 v0, v1, v2, v3;
	Store(v0, centerVec - rightVec - upVec);
	Store(v1, centerVec + rightVec - upVec);
	Store(v2, centerVec + rightVec + upVec);
	Store(v3, centerVec - rightVec + upVec);

	DrawTri(v0, v1, v2, color, useDepth, twoSided);
	DrawTri(v0, v2, v3, color, useDepth, twoSided);
}
void DebugDrawer::DrawBoxAABB(const XMFLOAT3 &center, const XMFLOAT3 &extents, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	// Axis-aligned box, draw 12 triangles
	XMVECTOR centerVec = Load(center);
	XMFLOAT3 extentsAbs = { std::abs(extents.x), std::abs(extents.y), std::abs(extents.z) };
	XMFLOAT3 v0, v1, v2, v3, v4, v5, v6, v7;

	Store(v0, centerVec + XMVectorSet(-extentsAbs.x, -extentsAbs.y, -extentsAbs.z, 0));
	Store(v1, centerVec + XMVectorSet(extentsAbs.x, -extentsAbs.y, -extentsAbs.z, 0));
	Store(v2, centerVec + XMVectorSet(extentsAbs.x, -extentsAbs.y, extentsAbs.z, 0));
	Store(v3, centerVec + XMVectorSet(-extentsAbs.x, -extentsAbs.y, extentsAbs.z, 0));
	Store(v4, centerVec + XMVectorSet(-extentsAbs.x, extentsAbs.y, -extentsAbs.z, 0));
	Store(v5, centerVec + XMVectorSet(extentsAbs.x, extentsAbs.y, -extentsAbs.z, 0));
	Store(v6, centerVec + XMVectorSet(extentsAbs.x, extentsAbs.y, extentsAbs.z, 0));
	Store(v7, centerVec + XMVectorSet(-extentsAbs.x, extentsAbs.y, extentsAbs.z, 0));

	DrawTri(v2, v1, v0, color, useDepth, twoSided);
	DrawTri(v3, v2, v0, color, useDepth, twoSided);
	DrawTri(v4, v5, v6, color, useDepth, twoSided);
	DrawTri(v4, v6, v7, color, useDepth, twoSided);
	DrawTri(v0, v1, v5, color, useDepth, twoSided);
	DrawTri(v0, v5, v4, color, useDepth, twoSided);
	DrawTri(v1, v2, v6, color, useDepth, twoSided);
	DrawTri(v1, v6, v5, color, useDepth, twoSided);
	DrawTri(v2, v3, v7, color, useDepth, twoSided);
	DrawTri(v2, v7, v6, color, useDepth, twoSided);
	DrawTri(v3, v0, v4, color, useDepth, twoSided);
	DrawTri(v3, v4, v7, color, useDepth, twoSided);
}
void DebugDrawer::DrawBoxAABB(const BoundingBox &aabb, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawBoxAABB(aabb.Center, aabb.Extents, color, useDepth, twoSided);
}
void DebugDrawer::DrawBoxMinMax(const XMFLOAT3 &min, const XMFLOAT3 &max, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	XMFLOAT3 
		v0 = min, 
		v1 = { max.x, min.y, min.z }, 
		v2 = { max.x, min.y, max.z }, 
		v3 = { min.x, min.y, max.z }, 
		v4 = { min.x, max.y, min.z }, 
		v5 = { max.x, max.y, min.z }, 
		v6 = max, 
		v7 = { min.x, max.y, max.z };

	DrawTri(v2, v1, v0, color, useDepth, twoSided);
	DrawTri(v3, v2, v0, color, useDepth, twoSided);
	DrawTri(v4, v5, v6, color, useDepth, twoSided);
	DrawTri(v4, v6, v7, color, useDepth, twoSided);
	DrawTri(v0, v1, v5, color, useDepth, twoSided);
	DrawTri(v0, v5, v4, color, useDepth, twoSided);
	DrawTri(v1, v2, v6, color, useDepth, twoSided);
	DrawTri(v1, v6, v5, color, useDepth, twoSided);
	DrawTri(v2, v3, v7, color, useDepth, twoSided);
	DrawTri(v2, v7, v6, color, useDepth, twoSided);
	DrawTri(v3, v0, v4, color, useDepth, twoSided);
	DrawTri(v3, v4, v7, color, useDepth, twoSided);
}
void DebugDrawer::DrawBoxOBB(const BoundingOrientedBox &obb, const XMFLOAT4 &color, bool useDepth, bool twoSided)
{
#ifndef DEBUG_DRAW
	return;
#endif

	// Draw a rotated box using the OBB's orientation
	XMVECTOR 
		center = Load(obb.Center),
		orientation = Load(obb.Orientation);

	XMVECTOR 
		right = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), orientation),
		up = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), orientation),
		forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), orientation);

	XMVECTOR 
		r = obb.Extents.x * right, 
		u = obb.Extents.y * up, 
		f = obb.Extents.z * forward;

	XMFLOAT3 v0, v1, v2, v3, v4, v5, v6, v7;
	Store(v0, center - r - u - f);
	Store(v1, center + r - u - f);
	Store(v2, center + r + u - f);
	Store(v3, center - r + u - f);
	Store(v4, center - r - u + f);
	Store(v5, center + r - u + f);
	Store(v6, center + r + u + f);
	Store(v7, center - r + u + f);

	DrawTri(v2, v1, v0, color, useDepth, twoSided);
	DrawTri(v3, v2, v0, color, useDepth, twoSided);
	DrawTri(v4, v5, v6, color, useDepth, twoSided);
	DrawTri(v4, v6, v7, color, useDepth, twoSided);
	DrawTri(v0, v1, v5, color, useDepth, twoSided);
	DrawTri(v0, v5, v4, color, useDepth, twoSided);
	DrawTri(v1, v2, v6, color, useDepth, twoSided);
	DrawTri(v1, v6, v5, color, useDepth, twoSided);
	DrawTri(v2, v3, v7, color, useDepth, twoSided);
	DrawTri(v2, v7, v6, color, useDepth, twoSided);
	DrawTri(v3, v0, v4, color, useDepth, twoSided);
	DrawTri(v3, v4, v7, color, useDepth, twoSided);
}


void DebugDrawer::DrawTriSS(const Tri &tri)
{
#ifndef DEBUG_DRAW
	return;
#endif

	_screenTriList.emplace_back(tri);
}
void DebugDrawer::DrawLineSS(const XMFLOAT2 &p1, const XMFLOAT2 &p2, const XMFLOAT4 &color, float thickness, float layer)
{
#ifndef DEBUG_DRAW
	return;
#endif

	// Layer is given as 0-1, reformat to fit within of z-planes (-1, -2).
	layer = std::clamp(layer - 2.0f, -2.0f, -1.0f);

	XMVECTOR p1v = Load(p1);
	XMVECTOR p2v = Load(p2);

	XMVECTOR perp = XMVectorSubtract(p2v, p1v);
	float length = XMVectorGetX(XMVector2Length(perp));

	XMVECTOR ortho = XMVector2Normalize(XMVector3Cross(perp, XMVectorSet(0, 0, 1, 0))) * (thickness * 0.5f);

	XMFLOAT3 v0, v1, v2, v3;

	Store(v0, p1v - ortho);
	Store(v1, p1v + ortho);
	Store(v2, p2v + ortho);
	Store(v3, p2v - ortho);

	v0.z = layer;
	v1.z = layer;
	v2.z = layer;
	v3.z = layer;

	DrawTriSS(Tri(Vertex(To3(v0), color), Vertex(To3(v1), color), Vertex(To3(v2), color)));
	DrawTriSS(Tri(Vertex(To3(v0), color), Vertex(To3(v2), color), Vertex(To3(v3), color)));
}
void DebugDrawer::DrawMinMaxRect(const XMFLOAT2 &min, const XMFLOAT2 &max, const XMFLOAT4 &color, float layer)
{
#ifndef DEBUG_DRAW
	return;
#endif

	// Layer is given as 0-1, reformat to fit within of z-planes (-1, -2).
	layer = std::clamp(layer - 2.0f, -2.0f, -1.0f);

	XMFLOAT3 v0 = { min.x, min.y, layer };
	XMFLOAT3 v1 = { max.x, min.y, layer };
	XMFLOAT3 v2 = { max.x, max.y, layer };
	XMFLOAT3 v3 = { min.x, max.y, layer };

	DrawTriSS(Tri(Vertex(To3(v0), color), Vertex(To3(v1), color), Vertex(To3(v2), color)));
	DrawTriSS(Tri(Vertex(To3(v0), color), Vertex(To3(v2), color), Vertex(To3(v3), color)));
}
void DebugDrawer::DrawExtentRect(const XMFLOAT2 &pos, const XMFLOAT2 &extents, const XMFLOAT4 &color, float layer)
{
#ifndef DEBUG_DRAW
	return;
#endif

	DrawMinMaxRect(
		{ pos.x - extents.x, pos.y - extents.y }, 
		{ pos.x + extents.x, pos.y + extents.y },
		color, layer
	);
}


void DebugDrawer::DrawSprite(UINT texID, Sprite sprite, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	if (texID == CONTENT_NULL)
		return;

	// Remap the UV coordinates such that they behave as expected.
	float temp = sprite.uv0.x;
	sprite.uv0.x = 1.0f - sprite.uv1.x;
	sprite.uv1.x = 1.0f - temp;

	auto &spriteLists = useDepth ? _sceneSpriteLists : _overlaySpriteLists;
	auto &spriteList = spriteLists[texID];

	spriteList.emplace_back(sprite);
}
void DebugDrawer::DrawSprite(UINT texID, const XMFLOAT4 &pos, const XMFLOAT2 &size, const XMFLOAT4 &color, XMFLOAT2 uv0, XMFLOAT2 uv1, bool useDepth)
{
#ifndef DEBUG_DRAW
	return;
#endif

	if (texID == CONTENT_NULL)
		return;

	// Remap the UV coordinates such that they behave as expected.
	float temp = uv0.x;
	uv0.x = 1.0f - uv1.x;
	uv1.x = 1.0f - temp;

	auto &spriteLists = useDepth ? _sceneSpriteLists : _overlaySpriteLists;
	auto &spriteList = spriteLists[texID];

	spriteList.emplace_back(pos, color, size, uv0, uv1);
}

void DebugDrawer::DrawSpriteSS(UINT texID, Sprite sprite)
{
#ifndef DEBUG_DRAW
	return;
#endif

	if (texID == CONTENT_NULL)
		return;

	// Layer is given as 0-1, reformat to fit within of z-planes (-1, -2).
	sprite.position.z = std::clamp(sprite.position.z - 2.0f, -2.0f, -1.0f);

	// Remap the UV coordinates such that they behave as expected.
	float temp = sprite.uv0.y;
	sprite.uv0.y = sprite.uv1.y;
	sprite.uv1.y = temp;
	sprite.uv0.x = 1.0f - sprite.uv0.x;
	sprite.uv1.x = 1.0f - sprite.uv1.x;

	_screenSpriteLists[texID].emplace_back(sprite);
}
void DebugDrawer::DrawSpriteSS(UINT texID, float layer, const XMFLOAT2 &pos, const XMFLOAT2 &size, const XMFLOAT4 &color, XMFLOAT2 uv0, XMFLOAT2 uv1)
{
#ifndef DEBUG_DRAW
	return;
#endif

	if (texID == CONTENT_NULL)
		return;

	// Layer is given as 0-1, reformat to fit within of z-planes (-1, -2).
	layer = std::clamp(layer - 2.0f, -2.0f, -1.0f);

	// Remap the UV coordinates such that they behave as expected.
	float temp = uv0.y;
	uv0.y = uv1.y;
	uv1.y = temp;
	uv0.x = 1.0f - uv0.x;
	uv1.x = 1.0f - uv1.x;

	XMFLOAT4 pos4 = { pos.x, pos.y, layer, 0.0f };
	_screenSpriteLists[texID].emplace_back(pos4, color, size, uv0, uv1);
}
#pragma endregion
