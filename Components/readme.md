## Component System
---

TODO: write this section

### How components are initialized and added in editor time  
0. In startup, App class registers all component creation handlers
1. User interacts with imgui accordingly and gets the type of the component they want to add
2. `AddComponent<T>`, or `AddScript` is called, which creates the component and adds it to the GameObject's component list and registers them to any systems if necessary.  
* For `DrawableComponent`, it will be registered to BVH system and collected to the scene's drawable list.  
* For `CustomBehaviour` (scripts), it will be registered to the `ScriptManager` under `Scene` and have its lifetime cycle configured as well.  
* As for other types (cameras, light, etc.), they are cached by sweeping through the component list of all game objects in the scene at the beginning of each frame. <- This could be improved in the future  
---

* `AddScript` is editor-only special case, which uses the script's name to find the corresponding creation handler, and creates the script component accordingly. 
This is because scripts are not known at compile time, so we cannot use template specialization like `AddComponent<T>` for other components. 
* 
* In scripting itself, we can use `AddComponent<T>` for scripts as well, since the script type is known at compile time when we write the script.

### Editor time Component Deletion (Under work)

Currently only hidden in imgui editor, and pending component unregistration is required (the GameObject still holds it, memory is still allocated, and scripts will still execute in game time)  

Hopefully in the future, we can encapsulate everything under `AddComponent<T>` and `RemoveComponent<T>` in `GameObject.h`