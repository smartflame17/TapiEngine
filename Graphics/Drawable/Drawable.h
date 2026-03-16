#pragma once
#include "../Graphics.h"
#include <DirectXMath.h>
#include "../IBindable/IndexBuffer.h"
#include <cassert>
#include <typeinfo>
#include "../../Scene/Transform.h"

class IBindable;

class Drawable
{
	template<class T>
	friend class DrawableBase;
public:
	Drawable() noexcept;
	Drawable(const Drawable&) = delete;
	virtual DirectX::XMMATRIX GetTransformXM() const noexcept = 0;
	virtual void Draw(Graphics& gfx) const noexcept(!IS_DEBUG);
	virtual void Update(float dt) noexcept {}
	virtual void DrawInspector() noexcept {}
	virtual ~Drawable() = default;

	void SetTransform(const Transform& transform) noexcept;
	const Transform& GetTransform() const noexcept;
	void SetExternalTransformMatrix(DirectX::FXMMATRIX matrix) noexcept;

protected:
	void AddBind(std::unique_ptr<IBindable> bind) noexcept(!IS_DEBUG);
	void AddIndexBuffer(std::unique_ptr<IndexBuffer> ibuf) noexcept;
	DirectX::XMMATRIX GetAppliedTransformXM() const noexcept;

private:
	virtual const std::vector<std::unique_ptr<IBindable>>& GetStaticBinds() const noexcept = 0;

private:
	Transform transform;
	DirectX::XMFLOAT4X4 externalTransform;
	const IndexBuffer* pIndexBuffer = nullptr;
	std::vector<std::unique_ptr<IBindable>> binds;
};
