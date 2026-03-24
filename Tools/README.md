# Tools
___

This directory contains tools that are used throughout the project.

## Dungeon Generator
---
The `DungeonGenerator` class is responsible for generating random dungeons for the game.  
It uses a variety of algorithms (such as BSP, A* pathfinding) to create unique layouts on the fly.
 
The generated dungeons are stored as a collection of rooms and corridors, which can be rendered in the game world.

## Bounding Volume Hierarchy (BVH)
---
The engine uses a Bounding Volume Hierarchy (BVH), with a proxy class for multiple use cases.  
The BVH is a spatial data structure that allows for efficient querying of objects in the game world, such as for collision detection and raycasting.  

### SpatialProxy

The `SpatialProxy` class is a wrapper / proxy class that allows them to be easily inserted into the BVH.  

It contains a reference to the actual object (such as a GameObject) and its bounding volume (such as an AABB). 
This way, the BVH system is not tightly coupled to the specific types of objects in the game, and can be used for a variety of purposes (such as rendering, physics, etc.) without modification.

It also has layer masks to allow for filtering of objects based on their layer, which can be useful for things like rendering and collision detection.  

### BVHNode

The `BVHNode` class represents a node in the BVH tree. It contains references to its child nodes, as well as the bounding volume that encompasses all of its children.  

A single node can contain multiple `SpatialProxy` objects, which are the actual objects in the game world that are being managed by the BVH.


### BVH

The `BVH` class is a scene-wide data structure that manages all of the `SpatialProxy` objects in the game world via a binary tree structure.  

To improve the quality of the tree, the BVH uses a surface area heuristic (SAH) to determine how to split the nodes when building the tree.  

### Use cases

- Frustrum culling: The BVH can be used to quickly determine which objects are within the camera's view frustum, reducing redundant draw calls.

- Raycasting: The BVH can be used to quickly determine which objects are intersected by a ray, such as for mouse picking or line of sight calculations.

- **[Not implemented yet]** Collision detection: The BVH can be used to quickly determine which objects are colliding with each other, allowing for efficient physics calculations.