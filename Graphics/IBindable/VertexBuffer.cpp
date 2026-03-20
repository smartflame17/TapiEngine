#include "VertexBuffer.h"

void Bind::VertexBuffer::Bind(Graphics& gfx) noexcept
{
	const UINT offset = 0u;
	GetContext(gfx)->IASetVertexBuffers(0u, 1u, pVertexBuffer.GetAddressOf(), &stride, &offset);
}

const Dvtx::VertexLayout& Bind::VertexBuffer::GetLayout() const noexcept
{
	return layout;
}
