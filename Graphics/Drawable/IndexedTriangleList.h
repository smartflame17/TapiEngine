#pragma once
#include <vector>
#include <DirectXMath.h>

// A base primitive class that interprets vertex data as triangle list (Used for other primitives such as cubes, planes)
template<class T>
class IndexedTriangleList
{
public:
	IndexedTriangleList() = default;
	IndexedTriangleList(std::vector<T> verts_in, std::vector<unsigned short> indices_in)
		:
		vertices(std::move(verts_in)),
		indices(std::move(indices_in))
	{
		assert(vertices.size() > 2);
		assert(indices.size() % 3 == 0);
	}
	void Transform(DirectX::FXMMATRIX matrix)
	{
		for (auto& v : vertices)
		{
			const DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&v.pos);
			DirectX::XMStoreFloat3(
				&v.pos,
				DirectX::XMVector3Transform(pos, matrix)
			);
		}
	}
	// face-independent vertices with normals set to zero
	void SetNormalsIndependentFlat() noexcept(!IS_DEBUG)
	{
		assert(indices.size() % 3 == 0 && indices.size() > 0);
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			auto& v0 = vertices[indices[i + 0]];
			auto& v1 = vertices[indices[i + 1]];
			auto& v2 = vertices[indices[i + 2]];
			const DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&v0.pos);
			const DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.pos);
			const DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&v2.pos);
			const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(
				DirectX::XMVector3Cross(
					DirectX::XMVectorSubtract(p1, p0),
					DirectX::XMVectorSubtract(p2, p0)
				)
			);
			DirectX::XMStoreFloat3(&v0.n, normal);
			DirectX::XMStoreFloat3(&v1.n, normal);
			DirectX::XMStoreFloat3(&v2.n, normal);
		}
	}

public:
	std::vector<T> vertices;
	std::vector<unsigned short> indices;
};