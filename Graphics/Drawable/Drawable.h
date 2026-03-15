#pragma once
#include "../Graphics.h"
#include <DirectXMath.h>
#include "../IBindable/IndexBuffer.h"
#include <cassert>
#include <typeinfo>

class IBindable;

struct Transform
{
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

class Drawable
{
	template<class T>
	friend class DrawableBase;
public:
	Drawable() = default;
	Drawable(const Drawable&) = delete;
	virtual DirectX::XMMATRIX GetTransformXM() const noexcept = 0;
	virtual void Draw(Graphics& gfx) const noexcept(!IS_DEBUG);
	virtual void Update(float dt) noexcept {}
	virtual void DrawInspector() noexcept {}
	virtual ~Drawable() = default;

	void SetTransform(const Transform& transform) noexcept;
	const Transform& GetTransform() const noexcept;

protected:
	void AddBind(std::unique_ptr<IBindable> bind) noexcept(!IS_DEBUG);
	void AddIndexBuffer(std::unique_ptr<IndexBuffer> ibuf) noexcept;
	DirectX::XMMATRIX GetAppliedTransformXM() const noexcept;

private:
	virtual const std::vector<std::unique_ptr<IBindable>>& GetStaticBinds() const noexcept = 0;

private:
	Transform transform;
	const IndexBuffer* pIndexBuffer = nullptr;
	std::vector<std::unique_ptr<IBindable>> binds;
};
