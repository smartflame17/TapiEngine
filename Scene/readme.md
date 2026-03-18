# Scene Structure

The engine uses a scene graph structure to manage and organize the various elements in a game or application.  
A scene graph is a hierarchical structure that allows for efficient rendering and management of objects in a 3D space.

Each scene keeps a list of root GameObjects, which are the top-level objects in the scene. Each GameObject can have child GameObjects, creating a tree-like structure.  

## Scene

A Scene is the main container for GameObjects and a unit for rendering and updating. It manages the lifecycle of GameObjects, including their creation, update, and destruction.  

(Note: Though not implemented yet, it will be serialized and deserialized to and from a file, allowing for saving and loading scenes.)  

Currently, the following is the structure of the Scene class:  
```
Scene
├── name: string
├── rootObjects: vector<unique_ptr<GameObject>> -> List of root GameObjects in the scene
├── std::vector<DrawableComponent*> drawables -> Cached list of drawable components for efficient rendering
```

## GameObject

A GameObject is a fundamental entity in the scene. It has a name, a transform (position, rotation, scale), and can have multiple components as well as child objects attached to it.  

```
GameObject
├── name: string
├── transform: Transform (position, rotation, scale)
├── components: vector<unique_ptr<Component>>
├── children: vector<unique_ptr<GameObject>>
```

## Component System

The engine uses a component-based architecture, where each GameObject can have multiple components attached to it.  
Components are modular pieces of functionality that can be added to GameObjects to give them specific behaviors or properties.  

```DrawableComponent``` is an example of a component that can be attached to a GameObject to make it renderable. 

A typical GameObject with a ```DrawableComponent``` (say, a ```Model```) would have the following structure:  
```
GameObject
├── Transform (position, rotation, scale)
├── DrawableComponent 
       ├── unique_ptr<Model>
             
```
When rendering a ```DrawableComponent```, the transform will be propagated as follows:  

```
(Parent GameObjects' Transform) * (Current GameObject's Transform) -> Saved into externalTransform as XMFloat4x4

externalTransform * (Drawable's Transform))
```
The transform is converted into a 4x4 matrix and passed to the shader.

### Using Gizmos during scene editing

The project uses ImGuizmo, a library that provides a set of tools for manipulating objects in a 3D space.  
When manipulating the ImGuizmo, the engine will pass the transform in the following way:  
```
modelMatrix = scene.GetSelectedWorldTransformMatrix() -> 
              GameObject.GetWorldTransformMatrix() -> 
              Transform.MakeTransformMatrix() -> Returns modelMatrix as XMFloat4x4

ImGuizmo::Manipulate(modelMatrix) -> Manipulates the passed modelMatrix

Application to original transform:
modelMatrix -> scene.SetSelectedWorldTransformMatrix()->
               GameObject.GetTransform()->
               Transform.MakeTransformFromMatrix(modelMatrix) -> Returns transform from modelMatrix

```
The engine will also update the transform of all child GameObjects, so that the changes are propagated down the hierarchy.  
Once tranform manipulation is done on GameObject level, the engine will update the transform of DrawableComponent as well, so that the changes are reflected in the rendering. 
The following is the flow of how the transform is applied on rendering:  
```
Scene.Render() -> DrawableComponent.OnRender() (We cache only the drawable components in the scene to reduce redundant looping)
               -> DrawableComponent.SetExternalTransformMatrix(GetGameObject().GetWorldTransformMatrix()) (Passes transform as 4x4 matrix)
               -> Drawable.Draw() (virtual function)

```