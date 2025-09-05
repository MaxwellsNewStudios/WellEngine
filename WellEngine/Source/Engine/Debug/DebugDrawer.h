#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Content/Content.h"
#include "Behaviours/CameraBehaviour.h"
#include "D3D/SimpleMeshD3D11.h"

namespace DebugDraw
{
	struct LineSection
	{
		dx::XMFLOAT3 position;
		float size;
		dx::XMFLOAT4 color;

		LineSection() = default;
		LineSection(const LineSection &) = default;
		LineSection(LineSection &&) = default;

		LineSection &operator=(const LineSection &) = default;
		LineSection &operator=(LineSection &&) = default;

		constexpr LineSection(dx::XMFLOAT3 position, float size, dx::XMFLOAT4 color) noexcept : position(position), size(size), color(color) {}
	};
	struct Line 
	{
		LineSection start, end;

		Line() = default;
		Line(const Line &) = default;
		Line(Line &&) = default;

		Line &operator=(const Line &) = default;
		Line &operator=(Line &&) = default;

		constexpr Line(LineSection start, LineSection end) noexcept : start(start), end(end) {}
		constexpr Line(dx::XMFLOAT3 start, dx::XMFLOAT3 end, float size, dx::XMFLOAT4A color) noexcept : 
			start({ start, size, color }), end({ end, size, color }) {}
	};

	struct Vertex
	{
		dx::XMFLOAT4 position{};
		dx::XMFLOAT4 color{};

		Vertex() = default;
		Vertex(const Vertex &) = default;
		Vertex(Vertex &&) = default;

		Vertex &operator=(const Vertex &) = default;
		Vertex &operator=(Vertex &&) = default;

		constexpr Vertex(const dx::XMFLOAT3 &position, const dx::XMFLOAT4 &color) noexcept : position(dx::XMFLOAT4A(position.x, position.y, position.z, 1.0f)), color(color) {}
	};
	struct Tri
	{
		Vertex v0, v1, v2;

		Tri() = default;
		Tri(const Tri &) = default;
		Tri(Tri &&) = default;

		Tri &operator=(const Tri &) = default;
		Tri &operator=(Tri &&) = default;

		constexpr Tri(Vertex v0, Vertex v1, Vertex v2) noexcept : v0(v0), v1(v1), v2(v2) {}
		constexpr Tri(dx::XMFLOAT3A v0, dx::XMFLOAT3A v1, dx::XMFLOAT3A v2, dx::XMFLOAT4 color) noexcept :
			v0({ v0, color }), v1({ v1, color }), v2({ v2, color }) {}
	};

	struct Sprite
	{
		dx::XMFLOAT4 position{};
		dx::XMFLOAT4 color{1,1,1,1};
		dx::XMFLOAT2 uv0{0,0};
		dx::XMFLOAT2 uv1{1,1};
		dx::XMFLOAT2 size{1,1};

		Sprite() = default;
		Sprite(dx::XMFLOAT4 pos, dx::XMFLOAT4 color, dx::XMFLOAT2 size, dx::XMFLOAT2 uv0, dx::XMFLOAT2 uv1) :
			position(std::move(pos)), color(std::move(color)), size(std::move(size)), uv0(std::move(uv0)), uv1(std::move(uv1)) { }
		Sprite(const Sprite &) = default;
		Sprite(Sprite &&) = default;

		Sprite &operator=(const Sprite &) = default;
		Sprite &operator=(Sprite &&) = default;
	};
}

#define DD DebugDraw

/// Allows for drawing debug shapes through simple function calls from anywhere.
class DebugDrawer
{
private:
	ID3D11Device *_device			= nullptr;
	ID3D11DeviceContext	*_context	= nullptr;
	Content	*_content				= nullptr;

	ComPtr<ID3D11Texture2D>			_overlayDST = nullptr;
	ComPtr<ID3D11DepthStencilView>	_overlayDSV = nullptr;
	ComPtr<ID3D11DepthStencilState>	_dss = nullptr;
	ComPtr<ID3D11BlendState>		_blendState = nullptr;
	ComPtr<ID3D11RasterizerState>	_defaultRasterizer = nullptr;
	ComPtr<ID3D11RasterizerState>	_wireframeRasterizer = nullptr;

	ConstantBufferD3D11 _screenViewProjBuffer;
	ConstantBufferD3D11 _camDirBuffer, _screenDirBuffer;
	CameraBehaviour *_camera = nullptr;

	std::vector<DD::Line> _sceneLineList, _overlayLineList;
	SimpleMeshD3D11		  _sceneLineMesh, _overlayLineMesh;

	std::vector<DD::Tri> _sceneTriList, _overlayTriList, _screenTriList;
	SimpleMeshD3D11		 _sceneTriMesh, _overlayTriMesh, _screenTriMesh;

