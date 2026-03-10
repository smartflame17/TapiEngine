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


## Others
---
I have used Microsoft's DirectXTK (DirectX Toolkit) for implementing sprite / text rendering.  

All font files must be of ```.spritefont``` extension and be included in ```Graphics/Fonts``` directory.  

To convert installed ```.ttf``` to ```.spritefont```, use the provided binary by Microsoft's DirectXTK library with the following command:  
```
MakeSpriteFont "My Font" myfont.spritefont /FontSize:16
```

Good old imgui is used for the debug / editor UI, which is rendered on top of everything else.  