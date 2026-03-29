# Graphics Component
---
TapiEngine utilizes Direct3D 11 to use hardware accceleration.  

<br>

The Graphics object per window encapsulates all direct3d functionality, and provides an extendable interface for individual 'objects' in the game to use (Component Architecture)

---
The overall architecture should look like the following diagram:
```
_____________               _____________________
| 3D Object |          ---  |     Drawable      |
-------------          |    --------------------|      |----------------------------------------------------------- Transformation Constant Buffer
      ^                |    |     Update()      |  <---|    _____________                                                           |
      |                |    | List of IBindable |  <------- | IBindable |  <------- InputLayout                                     |
      |                |    ---------------------           -------------      |                                                    |
___________________    |                                    |   Bind()  |      |--- VertexBuffer                                    |
| DrawableBase<T> | <--|                                    -------------      |                                                    |
-------------------                                                            |--- VertexShader                                    |
                                                                               |                                                    |
                                                                               ...      ...                                         |
                                                                               |                                                    |
                                                                               |--- ConstantBuffer <---- Vertex Constant Buffer -----
                                                                                                     |
                                                                                                     |-- Pixel Constant Buffer 
```
Each 3D object would bind any needed feature within the graphics pipeline by calling Bind() on all IBindable inside Drawable's collection.  

For the first instance of some Drawable, it will be staticly initialized (via ```AddStaticBind()```). Afterwards, all instances of the same Drawable will share the same static collection of IBindable.  
This will be done at initialization or whenever changes are made (in the update loop) to reduce redundant operations.  
As for the constant buffer, since the transformation matrices need to be independent per object, we keep a seperate reference to perform transformation.

A Child class Drawable Base from Drawable will be templated to account for different structural data needed per 3D object.
All instances of the same templated DrawableBase (ex. DrawableBase\<Cube> or DrawableBase\<Sphere>) shares a static buffer to operate on.

### IBindable  
```
virtual void Bind()
```
Performs any Direct3D pipeline-side function of the corresponding class. Constant Buffers are handled slightly differently to hold per-instance data.

### Drawable
```
virtual void Update()
```
Performs any update needed per frame.


## Geometric Primitives
---
All geometric primitive shapes such as cubes, planes, prisms, cones and spheres are essentially a wrapper for IndexedTriangleList.  

```Make()``` function transforms these primitives into IndexedTriangleList along with any divisions if any. This allows for tesselated textures or vertex manipulations.

## Dynamic Vertex (Dvtx) System
---
The Dynamic Vertex System is a system under construction, that will allow for semi-automatic vertex data management.  

The standard way of handling vertex data is to specify the data layout and creating a vertex buffer for each 3D object, which is done in compile time.  

However, this can be inefficient if there are many objects with the same data layout, and can lead to redundant code.  