	std::map<UINT, std::vector<DD::Sprite>> _sceneSpriteLists,	_overlaySpriteLists,  _screenSpriteLists;
	std::map<UINT, SimpleMeshD3D11>			_sceneSpriteMeshes, _overlaySpriteMeshes, _screenSpriteMeshes;

	[[nodiscard]] bool CreateOverlayDepthStencilTexture(const UINT width, const UINT height);
	[[nodiscard]] bool CreateDepthStencilState();
	[[nodiscard]] bool CreateBlendState();
	[[nodiscard]] bool CreateRasterizerStates();

	void SortLines(std::vector<DD::Line> *lineList, const dx::XMFLOAT3A &direction);
	void SortTris(std::vector<DD::Tri> *triList, const dx::XMFLOAT3A &direction);
	void SortSprites(std::vector<DD::Sprite> *spriteList, const dx::XMFLOAT3A &direction);

	[[nodiscard]] bool CreateMesh(std::vector<DD::Line> *lineList, SimpleMeshD3D11 *mesh);
	[[nodiscard]] bool RenderLines(std::vector<DD::Line> *lineList, SimpleMeshD3D11 *mesh);

	[[nodiscard]] bool CreateMesh(std::vector<DD::Tri> *triList, SimpleMeshD3D11 *mesh, bool screenSpace);
	[[nodiscard]] bool RenderTris(std::vector<DD::Tri> *triList, SimpleMeshD3D11 *mesh);

	[[nodiscard]] bool CreateMesh(std::vector<DD::Sprite> *spriteList, SimpleMeshD3D11 *mesh, bool screenSpace);
	[[nodiscard]] bool RenderSprites(std::map<UINT, std::vector<DD::Sprite>> *spriteLists, std::map<UINT, SimpleMeshD3D11> *meshes);

	[[nodiscard]] bool HasSceneLineDraws() const;
	[[nodiscard]] bool HasOverlayLineDraws() const;

	[[nodiscard]] bool HasSceneTriDraws() const;
	[[nodiscard]] bool HasOverlayTriDraws() const;
	[[nodiscard]] bool HasScreenTriDraws() const;

	[[nodiscard]] bool HasSceneSpriteDraws() const;
	[[nodiscard]] bool HasOverlaySpriteDraws() const;
	[[nodiscard]] bool HasScreenSpriteDraws() const;

public:
	[[nodiscard]] bool Setup(dx::XMUINT2 screenSize, dx::XMUINT2 sceneSize, 
		ID3D11Device *device, ID3D11DeviceContext *context, Content *content);

	void Shutdowm();

	[[nodiscard]] bool Render(ID3D11RenderTargetView *targetRTV, 
		ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport);

	[[nodiscard]] bool RenderScreenSpace(ID3D11RenderTargetView *targetRTV, 
		ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport);

	[[nodiscard]] static inline DebugDrawer &Instance() 
	{
		static DebugDrawer instance;
		return instance;
	}

	void Clear();
	void ClearScreenSpace();
	void SetCamera(CameraBehaviour *camera);


	/// Push a non-uniform line to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawLine(const DD::Line &line, bool useDepth = true);
	/// Push a non-uniform line to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawLine(const DD::LineSection &start, const DD::LineSection &end, bool useDepth = true);
	/// Push a uniform line to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawLine(const dx::XMFLOAT3 &start, const dx::XMFLOAT3 &end, float size, const dx::XMFLOAT4 &color, bool useDepth = true);
	/// Push a sequence of non-uniform discontinuous lines to the debug draw list. They will be drawn on the next render call, then discarded.
	void DrawLines(const std::vector<DD::Line> &lines, bool useDepth = true);
	/// Push a non-uniform & continuous line strip to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawLineStrip(const std::vector<DD::LineSection> &lineStrip, bool useDepth = true);
	/// Push a uniform & continuous line strip to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawLineStrip(const std::vector<dx::XMFLOAT3> &points, float size, const dx::XMFLOAT4 &color, bool useDepth = true);
	/// Push a uniform & continuous line strip to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawLineStrip(const dx::XMFLOAT3 *points, const UINT length, float size, const dx::XMFLOAT4 &color, bool useDepth = true);
	/// Push a uniform ray to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawRay(const dx::XMFLOAT3 &origin, const dx::XMFLOAT3 &dir, float size, const dx::XMFLOAT4 &color, bool useDepth = true);

