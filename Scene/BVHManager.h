#pragma once

#include "../Tools/BVH.h"
#include <DirectXCollision.h>
#include <unordered_map>
#include <vector>

class DrawableComponent;

class BVHManager
{
public:
	void Clear() noexcept;
	void RegisterDrawable(DrawableComponent* drawable);
	void UnregisterDrawable(DrawableComponent* drawable) noexcept;
	void Sync();
	void QueryVisibleDrawables(const DirectX::BoundingFrustum& frustum, std::vector<DrawableComponent*>& results) const;
	DrawableComponent* RaycastClosestDrawable(const DirectX::SimpleMath::Ray& ray) const noexcept;

private:
	struct DrawableProxyEntry
	{
		SpatialProxy* proxy = nullptr;
		bool isStatic = false;
	};

private:
	static constexpr std::uint32_t RenderLayerMask = 1u;

	BVH staticBVH;
	BVH dynamicBVH;
	std::unordered_map<DrawableComponent*, DrawableProxyEntry> drawableEntries;
};
