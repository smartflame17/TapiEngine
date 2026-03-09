## Graphics Component

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
This will be done at initialization or whenever changes are made (in the update loop) to reduce redundant operations.  
As for the constant buffer, since the transformation matrices will change per frame, we keep a seperate reference to perform transformation.

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
All geometric primitive shapes such as cubes, planes, prisms, cones and spheres are essentially a wrapper for IndexedTriangleList.  

```Make()``` function transforms these primitives into IndexedTriangleList along with any divisions if any. This allows for tesselated textures or vertex manipulations.

## Dynamic Vertex (Dvtx) System
The Dynamic Vertex System is a system under construction, that will allow for semi-automatic vertex data management.  

The standard way of handling vertex data is to specify the data layout and creating a vertex buffer for each 3D object, which is done in compile time.  

However, this can be inefficient if there are many objects with the same data layout, and can lead to redundant code.  

The new system will require the vertex data layout defined once, then automatically handle vertex buffer for each unique data layout.  
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
|         |     pos      |             |
|         |    color     |------------ +-------
|         |  etc.(tex)   |             |      |
|         ----------------             |      |
|									   |      |
|  Raw Vertex Data (std::vector<char>) |  <---+------- Contains the raw vertex data, which is used to create the vertex buffer.
|          ______________              |      |
|         |              |             |      |
|         |     ...      |             |      |
|         |   ________   |             |      | Used to cut out correct size and position of data
|         |  |        | <--------------+-------
|         |  |        |  |             |
|         |  ----------  |             |
|         ----------------             |
|                                      |
-----------------------------------------

```

Instead of ```vbuf[n].pos = ...```, we can use ```vbuf[n].Attr<Pos3D>() = ...``` to handle the vertex data, 
which will automatically handle the correct position in the vertex buffer based on the input layout defined in the VertexBuffer class.  

Think of it like a cookie cutter, where the data layout is the shape of the cookie cutter, and the raw vertex data is the dough.

## Others

I have used Microsoft's DirectXTK (DirectX Toolkit) for implementing sprite / text rendering.  

All font files must be of ```.spritefont``` extension and be included in ```Graphics/Fonts``` directory.  

To convert installed ```.ttf``` to ```.spritefont```, use the provided binary by Microsoft's DirectXTK library with the following command:  
```
MakeSpriteFont "My Font" myfont.spritefont /FontSize:16
```