#pragma once
#include "ConstantBuffers.h"

class SkyboxTransformCbuf : public IBindable
{
private:
	struct Transform
	{
		DirectX::XMMATRIX viewProj;
	};

public:
	SkyboxTransformCbuf(Graphics& gfx);
	void Bind(Graphics& gfx) noexcept override;

private:
	static std::unique_ptr<VertexConstantBuffer<Transform>> pVcbuf;
};
