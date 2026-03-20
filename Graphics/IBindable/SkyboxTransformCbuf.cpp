#include "SkyboxTransformCbuf.h"

SkyboxTransformCbuf::SkyboxTransformCbuf(Graphics& gfx)
{
	if (!pVcbuf)
	{
		pVcbuf = std::make_unique<VertexConstantBuffer<Transform>>(gfx);
	}
}

void SkyboxTransformCbuf::Bind(Graphics& gfx) noexcept
{
	auto view = gfx.GetCamera();
	view.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const Transform transform = {
		DirectX::XMMatrixTranspose(view * gfx.GetProjection())
	};
	pVcbuf->Update(gfx, transform);
	pVcbuf->Bind(gfx);
}

std::unique_ptr<VertexConstantBuffer<SkyboxTransformCbuf::Transform>> SkyboxTransformCbuf::pVcbuf;
