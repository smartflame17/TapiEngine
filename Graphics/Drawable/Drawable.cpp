#include "Drawable.h"

Drawable::Drawable() noexcept
{
	DirectX::XMStoreFloat4x4(&externalTransform, DirectX::XMMatrixIdentity());
	localBounds.Center = { 0.0f, 0.0f, 0.0f };
	localBounds.Extents = { 0.5f, 0.5f, 0.5f };
}


void Drawable::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	for (auto& b : binds)
	{
		b->Bind(gfx);
	}
	for (auto& b : GetStaticBinds())
	{
		b->Bind(gfx);
	}
	gfx.DrawIndexed(pIndexBuffer->GetCount());
}

void Drawable::SetTransform(const Transform& transform) noexcept
{
	this->transform = transform;
}

const Transform& Drawable::GetTransform() const noexcept
{
	return transform;
}

void Drawable::SetExternalTransformMatrix(DirectX::FXMMATRIX matrix) noexcept
{
	DirectX::XMStoreFloat4x4(&externalTransform, matrix);
}

void Drawable::SetLocalBounds(const DirectX::BoundingBox& bounds) noexcept
{
	localBounds = bounds;
}

const DirectX::BoundingBox& Drawable::GetLocalBounds() const noexcept
{
	return localBounds;
}

DirectX::BoundingBox Drawable::GetWorldBounds(DirectX::FXMMATRIX externalMatrix) const noexcept
{
	DirectX::BoundingBox worldBounds;
	localBounds.Transform(worldBounds,
		DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z) *
		DirectX::XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z) *
		DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z) *
		externalMatrix);
	return worldBounds;
}

DirectX::XMMATRIX Drawable::GetAppliedTransformXM() const noexcept
{
	return
		DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z) *
		DirectX::XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z) *
		DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z) *
		DirectX::XMLoadFloat4x4(&externalTransform);
}

void Drawable::AddBind(std::unique_ptr<IBindable> bind) noexcept(!IS_DEBUG)
{
	assert("*Must* use AddIndexBuffer to bind index buffer" && typeid(*bind) != typeid(IndexBuffer));
	binds.push_back(std::move(bind));
}

void Drawable::AddIndexBuffer(std::unique_ptr<IndexBuffer> ibuf) noexcept
{
	assert("Attempting to add index buffer a second time" && pIndexBuffer == nullptr);
	pIndexBuffer = ibuf.get();
	binds.push_back(std::move(ibuf));
}