Also, in cases where some data for a 3D object has different attributes (ex. some have texture coordinates, some don't), it can lead to a lot of redundant code and potential bugs.  

The new system will require the vertex data layout defined once, then automatically handle vertex buffer for each unique data layout.  
It can also handle Input Element Descriptions and Input Layouts, which can be tedious to manage for each 3D object.

The new VertexBuffer class in namespace Dvtx (Dynamic Vertex) will handle this, and will be used for all 3D objects in the future.  

---
The overall architecture should look like the following diagram:

```

________________________________________
|           Dvtx::VertexBuffer         |          
----------------------------------------
|                                      |
|     Vertex Data Layout (struct&)     |  <----------- Contains the data layout of the vertex data, which is used to create the input layout and vertex buffer.
|          ______________              |
|         |     pos      |             |                     ____________________________
|         |    color     |------------ +----------------     |        Dvtx::Vertex      |
|         |  etc.(tex)   |             |      |        |     ----------------------------
|         ----------------             |      |        |     |       pData (char*)      | <--- Dumb pointer to actual data in VertexBuffer.
|                                      |      |        |---> |  VertexLayout (struct&)  |                                            ^
|  Raw Vertex Data (std::vector<char>) |  <---+----          ----------------------------                                            |
|          ______________              |      |   |                                                                                  |
|         |              |             |      |   |-- Contains the raw vertex data, which is used to create the vertex buffer.       |
|         |     ...      |             |      |                                                                                      |
|         |   ________   |             |      | Used to cut out correct size and position of data                                    |
|         |  |        | <--------------+---------------------------------------------------------------------------------------------|
|         |  |        |  |             |
|         |  ----------  |             |
|         ----------------             |
|                                      |
-----------------------------------------

```

Instead of ```vbuf[n].pos = ...```, we can use ```vbuf[n].Attr<Pos3D>() = ...``` to handle the vertex data, 
which will automatically handle the correct position in the vertex buffer based on the input layout defined in the VertexBuffer class.  

Think of it like a cookie cutter, where the data layout is the shape of the cookie cutter, and the raw vertex data is the dough.

We introduce 3 main components to handle this: ```Dvtx::VertexBuffer```, ```Dvtx::Vertex```, and ```Dvtx::VertexLayout```. 
as well as a helper class ```VertexLayout::Element``` that maps potential vertex attributes for a vertex layout.  

### Dvtx::Vertex

A proxy class that contains a dumb pointer to the actual vertex data in the vertex buffer, and a reference to the vertex layout.  
Basically, we don't have to define a new struct for each vertex layout!  

To allow setting attributes by index, we use forwarding and variadic template to handle the attribute setting and getting.  
Some recursion voodoo is used to handle the variadic template, think of functional programming and pattern matching.  


### Dvtx::VertexLayout

A class that contains the data layout of the vertex data, which is used to create the input layout and vertex buffer.  

For now, we have ```Position2D```, ```Position3D```, ```Normal```, ```Texture2D```, ```Float3Color```, ```Float4Color```, ```BGRAColor``` 
as potential vertex attributes, but more can be added in the future.  

```
std::vector<D3D11_INPUT_ELEMENT_DESC> GetD3DLayout()
```
Returns the corresponding D3D11_INPUT_ELEMENT_DESC for the vertex layout, which is used to create the input layout.   
This is based on the potential vertex attributes defined in the VertexLayout::Element class.

### Dvtx::VertexBuffer

A wrapper for the vertex buffer, which contains the raw vertex data and the vertex layout.

```
size_t Size()
```
Returns the number of vertices in buffer.

```
size_t SizeBytes()
```
Returns the actual size of the vertex data in bytes, not to be confused with ```Size()``` which returns the number of vertices!


## Model
---
The Model class is a wrapper for a 3D model, which contains the vertex buffer, index buffer, and any other data needed to render the model.  
The engine uses assimp to load 3D models, which supports a wide range of file formats.  
It has a hierarchical structure regarding Mesh and Node classes to account for the potential hierarchy in the 3D model, which can be used for animations and other purposes.

The overall structure of the Model class should look like the following diagram:
```
Model : DrawableBase<Model>
├── unique_ptr<Node> pRoot -> Node tree root, which contains the hierarchy of the model.
├── vector<unique_ptr<Mesh>> meshPtrs -> The Model class holds all meshes in the model, which can be referenced by the nodes.
├── XMFloat4x4 transform -> The transform of the model (in Drawable class), which is applied to all nodes and meshes.
```

A Node represents a node in the hierarchy of the 3D model, which can contain multiple child nodes and references to Mesh.
```
Node
├── vector<unique_ptr<Node>> childPtrs
├── vector<Mesh*> meshPtrs
├── XMFloat4x4 transform -> Local transform of the node, which is applied to the child nodes.
```

When rendering a Model, the engine will traverse the hierarchy of the model and apply the transforms accordingly.  
It also applies the transform of its owning GameObject beforehand. (check out README in Scene directory).

## Others
---
I have used Microsoft's DirectXTK (DirectX Toolkit) for implementing sprite / text rendering.  

All font files must be of ```.spritefont``` extension and be included in ```Graphics/Fonts``` directory.  

To convert installed ```.ttf``` to ```.spritefont```, use the provided binary by Microsoft's DirectXTK library with the following command:  
```
MakeSpriteFont "My Font" myfont.spritefont /FontSize:16
```

Good old imgui is used for the debug / editor UI, which is rendered on top of everything else.  

### Skybox
The skybox is a cubemap rendered with a special shader that ignores depth.
For textures 0 to 5, they correspond to the following faces of the cubemap respectively:
- Positive X (right) : 0
- Negative X (left) : 1
- Positive Y (top) : 2
- Negative Y (bottom) : 3
- Positive Z (front) : 4
- Negative Z (back) : 5

Name the textures accordingly when loading the skybox, or else the textures will be mapped incorrectly.

## Render Queue And Multi-Pass Lighting
---
The renderer now has an explicit submission phase and an explicit execution phase.  
This replaces the older immediate-mode flow where scene traversal directly called `Draw()` on each object as soon as it was encountered.

The main goal of this structure is to make multi-pass rendering possible without rewriting each drawable every time a new pass is added.

### High-Level Flow

At runtime, the frame now proceeds in this order:

1. `App::RenderFrame()` begins the D3D frame as before.
2. `App` calls `Renderer::Render(scene, activeCam)`.
3. `Renderer` builds a `RenderView`.
4. `Renderer` asks `Scene` to collect lights for the frame.
5. `Renderer` creates a `RenderQueueBuilder` and asks `Scene` to submit visible content.
6. `Scene` traverses visible drawables and light gizmos, but instead of drawing them immediately, it submits work into the queue.
7. `Renderer` sorts the queue and executes passes in a fixed order.
8. Each pass binds its own blend / depth-stencil / rasterizer state, then replays queued work.

The important distinction is:
- Submission decides what should be rendered.
- Execution decides how and when it is rendered.

### Core Types

#### `RenderView`
`RenderView` is the frame-level snapshot used during submission.

It contains:
- active camera pointer
- view matrix
- projection matrix
- derived camera world position
- viewport information
- the list of gathered `RenderLight` values for the frame

This means scene traversal does not need to query global renderer state directly.  
Everything needed for queue submission is handed in explicitly.

#### `RenderPassId`
`RenderPassId` defines the pass order and gives each submitted item a destination pass.

The currently defined passes are:
- `Skybox`
- `OpaqueBase`
- `OpaqueLightAccum`
- `EditorGizmos`
- `Ui`
- `StencilMask`
- `Outline`

Only the first few are active right now, but the enum already leaves room for future stencil-based outline work.

#### `RenderItem`
`RenderItem` is the queued unit of work.

It currently supports two styles of submission:
- drawable replay
  - stores a `Drawable*`, transform, pass id, and sort key
- callback submission
  - stores a lambda for special work such as skybox or light gizmos

This keeps the first version simple while already supporting both standard geometry and special-case pass logic.

#### `PassState`
`PassState` stores the pipeline state used when a pass executes.

Right now it tracks:
- blend state
- depth-stencil state
- rasterizer state
- future-facing clear flags

The point of `PassState` is that pass-local pipeline configuration is centralized in the renderer instead of spread across drawables.

#### `RenderQueue`
`RenderQueue` owns:
- one list of opaque items
- one list of items per explicit pass
- one `PassState` per pass

The opaque list is important because the same geometry can be replayed in multiple passes without being resubmitted multiple times.

#### `RenderQueueBuilder`
`RenderQueueBuilder` is the write-only submission interface used by scene-side code.

It currently exposes:
- `SubmitOpaque(...)`
- `SubmitCallback(...)`

Scene code can add work to the queue, but it does not execute any D3D draw calls itself.

### Step-By-Step Frame Execution

#### 1. Build the frame view
`Renderer::BuildView()` derives the data needed for the frame:
- camera matrices
- camera position
- viewport dimensions

This is wrapped into `RenderView`.

#### 2. Gather lights
`Scene::CollectRenderLights()` walks the game object hierarchy and converts light components into plain `RenderLight` values.

Currently:
- `PointLight` produces a point-light `RenderLight`
- `DirectionalLight` produces a directional-light `RenderLight`

This is the point where the renderer becomes the source of truth for lighting, rather than `App.cpp` binding a point-light constant buffer manually.

#### 3. Submit scene content
`Scene::Submit(...)` performs visibility gathering and scene traversal.

The scene:
- submits the skybox as a callback in the `Skybox` pass
- queries visible drawables from the BVH
- submits each visible `DrawableComponent` through `SubmitOpaque(...)`
- submits light gizmos as callbacks in the `EditorGizmos` pass

No actual rendering happens yet.

This is the key seam that makes later multithreaded submission realistic:
- scene traversal writes queue data
- renderer execution consumes queue data later

#### 4. Configure pass states
`Renderer::ConfigurePassStates()` assigns the pipeline state used by each pass.

Currently:
- `Skybox`
  - opaque blend, custom skybox internals
- `OpaqueBase`
  - normal opaque blending
  - depth writes enabled
- `OpaqueLightAccum`
  - additive blending
  - depth test `EQUAL`
  - depth writes disabled
- `EditorGizmos`
  - standard opaque-like state

This is where new passes such as stencil masking or outlines will plug in later.

#### 5. Sort the queue
`RenderQueue::Sort()` sorts opaque items by sort key.

The current opaque sort key is distance-based and intended for front-to-back ordering.  
It is simple for now, but the structure is already there for later expansion into material / shader / state-aware sort keys.

#### 6. Execute `Skybox`
`Renderer::ExecuteCallbacks(RenderPassId::Skybox)` runs queued skybox callbacks first.

This keeps the skybox as special-case work for now while still routing it through the pass system.

#### 7. Execute `OpaqueBase`
`Renderer::ExecuteOpaqueBase()` draws all queued opaque geometry once.

During this pass:
- base pass state is bound
- the renderer chooses the first directional light as the primary directional light
- frame lighting constants are bound with ambient enabled
- the queued opaque geometry is replayed

This pass establishes:
- depth
- ambient term
- primary directional light contribution

#### 8. Execute `OpaqueLightAccum`
`Renderer::ExecuteOpaqueAccum()` replays the same opaque geometry once per additional light.

During this pass:
- additive blend state is bound
- depth comparison is `EQUAL`
- depth writes are disabled
- ambient is disabled
- one light is bound at a time
- all opaque geometry is replayed for that light

This is the current multi-pass light accumulation model.

The default behavior is:
- first directional light goes into the base pass
- all point lights go into additive accumulation
- additional directional lights also go into additive accumulation

So the scene is not resubmitted multiple times.  
Instead, one submitted opaque list is replayed under different pass-local lighting state.

#### 9. Execute `EditorGizmos`
Finally, editor-only helper visuals such as light gizmos are drawn from the `EditorGizmos` pass.

This keeps runtime scene rendering separate from editor visualization.

### Lighting Constant Buffer Split

The lighting shader interface is now split into two pixel constant buffers:

- `FrameLightCbuf`
  - ambient color
  - whether ambient should be applied this pass
  - whether an active light exists
  - current light type

- `LightPassCbuf`
  - one light's actual parameters for the current pass
  - color
  - intensity
  - direction
  - position
  - attenuation
  - enabled flag

This split is intentional:
- frame-level toggles stay separate from light payload data
- one light can be rebound repeatedly while replaying the same geometry
- the base pass and additive pass can differ only by constants and pass state

### Why This Design Matters

This queue structure is the foundation for later rendering features.

It enables:
- replaying geometry across multiple passes
- central pass ordering
- central pass-local state binding
- future offscreen target support
- future stencil mask / outline passes
- future multithreaded submission

Without a queue, every new multi-pass feature would force scene traversal and drawables to know about pass order and pass-specific behavior.  
With the queue, drawables mostly stay responsible for describing geometry and material bindings, while the renderer owns orchestration.

### Current Limitations

The current queue system is intentionally small in scope.

Not implemented yet:
- transparent-object queueing
- pass-specific render targets beyond the main back buffer
- stencil-mask and outline execution
- shadow mapping
- deferred shading
- true multithreaded submission / execution

So this is not a finished generalized render graph.  
It is a focused first queue-based renderer that supports the engine's current needs while opening a clean path for the next rendering features.
