#include "SolidSphere.h"
#include "../IBindable/IBindableBase.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include "Sphere.h"

SolidSphere::SolidSphere(Graphics& gfx, float radius)
{
	namespace dx = DirectX;
	if (!IsStaticInitialized())
	{
		struct Vertex
		{
			dx::XMFLOAT3 pos;
		};
		auto model = Sphere::Make<Vertex>();
		model.Transform(dx::XMMatrixScaling(radius, radius, radius));
		AddBind(std::make_unique<VertexBuffer>(gfx, model.vertices));
		AddIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

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

		const std::vector<D3D11_INPUT_ELEMENT_DESC> ied =
		{
			{ "Position",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};
		AddStaticBind(std::make_unique<InputLayout>(gfx, ied, pvsbc));
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}
}

DirectX::XMMATRIX SolidSphere::GetTransformXM() const noexcept
{
	namespace dx = DirectX;
	return dx::XMMatrixTranslation(pos.x, pos.y, pos.z);
}

void SolidSphere::Update(float dt) noexcept
{
	// No update logic for SolidSphere
}

void SolidSphere::SetPos(DirectX::XMFLOAT3 pos) noexcept
{
	this->pos = pos;
}