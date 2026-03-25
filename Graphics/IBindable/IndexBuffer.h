#pragma once
#include "IBindable.h"
#include <cstdint>
#include <vector>

class IndexBuffer : public IBindable
{
public:
	IndexBuffer(Graphics& gfx, const std::vector<unsigned short>& indices);
	IndexBuffer(Graphics& gfx, const std::vector<std::uint32_t>& indices);
	void Bind(Graphics& gfx) noexcept override;
	UINT GetCount() const noexcept;
protected:
	UINT count;
	DXGI_FORMAT format = DXGI_FORMAT_R16_UINT;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer;
};
