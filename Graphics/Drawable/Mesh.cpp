#include "Mesh.h"

Mesh::Mesh(Graphics& gfx, std::vector<std::unique_ptr<IBindable>> bindPtrs)
{
	if (!IsStaticInitialized())
	{
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}

	for (auto& bind : bindPtrs)
	{
		if (auto pIndexBuffer = dynamic_cast<IndexBuffer*>(bind.get()))
		{
			AddIndexBuffer(std::unique_ptr<IndexBuffer>{ pIndexBuffer });
			bind.release();
		}
		else
		{
			AddBind(std::move(bind));
		}
	}

	AddBind(std::make_unique<TransformCbuf>(gfx, *this));
}

void Mesh::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG)
{
	DirectX::XMStoreFloat4x4(&transform, accumulatedTransform);
	Drawable::Draw(gfx);
}

DirectX::XMMATRIX Mesh::GetTransformXM() const noexcept
{
	return DirectX::XMLoadFloat4x4(&transform) * GetAppliedTransformXM();
}
