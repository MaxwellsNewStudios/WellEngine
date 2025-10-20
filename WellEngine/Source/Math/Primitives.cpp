#include "stdafx.h"
#include "Primitives.h"

namespace Icosahedron
{
	// Source: https://schneide.blog/2016/07/15/generating-an-icosphere-in-c/

	using namespace DirectX;
	using Index = int;

	struct Triangle
	{
		Index vertex[3];
	};

	using TriangleList = std::vector<Triangle>;
	using VertexList = std::vector<XMFLOAT3>;

	const float X = .525731112119133606f;
	const float Z = .850650808352039932f;
	const float N = 0.f;

	static const VertexList IcosahedronVertices =
	{
		{-X,N,Z},	{X,N,Z},	{-X,N,-Z},	{X,N,-Z},
		{N,Z,X},	{N,Z,-X},	{N,-Z,X},	{N,-Z,-X},
		{Z,X,N},	{-Z,X, N},	{Z,-X,N},	{-Z,-X, N}
	};

	static const TriangleList IcosahedronTriangles =
	{
		{0,4,1},	{0,9,4},	{9,5,4},	{4,5,8},	{4,8,1},
		{8,10,1},	{8,3,10},	{5,3,8},	{5,2,3},	{2,7,3},
		{7,10,3},	{7,6,10},	{7,11,6},	{11,0,6},	{0,1,6},
		{6,1,10},	{9,0,11},	{9,11,2},	{9,2,5},	{7,2,11}
	};

	using Lookup = std::map<std::pair<Index, Index>, Index>;

	static Index VertexForEdge(Lookup &lookup, VertexList &vertices, Index first, Index second)
	{
		Lookup::key_type key(first, second);
		if (key.first > key.second)
			std::swap(key.first, key.second);

		auto inserted = lookup.insert({ key, vertices.size() });
		if (inserted.second)
		{
			XMFLOAT3 &edge0 = vertices[first];
			XMFLOAT3 &edge1 = vertices[second];

			XMVECTOR pointV = XMVector3Normalize(XMVectorAdd(Load(edge0), Load(edge1)));

			XMFLOAT3 point{};
			Store(point, pointV);

			vertices.push_back(point);
		}

		return inserted.first->second;
	}

	static TriangleList Subdivide(VertexList &vertices, TriangleList triangles)
	{
		Lookup lookup;
		TriangleList result;

		for (auto &&each : triangles)
		{
			std::array<Index, 3> mid{};
			for (int edge = 0; edge < 3; ++edge)
			{
				mid[edge] = VertexForEdge(lookup, vertices,
					each.vertex[edge], each.vertex[(edge + 1) % 3]);
			}

			result.push_back({ each.vertex[0], mid[0], mid[2] });
			result.push_back({ each.vertex[1], mid[1], mid[0] });
			result.push_back({ each.vertex[2], mid[2], mid[1] });
			result.push_back({ mid[0], mid[1], mid[2] });
		}

		return result;
	}

	using IndexedMesh = std::pair<VertexList, TriangleList>;

	static IndexedMesh MakeIcosphere(int subdivisions)
	{
		VertexList vertices = IcosahedronVertices;
		TriangleList triangles = IcosahedronTriangles;

		for (int i = 0; i < subdivisions; ++i)
		{
			triangles = Subdivide(vertices, triangles);
		}

		return{ vertices, triangles };
	}
};

void Primitives::GenerateIcoSphere(int subdivisions, std::vector<dx::XMFLOAT3> &outVertices, std::vector<int> &outIndices)
{
	auto [vertices, triangles] = Icosahedron::MakeIcosphere(subdivisions);

	outVertices.resize(vertices.size());
	std::copy(vertices.begin(), vertices.end(), outVertices.begin());
	outIndices.resize(triangles.size() * 3);
	for (size_t i = 0; i < triangles.size(); ++i)
	{
		outIndices[i * 3 + 0] = triangles[i].vertex[0];
		outIndices[i * 3 + 1] = triangles[i].vertex[1];
		outIndices[i * 3 + 2] = triangles[i].vertex[2];
	}
}
