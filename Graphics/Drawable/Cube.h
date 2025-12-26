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
	template<class V>
	static IndexedTriangleList<V> MakeIndependent()
	{
		namespace dx = DirectX;
		constexpr float side = 1.0f / 2.0f;

		std::vector<V> vertices(24);

		// Lambda to set pos and tex easily
		auto SetVert = [&vertices](int i, float x, float y, float z) {
			vertices[i].pos = dx::XMFLOAT3(x, y, z);
			};

		// Front Face
		SetVert(0, -side, -side, -side);
		SetVert(1, -side, side, -side);
		SetVert(2, side, side, -side);
		SetVert(3, side, -side, -side);

		// Right Face
		SetVert(4, side, -side, -side);
		SetVert(5, side, side, -side);
		SetVert(6, side, side, side);
		SetVert(7, side, -side, side);

		// Back Face
		SetVert(8, side, -side, side);
		SetVert(9, side, side, side);
		SetVert(10, -side, side, side);
		SetVert(11, -side, -side, side);

		// Left Face
		SetVert(12, -side, -side, side);
		SetVert(13, -side, side, side);
		SetVert(14, -side, side, -side);
		SetVert(15, -side, -side, -side);

		// Top Face
		SetVert(16, -side, side, -side);
		SetVert(17, -side, side, side);
		SetVert(18, side, side, side);
		SetVert(19, side, side, -side);

		// Bottom Face
		SetVert(20, -side, -side, side);
		SetVert(21, -side, -side, -side);
		SetVert(22, side, -side, -side);
		SetVert(23, side, -side, side);

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