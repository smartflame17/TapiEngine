#pragma once
#include "IndexedTriangleList.h"
#include <DirectXMath.h>

class Cube
{
public:
	// Creates an IndexedTriangleList from vertex data
	// template V must contain pos (position data) at least
	template<class V>
	static IndexedTriangleList<V> Make()
	{
		namespace dx = DirectX;

		constexpr float side = 1.0f / 2.0f;

		std::vector<dx::XMFLOAT3> vertices;
		vertices.emplace_back(-side, -side, -side); // 0
		vertices.emplace_back(side, -side, -side); // 1
		vertices.emplace_back(-side, side, -side); // 2
		vertices.emplace_back(side, side, -side); // 3
		vertices.emplace_back(-side, -side, side); // 4
		vertices.emplace_back(side, -side, side); // 5
		vertices.emplace_back(-side, side, side); // 6
		vertices.emplace_back(side, side, side); // 7

		std::vector<V> verts(vertices.size());
		for (size_t i = 0; i < vertices.size(); i++)
		{
			verts[i].pos = vertices[i];
		}
		return{
			std::move(verts),{
				0,2,1, 2,3,1,
				1,3,5, 3,7,5,
				2,6,3, 3,6,7,
				4,5,7, 4,7,6,
				0,4,2, 2,4,6,
				0,1,4, 1,5,4
			}
		};
	}

	// 24-vertex cube (independent faces). 
	// Allows each face to have its own normal/texture coordinates.
	// Template V must support .pos (XMFLOAT3) and .tex (XMFLOAT2)
	template<class V>
	static IndexedTriangleList<V> MakeTextured()
	{
		namespace dx = DirectX;
		constexpr float side = 1.0f / 2.0f;

		std::vector<V> vertices(24);

		// Lambda to set pos and tex easily
		auto SetVert = [&vertices](int i, float x, float y, float z, float u, float v) {
			vertices[i].pos = dx::XMFLOAT3(x, y, z);
			vertices[i].tex = dx::XMFLOAT2(u, v);
			};

		// Front Face
		SetVert(0, -side, -side, -side, 0.0f, 1.0f);
		SetVert(1, -side, side, -side, 0.0f, 0.0f);
		SetVert(2, side, side, -side, 1.0f, 0.0f);
		SetVert(3, side, -side, -side, 1.0f, 1.0f);

		// Right Face
		SetVert(4, side, -side, -side, 0.0f, 1.0f);
		SetVert(5, side, side, -side, 0.0f, 0.0f);
		SetVert(6, side, side, side, 1.0f, 0.0f);
		SetVert(7, side, -side, side, 1.0f, 1.0f);

		// Back Face
		SetVert(8, side, -side, side, 0.0f, 1.0f);
		SetVert(9, side, side, side, 0.0f, 0.0f);
		SetVert(10, -side, side, side, 1.0f, 0.0f);
		SetVert(11, -side, -side, side, 1.0f, 1.0f);

		// Left Face
		SetVert(12, -side, -side, side, 0.0f, 1.0f);
		SetVert(13, -side, side, side, 0.0f, 0.0f);
		SetVert(14, -side, side, -side, 1.0f, 0.0f);
		SetVert(15, -side, -side, -side, 1.0f, 1.0f);

		// Top Face
		SetVert(16, -side, side, -side, 0.0f, 1.0f);
		SetVert(17, -side, side, side, 0.0f, 0.0f);
		SetVert(18, side, side, side, 1.0f, 0.0f);
		SetVert(19, side, side, -side, 1.0f, 1.0f);

		// Bottom Face
		SetVert(20, -side, -side, side, 0.0f, 1.0f);
		SetVert(21, -side, -side, -side, 0.0f, 0.0f);
		SetVert(22, side, -side, -side, 1.0f, 0.0f);
		SetVert(23, side, -side, side, 1.0f, 1.0f);

		return{
			std::move(vertices),{
				0,1,2, 0,2,3,		// Front
				4,5,6, 4,6,7,		// Right
				8,9,10, 8,10,11,	// Back
				12,13,14, 12,14,15, // Left
				16,17,18, 16,18,19, // Top
				20,21,22, 20,22,23  // Bottom
			}
		};
	}
};