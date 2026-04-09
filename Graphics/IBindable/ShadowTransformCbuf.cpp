#include "ShadowTransformCbuf.h"

ShadowTransformCbuf::ShadowTransformCbuf(Graphics& gfx, const Drawable& parent)
	:
	parent(parent)
{
	if (!pVcbuf)
	{
		pVcbuf = std::make_unique<VertexConstantBuffer<Transforms>>(gfx);
	}
}

void ShadowTransformCbuf::Bind(Graphics& gfx) noexcept
{
	Transforms tf = {
		DirectX::XMMatrixTranspose(
			parent.GetTransformXM() * DirectX::XMLoadFloat4x4(&lightViewProjection)
		)
	};

	pVcbuf->Update(gfx, tf);
	pVcbuf->Bind(gfx);
}

void ShadowTransformCbuf::SetLightViewProjection(DirectX::FXMMATRIX lightViewProjection) noexcept
{
	DirectX::XMStoreFloat4x4(&ShadowTransformCbuf::lightViewProjection, lightViewProjection);
}

std::unique_ptr<VertexConstantBuffer<ShadowTransformCbuf::Transforms>> ShadowTransformCbuf::pVcbuf;
DirectX::XMFLOAT4X4 ShadowTransformCbuf::lightViewProjection = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};
