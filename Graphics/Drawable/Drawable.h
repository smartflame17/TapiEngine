#pragma once
#include "../Graphics.h"
#include <DirectXMath.h>

class IBindable;

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
protected:
	void AddBind(std::unique_ptr<IBindable> bind) noexcept(!IS_DEBUG);
	void AddIndexBuffer(std::unique_ptr<class IndexBuffer> ibuf) noexcept;
private:
	virtual const std::vector<std::unique_ptr<IBindable>>& GetStaticBinds() const noexcept = 0;	// implemented in DrawableBase
private:
	const IndexBuffer* pIndexBuffer = nullptr;		// reference to index buffer (for DrawIndexed)
	std::vector<std::unique_ptr<IBindable>> binds;
};