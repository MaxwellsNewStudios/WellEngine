#pragma once

#include <variant>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <Debug/ErrMsg.h>
#include "Collision/ColliderShapes.h"

namespace dx = DirectX;

#define SIGN(x)	(x > 0 ? 1 : -1)

static constexpr float DEG_TO_RAD = (dx::XM_PI / 180.0f);
static constexpr float RAD_TO_DEG = (180.0f / dx::XM_PI);

[[nodiscard]] static inline const dx::XMFLOAT4A To4(const dx::XMFLOAT3A &vec) noexcept
{
	return *reinterpret_cast<const dx::XMFLOAT4A *>(&vec);
}
[[nodiscard]] static inline dx::XMFLOAT4A To4(const dx::XMFLOAT3 &vec) noexcept
{
	return dx::XMFLOAT4A(vec.x, vec.y, vec.z, 0.0f);
}
[[nodiscard]] static inline dx::XMFLOAT3A To3(const dx::XMFLOAT4A &vec) noexcept
{
	return *reinterpret_cast<const dx::XMFLOAT3A *>(&vec);
}
[[nodiscard]] static inline dx::XMFLOAT3A To3(const dx::XMFLOAT4 &vec) noexcept
{
	return *reinterpret_cast<const dx::XMFLOAT3A *>(&vec);
}
[[nodiscard]] static inline dx::XMFLOAT3A To3(const dx::XMFLOAT3 &vec) noexcept
{
	return dx::XMFLOAT3A(vec.x, vec.y, vec.z);
}
[[nodiscard]] static inline dx::XMFLOAT3 To3(const dx::XMFLOAT3A &vec) noexcept
{
	return dx::XMFLOAT3(vec.x, vec.y, vec.z);
}
[[nodiscard]] static inline dx::XMFLOAT2A To2(const dx::XMFLOAT4 &vec) noexcept
{
	return *reinterpret_cast<const dx::XMFLOAT2A *>(&vec);
}
[[nodiscard]] static inline dx::XMFLOAT2A To2(const dx::XMFLOAT3 &vec) noexcept
{
	return dx::XMFLOAT2A(vec.x, vec.y);
}
[[nodiscard]] static inline dx::XMFLOAT2 To2(const dx::XMFLOAT3A &vec) noexcept
{
	return dx::XMFLOAT2(vec.x, vec.y);
}
[[nodiscard]] static inline dx::XMFLOAT2A To2(const dx::XMFLOAT2 &vec) noexcept
{
	return dx::XMFLOAT2A(vec.x, vec.y);
}
[[nodiscard]] static inline dx::XMFLOAT2 To2(const dx::XMFLOAT2A &vec) noexcept
{
	return dx::XMFLOAT2(vec.x, vec.y);
}
[[nodiscard]] static inline float To1(const dx::XMFLOAT4A &vec) noexcept
{
	return vec.x;
}
[[nodiscard]] static inline float To1(const dx::XMFLOAT3A &vec) noexcept
{
	return vec.x;
}
[[nodiscard]] static inline float To1(const dx::XMFLOAT4 &vec) noexcept
{
	return vec.x;
}
[[nodiscard]] static inline float To1(const dx::XMFLOAT3 &vec) noexcept
{
	return vec.x;
}

[[nodiscard]] static inline dx::XMFLOAT4 ToFloat(const dx::XMUINT4 &vec) noexcept
{
	return { (float)vec.x, (float)vec.y, (float)vec.z, (float)vec.w };
}
[[nodiscard]] static inline dx::XMFLOAT4 ToFloat(const dx::XMINT4 &vec) noexcept
{
	return { (float)vec.x, (float)vec.y, (float)vec.z, (float)vec.w };
}
[[nodiscard]] static inline dx::XMFLOAT3 ToFloat(const dx::XMUINT3 &vec) noexcept
{
	return { (float)vec.x, (float)vec.y, (float)vec.z };
}
[[nodiscard]] static inline dx::XMFLOAT3 ToFloat(const dx::XMINT3 &vec) noexcept
{
	return { (float)vec.x, (float)vec.y, (float)vec.z };
}
[[nodiscard]] static inline dx::XMFLOAT2 ToFloat(const dx::XMUINT2 &vec) noexcept
{
	return { (float)vec.x, (float)vec.y };
}
[[nodiscard]] static inline dx::XMFLOAT2 ToFloat(const dx::XMINT2 &vec) noexcept
{
	return { (float)vec.x, (float)vec.y };
}