	/// Thread-safe variant of DrawLine().
	void DrawLineThreadSafe(const dx::XMFLOAT3 &start, const dx::XMFLOAT3 &end, float size, const dx::XMFLOAT4 &color, bool useDepth = true);
	/// Thread-safe variant of DrawLine().
	void DrawLineThreadSafe(const DD::Line &line, bool useDepth = true);
	/// Thread-safe variant of DrawLine().
	void DrawLineThreadSafe(const DD::LineSection &start, const DD::LineSection &end, bool useDepth = true);
	/// Thread-safe variant of DrawLines().
	void DrawLinesThreadSafe(const std::vector<DD::Line> &lines, bool useDepth = true);
	/// Thread-safe variant of DrawLineStrip().
	void DrawLineStripThreadSafe(const std::vector<DD::LineSection> &lineStrip, bool useDepth = true);
	/// Thread-safe variant of DrawLineStrip().
	void DrawLineStripThreadSafe(const std::vector<dx::XMFLOAT3> &points, float size, const dx::XMFLOAT4 &color, bool useDepth = true);
	/// Thread-safe variant of DrawLineStrip().
	void DrawLineStripThreadSafe(const dx::XMFLOAT3 *points, UINT length, float size, const dx::XMFLOAT4 &color, bool useDepth = true);
	/// Thread-safe variant of DrawRay().
	void DrawRayThreadSafe(const dx::XMFLOAT3 &origin, const dx::XMFLOAT3 &dir, float size, const dx::XMFLOAT4 &color, bool useDepth = true);


	/// Push a multi-colored triangle to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTri(const DD::Tri &tri, bool useDepth = true, bool twoSided = false);
	/// Push a triangle to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTri(const Shape::Tri &tri, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// Push a multi-colored triangle to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTri(const DD::Vertex &v0, const DD::Vertex &v1, const DD::Vertex &v2, bool useDepth = true, bool twoSided = false);
	/// Push a triangle as three points to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTri(const dx::XMFLOAT3 &v0, const dx::XMFLOAT3 &v1, const dx::XMFLOAT3 &v2, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// Push a triangle as a point and two normals to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTriPlanar(const dx::XMFLOAT3 &v0, const dx::XMFLOAT3 &toV1, const dx::XMFLOAT3 &toV2, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// Push a sequence of multi-colored triangles to the debug draw list. They will be drawn on the next render call, then discarded.
	void DrawTris(const DD::Tri *tris, int count, bool useDepth = true, bool twoSided = false);
	/// Push a multi-colored triangle strip to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTriStrip(const DD::Vertex *verts, int count, bool useDepth = true, bool twoSided = false);
	/// Push a triangle strip to the debug draw list. It will be drawn on the next render call, then discarded.
	void DrawTriStrip(const dx::XMFLOAT3 *points, int count, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);


	/// TODO: Describe
	void DrawQuad(const dx::XMFLOAT3 &center, const dx::XMFLOAT3 &right, const dx::XMFLOAT3 &up, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// TODO: Describe
	void DrawBoxAABB(const dx::XMFLOAT3 &center, const dx::XMFLOAT3 &extents, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// TODO: Describe
	void DrawBoxAABB(const dx::BoundingBox &aabb, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// TODO: Describe
	void DrawBoxMinMax(const dx::XMFLOAT3 &min, const dx::XMFLOAT3 &max, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);
	/// TODO: Describe
	void DrawBoxOBB(const dx::BoundingOrientedBox &obb, const dx::XMFLOAT4 &color, bool useDepth = true, bool twoSided = false);


	/// TODO: Describe
	void DrawTriSS(const DD::Tri &tri);
	/// TODO: Describe
	void DrawLineSS(const dx::XMFLOAT2 &p1, const dx::XMFLOAT2 &p2, const dx::XMFLOAT4 &color, float thickness, float layer);
	/// TODO: Describe
	void DrawMinMaxRect(const dx::XMFLOAT2 &min, const dx::XMFLOAT2 &max, const dx::XMFLOAT4 &color, float layer);
	/// TODO: Describe
	void DrawExtentRect(const dx::XMFLOAT2 &pos, const dx::XMFLOAT2 &extents, const dx::XMFLOAT4 &color, float layer);


	/// TODO: Describe
	void DrawSprite(UINT texID, DD::Sprite sprite, bool useDepth = true);
	/// TODO: Describe
	void DrawSprite(UINT texID, const dx::XMFLOAT4 &pos, const dx::XMFLOAT2 &size, const dx::XMFLOAT4 &color = {1,1,1,1}, dx::XMFLOAT2 uv0 = {0,0}, dx::XMFLOAT2 uv1 = {1,1}, bool useDepth = true);


	/// TODO: Describe
	void DrawSpriteSS(UINT texID, DD::Sprite sprite);
	/// TODO: Describe
	void DrawSpriteSS(UINT texID, float layer, const dx::XMFLOAT2 &pos, const dx::XMFLOAT2 &size, const dx::XMFLOAT4 &color = {1,1,1,1}, dx::XMFLOAT2 uv0 = {0,0}, dx::XMFLOAT2 uv1 = {1,1});

	TESTABLE()
};

#undef DD
