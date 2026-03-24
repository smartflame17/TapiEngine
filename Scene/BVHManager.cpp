#include "BVHManager.h"
#include "../Components/DrawableComponent.h"
#include "../Scene/GameObject.h"

void BVHManager::Clear() noexcept
{
	drawableEntries.clear();
	staticBVH.Clear();
	dynamicBVH.Clear();
}

void BVHManager::RegisterDrawable(DrawableComponent* drawable)
{
	if (drawable == nullptr)
	{
		return;
	}

	drawableEntries.try_emplace(drawable);
}

void BVHManager::UnregisterDrawable(DrawableComponent* drawable) noexcept
{
	const auto it = drawableEntries.find(drawable);
	if (it == drawableEntries.end())
	{
		return;
	}

	if (it->second.proxy != nullptr)
	{
		if (it->second.isStatic)
		{
			staticBVH.Remove(it->second.proxy);
		}
		else
		{
			dynamicBVH.Remove(it->second.proxy);
		}
	}

	drawableEntries.erase(it);
}

void BVHManager::Sync()
{
	for (auto& [drawable, entry] : drawableEntries)
	{
		if (drawable == nullptr)
		{
			continue;
		}

		SpatialProxy proxy;
		proxy.bounds = drawable->GetWorldBounds();
		proxy.userData = drawable;
		proxy.layerMask = RenderLayerMask;
		proxy.type = ProxyType::Renderable;
		proxy.isStatic = drawable->GetGameObject().IsStatic();

		if (entry.proxy == nullptr)
		{
			entry.isStatic = proxy.isStatic;
			entry.proxy = entry.isStatic ? staticBVH.Insert(proxy) : dynamicBVH.Insert(proxy);
			continue;
		}

		// If static/dynamic state has changed, remove and re-insert to correct BVH
		if (entry.isStatic != proxy.isStatic)
		{
			if (entry.isStatic)
			{
				staticBVH.Remove(entry.proxy);
			}
			else
			{
				dynamicBVH.Remove(entry.proxy);
			}

			entry.isStatic = proxy.isStatic;
			entry.proxy = entry.isStatic ? staticBVH.Insert(proxy) : dynamicBVH.Insert(proxy);
			continue;
		}

		entry.proxy->bounds = proxy.bounds;
		entry.proxy->layerMask = proxy.layerMask;
		entry.proxy->type = proxy.type;
		entry.proxy->isStatic = proxy.isStatic;

		if (entry.isStatic)
		{
			staticBVH.Update(entry.proxy);
		}
		else
		{
			dynamicBVH.Update(entry.proxy);
		}
	}

	staticBVH.Build();
	dynamicBVH.Refit();
}

// Frustrum culling query to find visible drawables
void BVHManager::QueryVisibleDrawables(const DirectX::BoundingFrustum& frustum, std::vector<DrawableComponent*>& results) const
{
	std::vector<SpatialProxy*> visibleProxies;
	visibleProxies.reserve(drawableEntries.size());

	staticBVH.FrustumQuery(frustum, visibleProxies, RenderLayerMask, ProxyType::Renderable);
	dynamicBVH.FrustumQuery(frustum, visibleProxies, RenderLayerMask, ProxyType::Renderable);

	results.reserve(results.size() + visibleProxies.size());
	for (auto* proxy : visibleProxies)
	{
		if (proxy != nullptr)
		{
			results.push_back(static_cast<DrawableComponent*>(proxy->userData));
		}
	}
}
