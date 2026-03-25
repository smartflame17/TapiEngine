#include "IndexBuffer.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

IndexBuffer::IndexBuffer(Graphics& gfx, const std::vector<unsigned short>& indices)
	:
	count((UINT)indices.size()),
	format(DXGI_FORMAT_R16_UINT)
{
	HRESULT hr;
	D3D11_BUFFER_DESC ibd = {};
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.CPUAccessFlags = 0u;
	ibd.MiscFlags = 0u;
	ibd.ByteWidth = UINT(count * sizeof(unsigned short));
	ibd.StructureByteStride = sizeof(unsigned short);
	D3D11_SUBRESOURCE_DATA isd = {};
	isd.pSysMem = indices.data();
	GFX_THROW_FAILED(GetDevice(gfx)->CreateBuffer(&ibd, &isd, &pIndexBuffer));
}

// For larger meshes, 16-bit indices is not enough, so overload for 32-bit indices as well
IndexBuffer::IndexBuffer(Graphics& gfx, const std::vector<std::uint32_t>& indices)
	:
	count((UINT)indices.size()),
	format(DXGI_FORMAT_R32_UINT)
{
	HRESULT hr;
	D3D11_BUFFER_DESC ibd = {};
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.CPUAccessFlags = 0u;
	ibd.MiscFlags = 0u;
	ibd.ByteWidth = UINT(count * sizeof(std::uint32_t));
	ibd.StructureByteStride = sizeof(std::uint32_t);
	D3D11_SUBRESOURCE_DATA isd = {};
	isd.pSysMem = indices.data();
	GFX_THROW_FAILED(GetDevice(gfx)->CreateBuffer(&ibd, &isd, &pIndexBuffer));
}

void IndexBuffer::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->IASetIndexBuffer(pIndexBuffer.Get(), format, 0u);
}

UINT IndexBuffer::GetCount() const noexcept
{
	return count;
}