[[nodiscard]] static inline dx::XMUINT4 ToUint(const dx::XMFLOAT4A &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y, (UINT)vec.z, (UINT)vec.w };
}
[[nodiscard]] static inline dx::XMUINT4 ToUint(const dx::XMFLOAT4 &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y, (UINT)vec.z, (UINT)vec.w };
}
[[nodiscard]] static inline dx::XMUINT4 ToUint(const dx::XMINT4 &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y, (UINT)vec.z, (UINT)vec.w };
}
[[nodiscard]] static inline dx::XMUINT3 ToUint(const dx::XMFLOAT3A &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y, (UINT)vec.z };
}
[[nodiscard]] static inline dx::XMUINT3 ToUint(const dx::XMFLOAT3 &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y, (UINT)vec.z };
}
[[nodiscard]] static inline dx::XMUINT3 ToUint(const dx::XMINT3 &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y, (UINT)vec.z };
}
[[nodiscard]] static inline dx::XMUINT2 ToUint(const dx::XMFLOAT2A &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y };
}
[[nodiscard]] static inline dx::XMUINT2 ToUint(const dx::XMFLOAT2 &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y };
}
[[nodiscard]] static inline dx::XMUINT2 ToUint(const dx::XMINT2 &vec) noexcept
{
	return { (UINT)vec.x, (UINT)vec.y };
}

[[nodiscard]] static inline dx::XMINT4 ToInt(const dx::XMFLOAT4A &vec) noexcept
{
	return { (int)vec.x, (int)vec.y, (int)vec.z, (int)vec.w };
}
[[nodiscard]] static inline dx::XMINT4 ToInt(const dx::XMFLOAT4 &vec) noexcept
{
	return { (int)vec.x, (int)vec.y, (int)vec.z, (int)vec.w };
}
[[nodiscard]] static inline dx::XMINT4 ToInt(const dx::XMUINT4 &vec) noexcept
{
	return { (int)vec.x, (int)vec.y, (int)vec.z, (int)vec.w };
}
[[nodiscard]] static inline dx::XMINT3 ToInt(const dx::XMFLOAT3A &vec) noexcept
{
	return { (int)vec.x, (int)vec.y, (int)vec.z };
}
[[nodiscard]] static inline dx::XMINT3 ToInt(const dx::XMFLOAT3 &vec) noexcept
{
	return { (int)vec.x, (int)vec.y, (int)vec.z };
}
[[nodiscard]] static inline dx::XMINT3 ToInt(const dx::XMUINT3 &vec) noexcept
{
	return { (int)vec.x, (int)vec.y, (int)vec.z };
}
[[nodiscard]] static inline dx::XMINT2 ToInt(const dx::XMFLOAT2A &vec) noexcept
{
	return { (int)vec.x, (int)vec.y };
}
[[nodiscard]] static inline dx::XMINT2 ToInt(const dx::XMFLOAT2 &vec) noexcept
{
	return { (int)vec.x, (int)vec.y };
}
[[nodiscard]] static inline dx::XMINT2 ToInt(const dx::XMUINT2 &vec) noexcept
{
	return { (int)vec.x, (int)vec.y };
}

[[nodiscard]] static inline dx::XMVECTOR Load(float f) noexcept
{
	return dx::XMLoadFloat(&f);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT2 &float2) noexcept
{
	return dx::XMLoadFloat2(&float2);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT3 &float3) noexcept
{
	return dx::XMLoadFloat3(&float3);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT4 &float4) noexcept
{
	return dx::XMLoadFloat4(&float4);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT3A &float3A) noexcept
{
	return dx::XMLoadFloat3A(&float3A);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT4A &float4A) noexcept
{
	return dx::XMLoadFloat4A(&float4A);
}
[[nodiscard]] static inline dx::XMMATRIX Load(const dx::XMFLOAT3X3 &float3x3) noexcept
{
	return dx::XMLoadFloat3x3(&float3x3);
}
[[nodiscard]] static inline dx::XMMATRIX Load(const dx::XMFLOAT4X4 &float4x4) noexcept
{
	return dx::XMLoadFloat4x4(&float4x4);
}
[[nodiscard]] static inline dx::XMMATRIX Load(const dx::XMFLOAT4X4A &float4x4A) noexcept
{
	return dx::XMLoadFloat4x4A(&float4x4A);
}

[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT2 *float2) noexcept
{
	return dx::XMLoadFloat2(float2);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT3 *float3) noexcept
{
	return dx::XMLoadFloat3(float3);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT4 *float4) noexcept
{
	return dx::XMLoadFloat4(float4);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT3A *float3A) noexcept
{
	return dx::XMLoadFloat3A(float3A);
}
[[nodiscard]] static inline dx::XMVECTOR Load(const dx::XMFLOAT4A *float4A) noexcept
{
	return dx::XMLoadFloat4A(float4A);
}
[[nodiscard]] static inline dx::XMMATRIX Load(const dx::XMFLOAT3X3 *float3x3) noexcept
{
	return dx::XMLoadFloat3x3(float3x3);
}
[[nodiscard]] static inline dx::XMMATRIX Load(const dx::XMFLOAT4X4 *float4x4) noexcept
{
	return dx::XMLoadFloat4x4(float4x4);
}
[[nodiscard]] static inline dx::XMMATRIX Load(const dx::XMFLOAT4X4A *float4x4A) noexcept
{
	return dx::XMLoadFloat4x4A(float4x4A);
}

