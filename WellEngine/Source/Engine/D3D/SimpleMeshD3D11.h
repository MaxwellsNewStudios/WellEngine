#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXCollision.h>
#include <vector>

#include "VertexBufferD3D11.h"
#include "IndexBufferD3D11.h"


struct SimpleVertex 
{
	float
		x, y, z, s,
		r, g, b, a;

	SimpleVertex() :
		x(0.0f), y(0.0f), z(0.0f), s(0.0f),
		r(0.0f), g(0.0f), b(0.0f), a(0.0f)
	{ }

	SimpleVertex(
		float x, float y, float z, float s,
		float r, float g, float b, float a) :
		x(x), y(y), z(z), s(s),
		r(r), g(g), b(b), a(a)
	{ }

	bool operator==(const SimpleVertex &other) const
	{
		if (x != other.x) return false;
		if (y != other.y) return false;
		if (z != other.z) return false;
		if (s != other.s) return false;

		if (r != other.r) return false;
		if (g != other.g) return false;
		if (b != other.b) return false;
		if (a != other.a) return false;

		return true;
	}
};

struct SimpleSpritePoint
{
	float
		px, py, pz, pw,
		cr, cg, cb, ca,
		tx, ty, tz, tw,
		sx, sy;

	SimpleSpritePoint() :
		px(0.0f), py(0.0f), pz(0.0f), pw(0.0f),
		cr(0.0f), cg(0.0f), cb(0.0f), ca(0.0f),
		tx(0.0f), ty(0.0f), tz(0.0f), tw(0.0f),
		sx(0.0f), sy(0.0f)
	{ }

	SimpleSpritePoint(
		float px, float py, float pz, float pw,
		float cr, float cg, float cb, float ca,
		float tx, float ty, float tz, float tw,
		float sx, float sy) :
		px(px), py(py), pz(pz), pw(pw),
		cr(cr), cg(cg), cb(cb), ca(ca),
		tx(tx), ty(ty), tz(tz), tw(tw),
		sx(sx), sy(sy)
	{ }

	bool operator==(const SimpleSpritePoint &other) const
	{
		if (px != other.px) return false;
		if (py != other.py) return false;
		if (pz != other.pz) return false;
		if (pw != other.pw) return false;

		if (cr != other.cr) return false;
		if (cg != other.cg) return false;
		if (cb != other.cb) return false;
		if (ca != other.ca) return false;

		if (tx != other.tx) return false;
		if (ty != other.ty) return false;
		if (tz != other.tz) return false;
		if (tw != other.tw) return false;

		if (sx != other.sx) return false;
		if (sy != other.sy) return false;

		return true;
	}
};

struct SimpleMeshData
{
	struct VertexInfo
	{
		UINT sizeOfVertex = 0;
		UINT nrOfVerticesInBuffer = 0;
		float *vertexData = nullptr;

		~VertexInfo()
		{
			delete[] vertexData;
		}
	} vertexInfo;
};

class SimpleMeshD3D11
{
private:
	VertexBufferD3D11 _vertexBuffer;

public:
	SimpleMeshD3D11() = default;
	~SimpleMeshD3D11() = default;
	SimpleMeshD3D11(const SimpleMeshD3D11 &other) = delete;
	SimpleMeshD3D11 &operator=(const SimpleMeshD3D11 &other) = delete;
	SimpleMeshD3D11(SimpleMeshD3D11 &&other) = delete;
	SimpleMeshD3D11 &operator=(SimpleMeshD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, const SimpleMeshData &meshInfo);

	void Reset();

	void BindMeshBuffers(ID3D11DeviceContext *context, UINT stride = 0, UINT offset = 0) const;
	void PerformDrawCall(ID3D11DeviceContext *context) const;

	TESTABLE()
};