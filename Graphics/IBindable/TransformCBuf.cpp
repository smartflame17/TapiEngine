#include "TransformCBuf.h"

TransformCbuf::TransformCbuf(Graphics& gfx, const Drawable& parent):
	parent(parent)
{
	if (!pVcbuf) {
		pVcbuf = std::make_unique<VertexConstantBuffer<DirectX::XMMATRIX>>(gfx);
	}
}

void TransformCbuf::Bind(Graphics& gfx) noexcept
{
	pVcbuf->Update(gfx,
		DirectX::XMMatrixTranspose(
			parent.GetTransformXM() * gfx.GetCamera() * gfx.GetProjection()
		)
	);
	pVcbuf->Bind(gfx);
}

std::unique_ptr<VertexConstantBuffer<DirectX::XMMATRIX>> TransformCbuf::pVcbuf;

// We make all objects of the same DrawableBase<T> type share the space for constant buffer, as they will be using the same config, only different data per object
// A static reference to the constant buffer is shared across instances of same class