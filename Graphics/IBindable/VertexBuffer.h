#pragma once
#include "IBindable.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include "../Vertex.h"

namespace Bind
{ 
class VertexBuffer : public IBindable
{
public:
	template<class V>
	VertexBuffer(Graphics& gfx, const std::vector<V>& vertices)		// templated constructor to handle any vertex structure
		:
		stride(sizeof(V))
	{
		HRESULT hr;

		D3D11_BUFFER_DESC bd = {};
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.CPUAccessFlags = 0u;
		bd.MiscFlags = 0u;
		bd.ByteWidth = UINT(sizeof(V) * vertices.size());
		bd.StructureByteStride = sizeof(V);
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = vertices.data();
		GFX_THROW_FAILED(GetDevice(gfx)->CreateBuffer(&bd, &sd, &pVertexBuffer));
	}

	// new constructor that handles dynamic vertex system
	VertexBuffer(Graphics& gfx, const Dvtx::VertexBuffer& vbuf)
		:
		stride((UINT)vbuf.GetLayout().Size())
	{
		HRESULT hr;

		D3D11_BUFFER_DESC bd = {};
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.CPUAccessFlags = 0u;
		bd.MiscFlags = 0u;
		bd.ByteWidth = UINT(vbuf.SizeBytes());	// vbuf.Size() returns number of vertices, vbuf.GetLayout().Size() returns size of each vertex
		bd.StructureByteStride = stride;
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = vbuf.GetData();
		GFX_THROW_FAILED(GetDevice(gfx)->CreateBuffer(&bd, &sd, &pVertexBuffer));
	}
	const Dvtx::VertexLayout& GetLayout() const noexcept;
	void Bind(Graphics& gfx) noexcept override;
protected:
	UINT stride;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pVertexBuffer;
	Dvtx::VertexLayout layout;
};
}
