#pragma once

#include <vector>
#include <array>
#include "IndexedTriangleList.h"
#include "../Vertex.h"
#include "../../Tools/TapiMath.h"
#include <optional>

namespace Geometry
{
class Plane
{
public:
	// Create planes with many divisions for tesselated texture or deformation on vertices
	static IndexedTriangleList MakeTesselated(int divisions_x, int divisions_y, std::optional<Dvtx::VertexLayout> layout = {})
	{
		namespace dx = DirectX;
		using Type = Dvtx::VertexLayout::ElementType;
		assert(divisions_x >= 1);
		assert(divisions_y >= 1);

		if (!layout)
		{
			layout = Dvtx::VertexLayout{};
			layout->Append(Type::Position3D)
				.Append(Type::Normal);
		}

		constexpr float width = 2.0f;
		constexpr float height = 2.0f;
		const int nVertices_x = divisions_x + 1;
		const int nVertices_y = divisions_y + 1;
		Dvtx::VertexBuffer vb{ std::move(*layout) };
		const auto& vertexLayout = vb.GetLayout();
		const bool hasNormal = vertexLayout.Has(Type::Normal);
		const bool hasTexcoord = vertexLayout.Has(Type::Texture2D);

		// Divide vertex coordinates by division size and save position to vector<V> vertices
		{
			const float side_x = width / 2.0f;
			const float divisionSize_x = width / float(divisions_x);
			const float divisionSize_y = height / float(divisions_y);

			for (int y = 0, i = 0; y < nVertices_y; y++)
			{
				const float y_pos = float(y) * divisionSize_y;
				for (int x = 0; x < nVertices_x; x++, i++)
				{
					const float x_pos = float(x) * divisionSize_x - side_x;
					vb.Resize(vb.Size() + 1u);
					auto vertex = vb.Back();
					vertex.Attr<Type::Position3D>() = { x_pos,y_pos,0.0f };
					if (hasNormal)
					{
						vertex.Attr<Type::Normal>() = { 0.0f,0.0f,-1.0f };
					}
					if (hasTexcoord)
					{
						vertex.Attr<Type::Texture2D>() = {
							float(x) / float(divisions_x),
							1.0f - float(y) / float(divisions_y)
						};
					}
				}
			}
		}

		std::vector<unsigned short> indices;

		// For each subdivided square, divide it into 2 triangles and get index for each triangle's vertices
		// triangle 1: (x, y), (x, y+1), (x+1, y)
		// triangle 2: (x+1, y), (x, y+1), (x+1, y+1)
		indices.reserve(sq(divisions_x * divisions_y) * 6);
		{
			// Convert local xy coords to index
			const auto vxy2i = [nVertices_x](size_t x, size_t y)
				{
					return (unsigned short)(y * nVertices_x + x);
				};
			for (size_t y = 0; y < divisions_y; y++)
			{
				for (size_t x = 0; x < divisions_x; x++)
				{
					const std::array<unsigned short, 4> indexArray =
					{ vxy2i(x,y),vxy2i(x + 1,y),vxy2i(x,y + 1),vxy2i(x + 1,y + 1) };
					indices.push_back(indexArray[0]);
					indices.push_back(indexArray[2]);
					indices.push_back(indexArray[1]);
					indices.push_back(indexArray[1]);
					indices.push_back(indexArray[2]);
					indices.push_back(indexArray[3]);
				}
			}
		}

		return{ std::move(vb),std::move(indices) };
	}

	// default plane is 2x2 subplanes

	static IndexedTriangleList Make()
	{
		return MakeTesselated(1, 1);
	}
};
}