static inline void Store(float &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat(&dest, vec);
}
static inline void Store(dx::XMFLOAT2 &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat2(&dest, vec);
}
static inline void Store(dx::XMFLOAT3 &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat3(&dest, vec);
}
static inline void Store(dx::XMFLOAT4 &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat4(&dest, vec);
}
static inline void Store(dx::XMFLOAT2A &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat2A(&dest, vec);
}
static inline void Store(dx::XMFLOAT3A &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat3A(&dest, vec);
}
static inline void Store(dx::XMFLOAT4A &dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat4A(&dest, vec);
}
static inline void Store(dx::XMFLOAT3X3 &dest, const dx::XMMATRIX &mat) noexcept
{
	dx::XMStoreFloat3x3(&dest, mat);
}
static inline void Store(dx::XMFLOAT4X4 &dest, const dx::XMMATRIX &mat) noexcept
{
	dx::XMStoreFloat4x4(&dest, mat);
}
static inline void Store(dx::XMFLOAT4X4A &dest, const dx::XMMATRIX &mat) noexcept
{
	dx::XMStoreFloat4x4A(&dest, mat);
}
static inline void Store(float *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat(dest, vec);
}
static inline void Store(dx::XMFLOAT2 *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat2(dest, vec);
}
static inline void Store(dx::XMFLOAT3 *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat3(dest, vec);
}
static inline void Store(dx::XMFLOAT4 *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat4(dest, vec);
}
static inline void Store(dx::XMFLOAT2A *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat2A(dest, vec);
}
static inline void Store(dx::XMFLOAT3A *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat3A(dest, vec);
}
static inline void Store(dx::XMFLOAT4A *dest, dx::XMVECTOR vec) noexcept
{
	dx::XMStoreFloat4A(dest, vec);
}
static inline void Store(dx::XMFLOAT3X3 *dest, const dx::XMMATRIX &mat) noexcept
{
	dx::XMStoreFloat3x3(dest, mat);
}
static inline void Store(dx::XMFLOAT4X4 *dest, const dx::XMMATRIX &mat) noexcept
{
	dx::XMStoreFloat4x4(dest, mat);
}
static inline void Store(dx::XMFLOAT4X4A *dest, const dx::XMMATRIX &mat) noexcept
{
	dx::XMStoreFloat4x4A(dest, mat);
}


static inline int Wrap(int val, int min, int max)
{
	int length = max - min + 1;

	if (val < min)
		val += length * ((min - val) / length + 1);

	return min + (val - min) % length;
}
static inline float Wrap(float val, float min, float max)
{
	float length = max - min + 1.0f;

	if (val < min)
		val += length * ((min - val) / length + 1.0f);

	return min + fmodf(val - min, length);
}
static inline double Wrap(double val, double min, double max)
{
	double length = max - min + 1.0;

	if (val < min)
		val += length * ((min - val) / length + 1.0);

	return min + fmod(val - min, length);
}

template<typename T>
[[nodiscard]] static inline size_t NumericLimit() noexcept
{
#undef max
	return std::numeric_limits<T>::max();
#define max(a, b) (((a) > (b)) ? (a) : (b))
};

template<typename T>
[[nodiscard]] static inline size_t NumericLimit(T number) noexcept
{
#undef max
	return std::numeric_limits<T>::max();
#define max(a, b) (((a) > (b)) ? (a) : (b))
};

/// Calculate the reach of a light based on its falloff and color value.
/// This is done to prevent harsh edges between light tiles.
/// Shaders match this cutoff  by using CutoffLight() in LightData.hlsli.
[[nodiscard]] static inline float CalculateLightReach(dx::XMFLOAT3 color, float falloff) noexcept
{
	// The intensity at which the light is considered to have no effect.
	// Must match the value of the same name in CutoffLight().
	float intensityCutoff = 0.02f;

	float value = max(color.x, max(color.y, color.z));

	// intensity = value / (1 + (distance * falloff)^2)
	return sqrt((value / intensityCutoff) - 1.0f) / falloff;
};

