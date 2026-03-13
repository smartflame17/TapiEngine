#include "TransformCBuf.h"

TransformCbuf::TransformCbuf(Graphics& gfx, const Drawable& parent):
	parent(parent)
{
	if (!pVcbuf) {
		pVcbuf = std::make_unique<VertexConstantBuffer<Transforms>>(gfx);
	}
	if (!pPcbuf) {
		pPcbuf = std::make_unique<PixelConstantBuffer<Camera>>(gfx, 2u);
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

	const DirectX::XMMATRIX cameraMatrix = gfx.GetCamera();
	const DirectX::XMMATRIX inverseCamera = DirectX::XMMatrixInverse(nullptr, cameraMatrix);
	Camera cam = {
		{
			DirectX::XMVectorGetX(inverseCamera.r[3]),
			DirectX::XMVectorGetY(inverseCamera.r[3]),
			DirectX::XMVectorGetZ(inverseCamera.r[3])
		},
		0.0f
	};

	pVcbuf->Update(gfx,tf);
	pVcbuf->Bind(gfx);
	pPcbuf->Update(gfx, cam);
	pPcbuf->Bind(gfx);
}

std::unique_ptr<VertexConstantBuffer<TransformCbuf::Transforms>> TransformCbuf::pVcbuf;
std::unique_ptr<PixelConstantBuffer<TransformCbuf::Camera>> TransformCbuf::pPcbuf;

// We make all objects of the same DrawableBase<T> type share the space for constant buffer, as they will be using the same config, only different data per object
// A static reference to the constant buffer is shared across instances of same class
