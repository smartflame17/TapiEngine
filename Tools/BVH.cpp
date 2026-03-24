#include "BVH.h"
#include <algorithm>
#include <array>
#include <limits>

namespace
{
	float SurfaceArea(const DirectX::BoundingBox& box) noexcept
	{
		const auto& e = box.Extents;
		return 2.0f * ((e.x * 2.0f) * (e.y * 2.0f) + (e.y * 2.0f) * (e.z * 2.0f) + (e.z * 2.0f) * (e.x * 2.0f));
	}

	DirectX::BoundingBox MergeBounds(const DirectX::BoundingBox& a, const DirectX::BoundingBox& b) noexcept
	{
		DirectX::BoundingBox merged;
		DirectX::BoundingBox::CreateMerged(merged, a, b);
		return merged;
	}

	DirectX::BoundingBox ComputeMergedBounds(const std::vector<SpatialProxy*>& objects) noexcept
	{
		if (objects.empty())
		{
			return {};
		}

		auto merged = objects.front()->bounds;
		for (std::size_t i = 1; i < objects.size(); ++i)
		{
			merged = MergeBounds(merged, objects[i]->bounds);
		}
		return merged;
	}

	float GetAxisValue(const DirectX::XMFLOAT3& value, int axis) noexcept
	{
		switch (axis)
		{
		case 0: return value.x;
		case 1: return value.y;
		default: return value.z;
		}
	}
}

BVH::BVH(std::size_t maxLeafObjects)
	:
	maxLeafObjects(maxLeafObjects > 0u ? maxLeafObjects : 1u)
{}

SpatialProxy* BVH::Insert(const SpatialProxy& proxy)
{
	auto storedProxy = std::make_unique<SpatialProxy>(proxy);
	auto* proxyPtr = storedProxy.get();
	proxies.push_back(std::move(storedProxy));
	needsBuild = true;
	return proxyPtr;
}

void BVH::Remove(SpatialProxy* proxy)
{
	if (proxy == nullptr)
	{
		return;
	}

	const auto it = std::find_if(proxies.begin(), proxies.end(),
		[proxy](const auto& current) { return current.get() == proxy; });
	if (it != proxies.end())
	{
		proxies.erase(it);
		needsBuild = true;
	}
}

void BVH::Update(SpatialProxy* proxy)
{
	if (proxy != nullptr)
	{
		needsBuild = (root == nullptr);
	}
}

void BVH::Build()
{
	nodePool.Reset();
	root = nullptr;

	if (proxies.empty())
	{
		needsBuild = false;
		return;
	}

	std::vector<SpatialProxy*> objectPtrs;
	objectPtrs.reserve(proxies.size());
	for (auto& proxy : proxies)
	{
		objectPtrs.push_back(proxy.get());
	}

	root = BuildRecursive(objectPtrs, nullptr);
	needsBuild = false;
}

void BVH::Refit()
{
	if (needsBuild)
	{
		Build();
		return;
	}

	if (root != nullptr)
	{
		RefitRecursive(root);
	}
}

void BVH::Clear() noexcept
{
	root = nullptr;
	proxies.clear();
	nodePool.Reset();
	needsBuild = false;
}