[[nodiscard]] static inline dx::XMFLOAT3 RGBtoHSV(dx::XMFLOAT3 rgb)
{
	float
		r = rgb.x,
		g = rgb.y,
		b = rgb.z;

	float max = max(r, max(g, b));
	float min = min(r, min(g, b));
	float diff = max - min;

	float h = 0.0, s, v;

	if (max == min) 	h = 0.0f;
	else if (max == r)	h = fmodf((60.0f * ((g - b) / diff) + 360.0f), 360.0f);
	else if (max == g)	h = fmodf((60.0f * ((b - r) / diff) + 120.0f), 360.0f);
	else if (max == b)	h = fmodf((60.0f * ((r - g) / diff) + 240.0f), 360.0f);
	else				h = 0.0f;

	s = (max == 0.0f) ? (0.0f) : ((diff / max) * 1.0f);
	v = max;

	return { h, s, v };
};
[[nodiscard]] static inline dx::XMFLOAT3 HSVtoRGB(dx::XMFLOAT3 hsv)
{
	float
		r = 0.0f,
		g = 0.0f,
		b = 0.0f;

	if (hsv.y == 0.0f)
	{
		r = hsv.z;
		g = hsv.z;
		b = hsv.z;
	}
	else
	{
		int i;
		float f, p, q, t;

		if (hsv.x == 360.0f)
			hsv.x = 0.0f;
		else
			hsv.x = hsv.x / 60.0f;

		i = (int)trunc(hsv.x);
		f = hsv.x - i;

		p = hsv.z * (1.0f - hsv.y);
		q = hsv.z * (1.0f - (hsv.y * f));
		t = hsv.z * (1.0f - (hsv.y * (1.0f - f)));

		switch (i)
		{
		case 0:
			r = hsv.z;
			g = t;
			b = p;
			break;

		case 1:
			r = q;
			g = hsv.z;
			b = p;
			break;

		case 2:
			r = p;
			g = hsv.z;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = hsv.z;
			break;

		case 4:
			r = t;
			g = p;
			b = hsv.z;
			break;

		default:
			r = hsv.z;
			g = p;
			b = q;
			break;
		}

	}

	return {
		r,
		g,
		b
	};
};

[[nodiscard]] static inline float RandomFloat(float min, float max)
{
	return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
};

[[nodiscard]] static inline dx::BoundingBox OBBtoAABB(const dx::BoundingOrientedBox &obb) noexcept
{
	dx::XMFLOAT3 corners[8];
	obb.GetCorners(corners);

	dx::XMVECTOR cornersV[8]{};
	for (int i = 0; i < 8; i++)
		cornersV[i] = Load(corners[i]);

	dx::XMVECTOR
		min = { FLT_MAX, FLT_MAX, FLT_MAX },
		max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (int i = 0; i < 8; i++)
	{
		min = dx::XMVectorMin(min, Load(corners[i]));
		max = dx::XMVectorMax(max, Load(corners[i]));
	}

	dx::BoundingBox axisAlignedBounds;
	dx::BoundingBox().CreateFromPoints(axisAlignedBounds, min, max);

	return axisAlignedBounds;
}

[[nodiscard]] static inline dx::BoundingBox MergeBounds(const dx::BoundingOrientedBox &a, const dx::BoundingOrientedBox &b) noexcept
{
	dx::XMFLOAT3 corners[16]{};
	a.GetCorners(&(corners[0])); // fills corners[0] to corners[7]
	b.GetCorners(&(corners[8])); // fills corners[8] to corners[15]

	dx::XMVECTOR cornersV[16]{};
	for (int i = 0; i < 16; i++)
		cornersV[i] = Load(corners[i]);

	dx::XMVECTOR
		min = { FLT_MAX, FLT_MAX, FLT_MAX },
		max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (int i = 0; i < 16; i++)
	{
		min = dx::XMVectorMin(min, Load(corners[i]));
		max = dx::XMVectorMax(max, Load(corners[i]));
	}

	dx::BoundingBox axisAlignedBounds;
	dx::BoundingBox().CreateFromPoints(axisAlignedBounds, min, max);

	return axisAlignedBounds;
}


namespace DXMath
{
	/// <summary>
	/// <para/> Vec is a wrapper for DirectXMath, merging storage and 
	/// operator types with the "power" of unions.
	/// 
	/// <para/> While Vec is more convenient compared to their wrapped 
	/// types, you must still ensure that it is used properly.
	/// 
	/// <para/> Before using Vec in maths operations, you must call 
	/// ToOp() to convert it to its operator state. 
	/// 
	/// <para/> While not in use, Vec should be in its storage state 
	/// (ToMem()). This is the state it is in by default, unless 
	/// constructed with an operator type.
	/// </summary>
	template<typename M>
	struct Vec
	{
	private:
		using Op = dx::XMVECTOR;
		union U {
			M mem = {};
			Op op;
		} _v;

		[[nodiscard]] inline operator U() const noexcept            { return _v; }
		[[nodiscard]] inline operator U &() noexcept                { return _v; }
		[[nodiscard]] inline operator const U &() const noexcept    { return _v; }

	public:
		inline Vec() noexcept = default;
		inline Vec(float x, float y = 0.f, float z = 0.f, float w = 0.f) noexcept	{ Store(_v.mem, dx::XMVectorSet(x,y,z,w));	}
		inline Vec(const M &value) noexcept											{ _v.mem = value;							}
		inline Vec(Op value) noexcept												{ _v.op = value;							}
		inline Vec(const Vec &other) noexcept										{ _v = other._v;							}
		inline Vec(Vec &&other) noexcept											{ _v = std::move(other._v);					}

