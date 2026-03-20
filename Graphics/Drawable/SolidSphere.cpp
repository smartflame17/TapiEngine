#include "SolidSphere.h"

SolidSphere::SolidSphere(Graphics& gfx, float radius)
{
	namespace dx = DirectX;
	if (!IsStaticInitialized())
	{
		using Type = Dvtx::VertexLayout::ElementType;
		auto model = Sphere::MakeTesselated(12, 24, Dvtx::VertexLayout{}
			.Append(Type::Position3D));
		model.Transform(dx::XMMatrixScaling(radius, radius, radius));
		AddStaticBind(std::make_unique<Bind::VertexBuffer>(gfx, model.vertices));
		AddStaticIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

		auto pvs = std::make_unique<VertexShader>(gfx, L"SolidVS.cso");
		auto pvsbc = pvs->GetBytecode();
		AddStaticBind(std::move(pvs));
		AddStaticBind(std::make_unique<PixelShader>(gfx, L"SolidPS.cso"));

		struct PSColorConstant
		{
			dx::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f }; // Default to white
			float padding; // Padding to align to 16 bytes
		} colorConst;

		AddStaticBind(std::make_unique<PixelConstantBuffer<PSColorConstant>>(gfx, colorConst));

		AddStaticBind(std::make_unique<InputLayout>(gfx, model.vertices.GetLayout().GetD3DLayout(), pvsbc));
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}
	else 
	{
		SetIndexFromStatic();
	}
	AddBind(std::make_unique<TransformCbuf>(gfx, *this));	// the transformation constant buffer is non-static (different per object)
}

DirectX::XMMATRIX SolidSphere::GetTransformXM() const noexcept
{
	namespace dx = DirectX;
	return GetAppliedTransformXM() * dx::XMMatrixTranslation(pos.x, pos.y, pos.z);
}

void SolidSphere::Update(float dt) noexcept
{
	// No update logic for SolidSphere
}

void SolidSphere::SetPos(DirectX::XMFLOAT3 pos) noexcept
{
	this->pos = pos;
}
