#pragma once
#include "DrawableBase.h"
#include "../IBindable/IBindableBase.h"
#include "../IBindable/IndexBuffer.h"
#include "../IBindable/Topology.h"
#include "../IBindable/TransformCBuf.h"
#include <vector>
#include <memory>

class Mesh : public DrawableBase<Mesh>
{
public:
	Mesh(Graphics& gfx, std::vector<std::unique_ptr<IBindable>> bindPtrs);
	void Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void SetTransform(const Transform& transform) noexcept = delete;
	void SetExternalTransformMatrix(DirectX::FXMMATRIX matrix) noexcept = delete;

private:
	mutable DirectX::XMFLOAT4X4 transform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
};
