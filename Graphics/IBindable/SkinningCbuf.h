#pragma once
#include "ConstantBuffers.h"
#include "../Animation/Animation.h"
#include <array>
#include <stdexcept>

class SkinningCbuf : public IBindable
{
public:
	explicit SkinningCbuf(Graphics& gfx) : buffer(gfx, 1u)
	{
		data.bones.fill(Animation::Identity()); data.normals.fill(Animation::Identity());
	}
	void SetPalette(const std::vector<DirectX::XMFLOAT4X4>& palette)
	{
		Animation::PrepareSkinningConstants(palette, data);
		dirty = true;
	}
	void Bind(Graphics& gfx) noexcept override
	{
		if (dirty) { buffer.Update(gfx, data); dirty = false; }
		buffer.Bind(gfx);
	}
private:
	VertexConstantBuffer<Animation::SkinningConstants> buffer;
	Animation::SkinningConstants data;
	bool dirty = true;
};