		inline Vec &ToMem() noexcept    { Store(_v.mem, _v.op); return *this; }
		inline Vec &ToOp() noexcept		{ _v.op = Load(_v.mem); return *this; }

		inline M AsMem() const noexcept	{ M m; Store(m, _v.op); return m; }
		inline Op AsOp() const noexcept	{ return Load(_v.mem);			  }

		[[nodiscard]] inline M &Get() noexcept  { return _v.mem; }
		[[nodiscard]] inline const M &Get() const noexcept  { return _v.mem; }
		template<typename... Args>
		inline void Set(Args&&... args)         { _v.mem = M(std::forward<Args>(args)...); }

		[[nodiscard]] inline Op &GetOp() noexcept               { return _v.op; }
		template<typename... Args>
		inline void SetOp(Args&&... args)                       { _v.op = Load(M(std::forward<Args>(args)...)); }
		inline void SetOp(float x, float y, float z, float w)   { _v.op = dx::XMVectorSet(x, y, z, w);          }

		// Operators
		[[nodiscard]] inline operator M() const noexcept            { return _v.mem; }
		[[nodiscard]] inline operator M &() noexcept                { return _v.mem; }
		[[nodiscard]] inline operator const M &() const noexcept    { return _v.mem; }
		[[nodiscard]] inline operator Op() const noexcept           { return _v.op;  }
		[[nodiscard]] inline operator Op &() noexcept               { return _v.op;  }
		[[nodiscard]] inline operator const Op &() const noexcept   { return _v.op;  }

		[[nodiscard]] inline Vec operator-() const noexcept					{ return Vec(dx::XMVectorNegate((Op)*this));			  }
		[[nodiscard]] inline Vec operator+(const Vec &other) const noexcept { return Vec(dx::XMVectorAdd((Op)*this, (Op)other));      }
		[[nodiscard]] inline Vec operator-(const Vec &other) const noexcept { return Vec(dx::XMVectorSubtract((Op)*this, (Op)other)); }
		[[nodiscard]] inline Vec operator*(const Vec &other) const noexcept { return Vec(dx::XMVectorMultiply((Op)*this, (Op)other)); }
		[[nodiscard]] inline Vec operator/(const Vec &other) const noexcept { return Vec(dx::XMVectorDivide((Op)*this, (Op)other));   }
		[[nodiscard]] inline Vec operator*(float other) const noexcept		{ return Vec(dx::XMVectorScale((Op)*this, other));		  }
		[[nodiscard]] inline Vec operator/(float other) const noexcept		{ return Vec(dx::XMVectorScale((Op)*this, 1.0f / other)); }

		inline Vec &operator=(const M &value) noexcept      { _v.mem = value;           return *this; }
		inline Vec &operator=(Op value) noexcept			{ _v.op = value;            return *this; }
		inline Vec &operator=(const Vec &other) noexcept    { _v = other._v;            return *this; }
		inline Vec &operator=(Vec &&other) noexcept         { _v = std::move(other._v); return *this; }

		inline Vec &operator+=(const Vec &other) noexcept   { return *this = *this + other; }
		inline Vec &operator-=(const Vec &other) noexcept   { return *this = *this - other; }
		inline Vec &operator*=(const Vec &other) noexcept   { return *this = *this * other; }
		inline Vec &operator/=(const Vec &other) noexcept   { return *this = *this / other; }
		inline Vec &operator*=(float other) noexcept		{ return *this = *this * other; }
		inline Vec &operator/=(float other) noexcept		{ return *this = *this / other; }

		[[nodiscard]] inline bool operator==(const Vec &other) const noexcept { return dx::XMVector4Equal((Op)*this, (Op)other); }
		[[nodiscard]] inline bool operator!=(const Vec &other) const noexcept { return !(*this == other); }

		// DirectXMath functions
		[[nodiscard]] inline float Length2() const noexcept { return dx::XMVectorGetX(dx::XMVector2Length((Op)*this)); }
		[[nodiscard]] inline float Length3() const noexcept { return dx::XMVectorGetX(dx::XMVector3Length((Op)*this)); }
		[[nodiscard]] inline float Length4() const noexcept { return dx::XMVectorGetX(dx::XMVector4Length((Op)*this)); }
		[[nodiscard]] inline float LengthSqr2() const noexcept { return dx::XMVectorGetX(dx::XMVector2LengthSq((Op)*this)); }
		[[nodiscard]] inline float LengthSqr3() const noexcept { return dx::XMVectorGetX(dx::XMVector3LengthSq((Op)*this)); }
		[[nodiscard]] inline float LengthSqr4() const noexcept { return dx::XMVectorGetX(dx::XMVector4LengthSq((Op)*this)); }
		[[nodiscard]] inline float LengthEst2() const noexcept { return dx::XMVectorGetX(dx::XMVector2LengthEst((Op)*this)); }
		[[nodiscard]] inline float LengthEst3() const noexcept { return dx::XMVectorGetX(dx::XMVector3LengthEst((Op)*this)); }
		[[nodiscard]] inline float LengthEst4() const noexcept { return dx::XMVectorGetX(dx::XMVector4LengthEst((Op)*this)); }

