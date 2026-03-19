#include "VertexBuffer.h"

void VertexBuffer::Bind(Graphics& gfx) noexcept
{
	const UINT offset = 0u;
	GetContext(gfx)->IASetVertexBuffers(0u, 1u, pVertexBuffer.GetAddressOf(), &stride, &offset);
}

const Dvtx::VertexLayout& VertexBuffer::GetLayout() const noexcept
{
	return layout;
}