#pragma once
#include "IndexedTriangleList.h"
#include "../Vertex.h"
#include <DirectXMath.h>
#include "../../Tools/TapiMath.h"
#include <optional>

// Cone is similar to prism except that one end merges into single vertex
class Cone
{
public:
	static IndexedTriangleList MakeTesselated(int longDiv, std::optional<Dvtx::VertexLayout> layout = {})
	{
		namespace dx = DirectX;
		using Type = Dvtx::VertexLayout::ElementType;
		assert(longDiv >= 3);

		if (!layout)
		{
			layout = Dvtx::VertexLayout{};
			layout->Append(Type::Position3D);
		}

		const auto base = dx::XMVectorSet(1.0f, 0.0f, -1.0f, 0.0f);
		const float longitudeAngle = 2.0f * PI / longDiv;

		Dvtx::VertexBuffer vertices{ std::move(*layout) };
		for (int iLong = 0; iLong < longDiv; iLong++)
		{
			vertices.Resize(vertices.Size() + 1u);
			auto vertex = vertices.Back();
			auto v = dx::XMVector3Transform(
				base,
				dx::XMMatrixRotationZ(longitudeAngle * iLong)
			);
			dx::XMStoreFloat3(&vertex.Attr<Type::Position3D>(), v);
		}
		// the center
		vertices.Resize(vertices.Size() + 1u);
		vertices.Back().Attr<Type::Position3D>() = { 0.0f,0.0f,-1.0f };
		const auto iCenter = (unsigned short)(vertices.Size() - 1);
		// the tip :darkness:
		vertices.Resize(vertices.Size() + 1u);
		vertices.Back().Attr<Type::Position3D>() = { 0.0f,0.0f,1.0f };
		const auto iTip = (unsigned short)(vertices.Size() - 1);


		// base indices
		std::vector<unsigned short> indices;
		for (unsigned short iLong = 0; iLong < longDiv; iLong++)
		{
			indices.push_back(iCenter);
			indices.push_back((iLong + 1) % longDiv);
			indices.push_back(iLong);
		}

		// cone indices
		for (unsigned short iLong = 0; iLong < longDiv; iLong++)
		{
			indices.push_back(iLong);
			indices.push_back((iLong + 1) % longDiv);
			indices.push_back(iTip);
		}

		return { std::move(vertices),std::move(indices) };
	}

	static IndexedTriangleList Make()
	{
		return MakeTesselated(24);
	}
};