void BVH::QueryAABB(const DirectX::BoundingBox& region, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const
{
	QueryAABBRecursive(root, region, results, layerMask, typeFilter);
}

void BVH::Raycast(const DirectX::SimpleMath::Ray& ray, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const
{
	RaycastRecursive(root, ray, results, layerMask, typeFilter);
}

void BVH::FrustumQuery(const DirectX::BoundingFrustum& frustum, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const
{
	FrustumQueryRecursive(root, frustum, results, layerMask, typeFilter);
}

BVHNode* BVH::NodePool::Acquire()
{
	if (nextFree >= nodes.size())
	{
		nodes.push_back(std::make_unique<BVHNode>());
	}

	auto* node = nodes[nextFree++].get();
	node->bounds = {};
	node->left = nullptr;
	node->right = nullptr;
	node->parent = nullptr;
	node->objects.clear();
	node->isLeaf = true;
	return node;
}

void BVH::NodePool::Reset() noexcept
{
	nextFree = 0u;
}

BVHNode* BVH::BuildRecursive(std::vector<SpatialProxy*>& objects, BVHNode* parent)
{
	if (objects.empty())
	{
		return nullptr;
	}

	auto* node = nodePool.Acquire();
	node->parent = parent;
	node->bounds = ComputeMergedBounds(objects);

	if (objects.size() <= maxLeafObjects)
	{
		node->objects = objects;
		return node;
	}

	float bestCost = (std::numeric_limits<float>::max)();
	int bestAxis = -1;
	std::size_t bestSplitIndex = 0u;
	std::vector<SpatialProxy*> bestSorted;

	for (int axis = 0; axis < 3; ++axis)
	{
		std::vector<SpatialProxy*> sorted = objects;
		std::sort(sorted.begin(), sorted.end(),
			[axis](const SpatialProxy* lhs, const SpatialProxy* rhs)
			{
				return GetAxisValue(lhs->bounds.Center, axis) < GetAxisValue(rhs->bounds.Center, axis);
			});

		std::vector<DirectX::BoundingBox> prefix(sorted.size());
		std::vector<DirectX::BoundingBox> suffix(sorted.size());

		prefix[0] = sorted[0]->bounds;
		for (std::size_t i = 1; i < sorted.size(); ++i)
		{
			prefix[i] = MergeBounds(prefix[i - 1], sorted[i]->bounds);
		}

		suffix[sorted.size() - 1] = sorted.back()->bounds;
		for (std::size_t i = sorted.size() - 1; i > 0; --i)
		{
			suffix[i - 1] = MergeBounds(suffix[i], sorted[i - 1]->bounds);
		}

		for (std::size_t splitIndex = 1; splitIndex < sorted.size(); ++splitIndex)
		{
			const float leftCost = SurfaceArea(prefix[splitIndex - 1]) * static_cast<float>(splitIndex);
			const float rightCost = SurfaceArea(suffix[splitIndex]) * static_cast<float>(sorted.size() - splitIndex);
			const float totalCost = leftCost + rightCost;
			if (totalCost < bestCost)
			{
				bestCost = totalCost;
				bestAxis = axis;
				bestSplitIndex = splitIndex;
				bestSorted = sorted;
			}
		}
	}

	if (bestAxis < 0 || bestSplitIndex == 0u || bestSplitIndex >= objects.size())
	{
		node->objects = objects;
		return node;
	}

	std::vector<SpatialProxy*> leftObjects(bestSorted.begin(), bestSorted.begin() + static_cast<std::ptrdiff_t>(bestSplitIndex));
	std::vector<SpatialProxy*> rightObjects(bestSorted.begin() + static_cast<std::ptrdiff_t>(bestSplitIndex), bestSorted.end());

	if (leftObjects.empty() || rightObjects.empty())
	{
		node->objects = objects;
		return node;
	}

	node->isLeaf = false;
	node->left = BuildRecursive(leftObjects, node);
	node->right = BuildRecursive(rightObjects, node);
	return node;
}

void BVH::QueryAABBRecursive(BVHNode* node, const DirectX::BoundingBox& region, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const
{
	if (node == nullptr || !node->bounds.Intersects(region))
	{
		return;
	}

	if (node->isLeaf)
	{
		for (auto* proxy : node->objects)
		{
			if (proxy != nullptr && proxy->bounds.Intersects(region) && PassesFilter(*proxy, layerMask, typeFilter))
			{
				results.push_back(proxy);
			}
		}
		return;
	}

	QueryAABBRecursive(node->left, region, results, layerMask, typeFilter);
	QueryAABBRecursive(node->right, region, results, layerMask, typeFilter);
}

void BVH::RaycastRecursive(BVHNode* node, const DirectX::SimpleMath::Ray& ray, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const
{
	if (node == nullptr)
	{
		return;
	}

	float nodeDistance = 0.0f;
	if (!ray.Intersects(node->bounds, nodeDistance))
	{
		return;
	}

	if (node->isLeaf)
	{
		for (auto* proxy : node->objects)
		{
			float hitDistance = 0.0f;
			if (proxy != nullptr && PassesFilter(*proxy, layerMask, typeFilter) && ray.Intersects(proxy->bounds, hitDistance))
			{
				results.push_back(proxy);
			}
		}
		return;
	}

	RaycastRecursive(node->left, ray, results, layerMask, typeFilter);
	RaycastRecursive(node->right, ray, results, layerMask, typeFilter);
}

void BVH::FrustumQueryRecursive(BVHNode* node, const DirectX::BoundingFrustum& frustum, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const
{
	if (node == nullptr || frustum.Contains(node->bounds) == DirectX::DISJOINT)
	{
		return;
	}

	if (node->isLeaf)
	{
		for (auto* proxy : node->objects)
		{
			if (proxy != nullptr && frustum.Contains(proxy->bounds) != DirectX::DISJOINT && PassesFilter(*proxy, layerMask, typeFilter))
			{
				results.push_back(proxy);
			}
		}
		return;
	}

	FrustumQueryRecursive(node->left, frustum, results, layerMask, typeFilter);
	FrustumQueryRecursive(node->right, frustum, results, layerMask, typeFilter);
}

void BVH::RefitRecursive(BVHNode* node)
{
	if (node == nullptr)
	{
		return;
	}

	if (node->isLeaf)
	{
		node->bounds = ComputeMergedBounds(node->objects);
		return;
	}

	RefitRecursive(node->left);
	RefitRecursive(node->right);

	if (node->left != nullptr && node->right != nullptr)
	{
		node->bounds = MergeBounds(node->left->bounds, node->right->bounds);
	}
	else if (node->left != nullptr)
	{
		node->bounds = node->left->bounds;
	}
	else if (node->right != nullptr)
	{
		node->bounds = node->right->bounds;
	}
}

bool BVH::PassesFilter(const SpatialProxy& proxy, std::uint32_t layerMask, ProxyType typeFilter) noexcept
{
	if ((proxy.layerMask & layerMask) == 0u)
	{
		return false;
	}

	return typeFilter == ProxyType::Unknown || proxy.type == typeFilter;
}
