#include "SpatialGrid.h"

// Inserts a collider into the spatial grid
void Physics::SpatialGrid::Insert(Collider* collider)
{
    AABB bounds = collider->GetWorldAABB();

    // Determine range of cells this object overlaps
    int minX = (int)floor(bounds.min.x / cellSize);
    int maxX = (int)floor(bounds.max.x / cellSize);
    int minY = (int)floor(bounds.min.y / cellSize);
    int maxY = (int)floor(bounds.max.y / cellSize);
    int minZ = (int)floor(bounds.min.z / cellSize);
    int maxZ = (int)floor(bounds.max.z / cellSize);

    for (int x = minX; x <= maxX; ++x)
        for (int y = minY; y <= maxY; ++y)
            for (int z = minZ; z <= maxZ; ++z)
            {
                grid[{x, y, z}].push_back(collider);
            }
}

// Returns potential collision candidates (Broadphase)
std::vector < Physics::Collider* > Physics::SpatialGrid::GetCandidates(Collider* collider)
{
    // Use a vector or set to store unique candidates
    // For simplicity, just returning raw list (may contain duplicates)
    std::vector<Collider*> candidates;

    AABB bounds = collider->GetWorldAABB();
    int minX = (int)floor(bounds.min.x / cellSize);
    int maxX = (int)floor(bounds.max.x / cellSize);
    int minY = (int)floor(bounds.min.y / cellSize);
    int maxY = (int)floor(bounds.max.y / cellSize);
    int minZ = (int)floor(bounds.min.z / cellSize);
    int maxZ = (int)floor(bounds.max.z / cellSize);

    for (int x = minX; x <= maxX; ++x)
        for (int y = minY; y <= maxY; ++y)
            for (int z = minZ; z <= maxZ; ++z)
            {
                auto it = grid.find({ x, y, z });
                if (it != grid.end())
                {
                    for (auto* other : it->second)
                    {
                        if (other != collider) candidates.push_back(other);
                    }
                }
            }
    return candidates;
}