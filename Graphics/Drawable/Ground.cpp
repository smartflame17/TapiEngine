#include "Ground.h"

Ground::Ground(Graphics& gfx, int divisionsX, int divisionsY, float scale)
	:
	scale(scale)
{
	namespace dx = DirectX;
	if (!IsStaticInitialized())
	{
		struct Vertex
		{
			dx::XMFLOAT3 pos;
			dx::XMFLOAT3 n;
		};

		auto model = Geometry::Plane::MakeTesselated<Vertex>(divisionsX, divisionsY);
		model.SetNormalsIndependentFlat();

		AddStaticBind(std::make_unique<VertexBuffer>(gfx, model.vertices));
		AddStaticIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

		auto pvs = std::make_unique<VertexShader>(gfx, L"PhongVS.cso");
		auto pvsbc = pvs->GetBytecode();
		AddStaticBind(std::move(pvs));

		AddStaticBind(std::make_unique<PixelShader>(gfx, L"PhongPS.cso"));

		const std::vector<D3D11_INPUT_ELEMENT_DESC> ied =
		{
			{ "Position",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "Normal",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};
		AddStaticBind(std::make_unique<InputLayout>(gfx, ied, pvsbc));

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