		[[nodiscard]] inline Vec Normalized2() const noexcept { return Vec(dx::XMVector2Normalize((Op)*this)); }
		[[nodiscard]] inline Vec Normalized3() const noexcept { return Vec(dx::XMVector3Normalize((Op)*this)); }
		[[nodiscard]] inline Vec Normalized4() const noexcept { return Vec(dx::XMVector4Normalize((Op)*this)); }
		[[nodiscard]] inline Vec NormalizedEst2() const noexcept { return Vec(dx::XMVector2NormalizeEst((Op)*this)); }
		[[nodiscard]] inline Vec NormalizedEst3() const noexcept { return Vec(dx::XMVector3NormalizeEst((Op)*this)); }
		[[nodiscard]] inline Vec NormalizedEst4() const noexcept { return Vec(dx::XMVector4NormalizeEst((Op)*this)); }

		inline void Normalize2() noexcept { *this = this->Normalized2(); }
		inline void Normalize3() noexcept { *this = this->Normalized3(); }
		inline void Normalize4() noexcept { *this = this->Normalized4(); }
		inline void NormalizeEst2() noexcept { *this = this->NormalizedEst2(); }
		inline void NormalizeEst3() noexcept { *this = this->NormalizedEst3(); }
		inline void NormalizeEst4() noexcept { *this = this->NormalizedEst4(); }

		[[nodiscard]] inline float Dot2(const Vec &other) const noexcept { return dx::XMVectorGetX(dx::XMVector2Dot((Op)*this, (Op)other)); }
		[[nodiscard]] inline float Dot3(const Vec &other) const noexcept { return dx::XMVectorGetX(dx::XMVector3Dot((Op)*this, (Op)other)); }
		[[nodiscard]] inline float Dot4(const Vec &other) const noexcept { return dx::XMVectorGetX(dx::XMVector4Dot((Op)*this, (Op)other)); }

		[[nodiscard]] inline Vec Cross3(const Vec &other) const noexcept { return Vec(dx::XMVector3Cross((Op)*this, (Op)other)); }
		[[nodiscard]] inline Vec Cross4(const Vec &other1, const Vec &other2) const noexcept { return Vec(dx::XMVector4Cross((Op)*this, (Op)other1, (Op)other2)); }

		[[nodiscard]] inline Vec MultiplyAdd(const Vec &otherMult, const Vec &otherAdd) const noexcept { return Vec(dx::XMVectorMultiplyAdd((Op)*this, (Op)otherMult, (Op)otherAdd)); }

		// Static methods
		[[nodiscard]] static inline Vec Zero() noexcept  { return Vec(0.f, 0.f, 0.f, 0.f); }
		[[nodiscard]] static inline Vec One() noexcept   { return Vec(1.f, 1.f, 1.f, 1.f); }
		[[nodiscard]] static inline Vec UnitX() noexcept { return Vec(1.f, 0.f, 0.f, 0.f); }
		[[nodiscard]] static inline Vec UnitY() noexcept { return Vec(0.f, 1.f, 0.f, 0.f); }
		[[nodiscard]] static inline Vec UnitZ() noexcept { return Vec(0.f, 0.f, 1.f, 0.f); }
		[[nodiscard]] static inline Vec UnitW() noexcept { return Vec(0.f, 0.f, 0.f, 1.f); }

		[[nodiscard]] static inline float Dot2(const Vec &v1, const Vec &v2) noexcept { return v1.Dot2(v2); }
		[[nodiscard]] static inline float Dot3(const Vec &v1, const Vec &v2) noexcept { return v1.Dot3(v2); }
		[[nodiscard]] static inline float Dot4(const Vec &v1, const Vec &v2) noexcept { return v1.Dot4(v2); }

		[[nodiscard]] static inline Vec Cross3(const Vec &v1, const Vec &v2) noexcept { return v1.Cross3(v2); }
		[[nodiscard]] static inline Vec Cross4(const Vec &v1, const Vec &v2, const Vec &v3) noexcept { return v1.Cross4(v2, v3); }

		[[nodiscard]] static inline Vec MultiplyAdd(const Vec &v1, const Vec &v2, const Vec &v3) noexcept { return v1.MultiplyAdd(v2, v3); }

		TESTABLE();
	};

