#pragma once
#include "ConstantBuffers.h"
#include "../Drawable/Drawable.h"

class ShadowTransformCbuf : public IBindable
{
private:
	struct Transforms
	{
		DirectX::XMMATRIX modelLightViewProjection;
	};

public:
	ShadowTransformCbuf(Graphics& gfx, const Drawable& parent);
	void Bind(Graphics& gfx) noexcept override;
	static void SetLightViewProjection(DirectX::FXMMATRIX lightViewProjection) noexcept;

private:
	static std::unique_ptr<VertexConstantBuffer<Transforms>> pVcbuf;
	static DirectX::XMFLOAT4X4 lightViewProjection;
	const Drawable& parent;
};
