#pragma once
#include "ConstantBuffers.h"
#include "../Drawable/Drawable.h"
#include <DirectXMath.h>

// We inherit directly from IBindable instead of Vertex Constant Buffer
// By containing a reference to the parent that houses the collection it is in, and a vertex constant buffer,
// We include this to the collection of IBindables in Drawable.
// When Bind() is called, we calculate the final transformation constant buffer with the projection matrix, and bind the vertex constant buffer as well
//
// This way, the collection in Drawable doesn't have to keep track of which element in the collection is TransformCBuf 
// So it is using composition method on VertexConstantBuffer, and inheritance to IBIndable for smoother handling
//
// (Vertex Constant Buffer is never directly added to collection in Drawable, but through TransformCbuf)

class TransformCbuf : public IBindable
{
private:
	struct Transforms
	{
		DirectX::XMMATRIX modelViewProjection;
		DirectX::XMMATRIX model;
	};
public:
	TransformCbuf(Graphics& gfx, const Drawable& parent);
	void Bind(Graphics& gfx) noexcept override;
private:
	static std::unique_ptr<VertexConstantBuffer<Transforms>> pVcbuf;
	const Drawable& parent;
};