	/// <summary> See Vec for usage </summary>
	using Vec2 = Vec<dx::XMFLOAT2>;
	/// <summary> See Vec for usage </summary>
	using Vec3 = Vec<dx::XMFLOAT3>;
	/// <summary> See Vec for usage </summary>
	using Vec4 = Vec<dx::XMFLOAT4>;
	/// <summary> See Vec for usage </summary>
	using Vec2A = Vec<dx::XMFLOAT2A>;
	/// <summary> See Vec for usage </summary>
	using Vec3A = Vec<dx::XMFLOAT3A>;
	/// <summary> See Vec for usage </summary>
	using Vec4A = Vec<dx::XMFLOAT4A>;


	struct Quat
	{
	private:
		using M = dx::XMFLOAT4;
		using Op = dx::XMVECTOR;
		union U {
			M mem = {};
			Op op;
		} _v;

		[[nodiscard]] inline operator U() const noexcept { return _v; }
		[[nodiscard]] inline operator U &() noexcept { return _v; }
		[[nodiscard]] inline operator const U &() const noexcept { return _v; }

	public:
		inline Quat() noexcept = default;
		inline Quat(const M &value) noexcept	{ _v.mem = value; }
		inline Quat(Op value) noexcept			{ _v.op = value; }
		inline Quat(const Quat &other) noexcept { _v = other._v; }
		inline Quat(Quat &&other) noexcept		{ _v = std::move(other._v); }

		inline void ToMem() noexcept { Store(_v.mem, _v.op); }
		inline void ToOp() noexcept { _v.op = Load(_v.mem); }

		[[nodiscard]] inline M &Get() noexcept { return _v.mem; }
		inline void Set(float x, float y, float z, float w) { _v.mem = M(x, y, z, w); }

		[[nodiscard]] inline Op &GetOp() noexcept { return _v.op; }
		inline void SetOp(float x, float y, float z, float w) { _v.op = dx::XMVectorSet(x, y, z, w); }

		// Operators
		[[nodiscard]] inline operator M() const noexcept { return _v.mem; }
		[[nodiscard]] inline operator M &() noexcept { return _v.mem; }
		[[nodiscard]] inline operator const M &() const noexcept { return _v.mem; }
		[[nodiscard]] inline operator Op() const noexcept { return _v.op; }
		[[nodiscard]] inline operator Op &() noexcept { return _v.op; }
		[[nodiscard]] inline operator const Op &() const noexcept { return _v.op; }

		[[nodiscard]] inline Quat operator-() const noexcept				  { return Quat(dx::XMQuaternionInverse((Op)*this));			  }
		[[nodiscard]] inline Quat operator*(const Quat &other) const noexcept { return Quat(dx::XMQuaternionMultiply((Op)*this, (Op)other));  }
		[[nodiscard]] inline Quat operator/(const Quat &other) const noexcept { return Quat(dx::XMQuaternionMultiply((Op)*this, (Op)-other)); }

		inline Quat &operator=(const M &value) noexcept		{ _v.mem = value;           return *this; }
		inline Quat &operator=(Op value) noexcept			{ _v.op = value;            return *this; }
		inline Quat &operator=(const Quat &other) noexcept	{ _v = other._v;            return *this; }
		inline Quat &operator=(Quat &&other) noexcept		{ _v = std::move(other._v); return *this; }

		inline Quat &operator*=(const Quat &other) noexcept { return *this = *this * other; }
		inline Quat &operator/=(const Quat &other) noexcept { return *this = *this / other; }

		[[nodiscard]] inline bool operator==(const Quat &other) const noexcept { return dx::XMQuaternionEqual((Op)*this, (Op)other); }
		[[nodiscard]] inline bool operator!=(const Quat &other) const noexcept { return !(*this == other); }

		// DirectXMath functions
		[[nodiscard]] inline float Length() const noexcept { return dx::XMVectorGetX(dx::XMQuaternionLength((Op)*this)); }
		[[nodiscard]] inline float LengthSqr() const noexcept { return dx::XMVectorGetX(dx::XMQuaternionLengthSq((Op)*this)); }

		[[nodiscard]] inline Quat Normalized() const noexcept	 { return Quat(dx::XMQuaternionNormalize((Op)*this));	 }
		[[nodiscard]] inline Quat NormalizedEst() const noexcept { return Quat(dx::XMQuaternionNormalizeEst((Op)*this)); }

		inline void Normalize() noexcept	{ *this = this->Normalized();	 }
		inline void NormalizeEst() noexcept { *this = this->NormalizedEst(); }

		[[nodiscard]] inline float Dot(const Quat &other) const noexcept { return dx::XMVectorGetX(dx::XMQuaternionDot((Op)*this, (Op)other)); }

		//[[nodiscard]] inline float _(const Quat &other) const noexcept { return dx::; }
		
		// Static methods
		[[nodiscard]] static inline Quat Identity() noexcept { return Quat(dx::XMQuaternionIdentity()); }

		[[nodiscard]] static inline float Dot(const Quat &q1, const Quat &q2) noexcept { return q1.Dot(q2); }

