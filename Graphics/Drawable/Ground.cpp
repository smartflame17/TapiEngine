#include "Ground.h"

Ground::Ground(Graphics& gfx, int divisionsX, int divisionsY, float scale)
	:
	scale(scale)
{
	namespace dx = DirectX;
	if (!IsStaticInitialized())
	{
		using Type = Dvtx::VertexLayout::ElementType;
		auto model = Geometry::Plane::MakeTesselated(divisionsX, divisionsY, Dvtx::VertexLayout{}
			.Append(Type::Position3D)
			.Append(Type::Normal));
		model.SetNormalsIndependentFlat();

		AddStaticBind(std::make_unique<Bind::VertexBuffer>(gfx, model.vertices));
		AddStaticIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

		auto pvs = std::make_unique<VertexShader>(gfx, L"PhongVS.cso");
		auto pvsbc = pvs->GetBytecode();
		AddStaticBind(std::move(pvs));

		AddStaticBind(std::make_unique<PixelShader>(gfx, L"PhongPS.cso"));

		AddStaticBind(std::make_unique<InputLayout>(gfx, model.vertices.GetLayout().GetD3DLayout(), pvsbc));

		struct MaterialCbuf
		{
			dx::XMFLOAT3 color;
			float specularIntensity;
			float specularPower;
			dx::XMFLOAT3 specularColor;
		};

		const MaterialCbuf material =
		{
			{ 0.4f, 0.55f, 0.4f },
			0.2f,
			12.0f,
			{ 1.0f, 1.0f, 1.0f }
		};
		AddStaticBind(std::make_unique<PixelConstantBuffer<MaterialCbuf>>(gfx, material, 0u));

		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}
	else
	{
		SetIndexFromStatic();
	}

	AddBind(std::make_unique<TransformCbuf>(gfx, *this));
}

DirectX::XMMATRIX Ground::GetTransformXM() const noexcept
{
	return
		DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) *
		DirectX::XMMatrixScaling(scale, 1.0f, scale) *
		GetAppliedTransformXM();
}
