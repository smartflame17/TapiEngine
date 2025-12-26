#include "TransformCBuf.h"

TransformCbuf::TransformCbuf(Graphics& gfx, const Drawable& parent):
	parent(parent)
{
	if (!pVcbuf) {
		pVcbuf = std::make_unique<VertexConstantBuffer<Transforms>>(gfx);
	}
}

void TransformCbuf::Bind(Graphics& gfx) noexcept
{
	const auto model = parent.GetTransformXM();
	Transforms tf = {
		DirectX::XMMatrixTranspose(
			model * gfx.GetCamera() * gfx.GetProjection()
		),
		DirectX::XMMatrixTranspose(
			model
		)
	};	
	pVcbuf->Update(gfx,tf);
	pVcbuf->Bind(gfx);
}

std::unique_ptr<VertexConstantBuffer<TransformCbuf::Transforms>> TransformCbuf::pVcbuf;

// We make all objects of the same DrawableBase<T> type share the space for constant buffer, as they will be using the same config, only different data per object
// A static reference to the constant buffer is shared across instances of same class