		template<typename V>
		[[nodiscard]] static inline Quat RotationAxis(const Vec<V> &axis, float angle) noexcept { return Quat(dx::XMQuaternionRotationAxis((Op)axis, angle)); }
		template<typename V>
		[[nodiscard]] static inline Quat RotationNormal(const Vec<V> &normal, float angle) noexcept { return Quat(dx::XMQuaternionRotationNormal((Op)normal, angle)); }

		template<typename V>
		static inline void ToAxisAngle(Vec<V> *normal, float *angle, const Quat &quat) noexcept 
		{ 
			Assert(normal, "Normal must not be nullptr");
			Assert(angle, "Angle must not be nullptr");

			dx::XMQuaternionToAxisAngle(((Op *)normal), angle, quat); 
		}

		TESTABLE();
	};
}


namespace GameCollision
{
	static inline bool _IntersectsTriangleAABBSat(const Shape::Tri &tri, const DXMath::Vec3 &extent, const DXMath::Vec3 &axis)
	{
		using namespace DXMath;

		Vec3 v0 = Load(tri.v0);
		Vec3 v1 = Load(tri.v1);
		Vec3 v2 = Load(tri.v2);

		const float p0 = v0.Dot3(axis);
		const float p1 = v1.Dot3(axis);
		const float p2 = v2.Dot3(axis);

		const dx::XMFLOAT3 &extentMem = extent.Get();

		const float r = extentMem.x * std::abs(Vec3::UnitX().ToOp().Dot3(axis)) +
			extentMem.y * std::abs(Vec3::UnitY().ToOp().Dot3(axis)) +
			extentMem.z * std::abs(Vec3::UnitZ().ToOp().Dot3(axis));

		const float minP = min(p0, min(p1, p2));
		const float maxP = max(p0, max(p1, p2));

		return !(max(-maxP, minP) > r);
	}
	static inline bool _IntersectsTriangleAABBB(const Shape::Tri &tri, const dx::BoundingBox &aabb)
	{
		using namespace DXMath;

		Vec3 v0 = Load(tri.v0);
		Vec3 v1 = Load(tri.v1);
		Vec3 v2 = Load(tri.v2);

		Vec3 boxCenter = Load(aabb.Center);
		Vec3 boxExtent = aabb.Extents;

		Vec3 a = v0 - boxCenter;
		Vec3 b = v1 - boxCenter;
		Vec3 c = v2 - boxCenter;

		Vec3 ab = b - a;
		Vec3 bc = c - b;
		Vec3 ca = a - c;

		ab.Normalize3();
		bc.Normalize3();
		ca.Normalize3();

		dx::XMFLOAT3 abMem = ab.AsMem();
		dx::XMFLOAT3 bcMem = bc.AsMem();
		dx::XMFLOAT3 caMem = ca.AsMem();

		// Cross AB, BV and CA with (1, 0, 0)
		Vec3 a00 = Vec3(0, -abMem.z, abMem.y);
		Vec3 a01 = Vec3(0, -bcMem.z, bcMem.y);
		Vec3 a02 = Vec3(0, -caMem.z, caMem.y);

		// Cross AB, BC and CA with (0, 1, 0)
		Vec3 a10 = Vec3(abMem.z, 0, -abMem.x);
		Vec3 a11 = Vec3(bcMem.z, 0, -bcMem.x);
		Vec3 a12 = Vec3(caMem.z, 0, -caMem.x);

		// Cross AB, BC and CA with (0, 0, 1)
		Vec3 a20 = Vec3(-abMem.y, abMem.x, 0);
		Vec3 a21 = Vec3(-bcMem.y, bcMem.x, 0);
		Vec3 a22 = Vec3(-caMem.y, caMem.x, 0);

		a.ToMem(); b.ToMem(); c.ToMem();
		Shape::Tri TranslatedTriangle(a.Get(), b.Get(), c.Get());

		return _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a00.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a01.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a02.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a10.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a11.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a12.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a20.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a21.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, a22.ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, Vec3::UnitX().ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, Vec3::UnitY().ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, Vec3::UnitZ().ToOp()) &&
			   _IntersectsTriangleAABBSat(TranslatedTriangle, boxExtent, ab.Cross3(bc));
	}
	static inline bool BoxTriIntersect(const dx::BoundingBox &box, const Shape::Tri &tri)
	{
		using namespace DXMath;

		dx::BoundingBox triBounds;
		dx::BoundingBox::CreateFromPoints(triBounds, 3, (dx::XMFLOAT3 *)(&tri.v0), sizeof(dx::XMFLOAT3));

		auto intersectResult = box.Contains(triBounds);
		if (intersectResult == dx::DISJOINT)
			return false;
		if (intersectResult == dx::CONTAINS)
			return true;

		return _IntersectsTriangleAABBB(tri, box);
	}
}
