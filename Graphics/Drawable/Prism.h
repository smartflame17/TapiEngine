#pragma once
#include "IndexedTriangleList.h"
#include "../Vertex.h"
#include <DirectXMath.h>
#include "../../Tools/TapiMath.h"
#include <optional>

// A prism with subdivisions can represent all 'column'-like primitives
// Prism with sufficient division == cylinder
class Prism
{
public:
	static IndexedTriangleList MakeTesselated(int longDiv, std::optional<Dvtx::VertexLayout> layout = {})
	{
		namespace dx = DirectX;
		using Type = Dvtx::VertexLayout::ElementType;
		assert(longDiv >= 3);	// minimum division

		if (!layout)
		{
			layout = Dvtx::VertexLayout{};
			layout->Append(Type::Position3D);
		}

		const auto base = dx::XMVectorSet(1.0f, 0.0f, -1.0f, 0.0f);
		const auto offset = dx::XMVectorSet(0.0f, 0.0f, 2.0f, 0.0f);
		const float longitudeAngle = 2.0f * PI / longDiv;

		Dvtx::VertexBuffer vertices{ std::move(*layout) };

		// near center
		vertices.Resize(vertices.Size() + 1u);
		vertices.Back().Attr<Type::Position3D>() = { 0.0f,0.0f,-1.0f };
		const auto iCenterNear = (unsigned short)(vertices.Size() - 1);
		// far center
		vertices.Resize(vertices.Size() + 1u);
		vertices.Back().Attr<Type::Position3D>() = { 0.0f,0.0f,1.0f };
		const auto iCenterFar = (unsigned short)(vertices.Size() - 1);

		// base vertices
		for (int iLong = 0; iLong < longDiv; iLong++)
		{
			// near base
			{
				vertices.Resize(vertices.Size() + 1u);
				auto vertex = vertices.Back();
				auto v = dx::XMVector3Transform(
					base,
					dx::XMMatrixRotationZ(longitudeAngle * iLong)
				);
				dx::XMStoreFloat3(&vertex.Attr<Type::Position3D>(), v);
			}
			// far base
			{
				vertices.Resize(vertices.Size() + 1u);
				auto vertex = vertices.Back();
				auto v = dx::XMVector3Transform(
					base,
					dx::XMMatrixRotationZ(longitudeAngle * iLong)
				);
				v = dx::XMVectorAdd(v, offset);
				dx::XMStoreFloat3(&vertex.Attr<Type::Position3D>(), v);
			}
		}

		// side indices
		std::vector<unsigned short> indices;
		for (unsigned short iLong = 0; iLong < longDiv; iLong++)
		{
			const auto i = iLong * 2;
			const auto mod = longDiv * 2;
			indices.push_back(i + 2);
			indices.push_back((i + 2) % mod + 2);
			indices.push_back(i + 1 + 2);
			indices.push_back((i + 2) % mod + 2);
			indices.push_back((i + 3) % mod + 2);
			indices.push_back(i + 1 + 2);
		}

		// base indices
		for (unsigned short iLong = 0; iLong < longDiv; iLong++)
		{
			const auto i = iLong * 2;
			const auto mod = longDiv * 2;
			indices.push_back(i + 2);
			indices.push_back(iCenterNear);
			indices.push_back((i + 2) % mod + 2);
			indices.push_back(iCenterFar);
			indices.push_back(i + 1 + 2);
			indices.push_back((i + 3) % mod + 2);
		}

		return { std::move(vertices),std::move(indices) };
	}

	// Default prism is a cylinder-like (24-polygon)
	static IndexedTriangleList Make()
	{
		return MakeTesselated(24);
	}
};
