# TapiEngine JSON Serialization Implementation Plan

## Goal

**Implementation status:** Stages 1–6 are implemented. Editor scenes use `.scene`
files containing version 1 JSON. Play saves the current scene file before
simulation; Stop/Escape reload it, using an in-memory backup if file loading
fails. Stage 7 remains follow-up work. See `Serialization/README.md` for the
implemented API, editor workflow and validation commands.

Implement versioned JSON scene serialization and deserialization for TapiEngine using `nlohmann/json`. The system should save and reconstruct the complete editable scene hierarchy while rebuilding transient rendering, physics, scripting, and editor state at load time.

The initial scope is **scene assets**, not runtime save games. Runtime-only data such as current physics poses, animation playback time, lifecycle queues, and editor selection should not be persisted yet.

## Core design

- `SceneSerializer` owns file I/O, schema validation, version migration, and recursive traversal.
- `Scene` and `GameObject` expose the data required by the serializer but do not need a shared polymorphic serialization base.
- `Component` provides virtual serialization hooks for its own payload.
- `ComponentRegistry` maps stable string type identifiers to construction functions and is shared by the editor and scene loader.
- Persistent UUIDs identify `GameObject` and `Component` instances across runs.
- Asset references are saved as project-relative paths.
- GPU objects, Box3D handles, raw pointers, caches, and lifecycle queues are reconstructed instead of serialized.

## Target architecture

```text
Scene file
  -> parse and validate JSON
  -> migrate older schema versions
  -> create GameObject hierarchy
  -> construct components through ComponentRegistry
  -> restore component data
  -> resolve UUID references
  -> rebuild runtime registrations and caches
```

## Proposed modules

```text
Serialization/
├── SceneSerializer.h/.cpp
├── ComponentRegistry.h/.cpp
├── SerializationContext.h
├── JsonMath.h
├── Guid.h/.cpp
├── SceneFormat.h
└── SerializationError.h
```

### SceneSerializer

Responsibilities:

- Serialize and deserialize a complete `Scene`.
- Recursively process root objects, child objects, and components.
- Read and write scene files.
- Validate the root format and version.
- Run schema migrations.
- Report errors with JSON paths and object/component context.
- Perform atomic saves using a temporary file followed by rename.

Suggested interface:

```cpp
class SceneSerializer
{
public:
    static nlohmann::json Serialize(const Scene& scene);

    static LoadResult Deserialize(
        const nlohmann::json& document,
        Scene& scene,
        LoadContext& context);

    static bool SaveToFile(
        const Scene& scene,
        const std::filesystem::path& path);

    static LoadResult LoadFromFile(
        const std::filesystem::path& path,
        Scene& scene,
        LoadContext& context);
};
```

### Component serialization hooks

Use stable string identifiers rather than numeric `ComponentType` values.

```cpp
class Component
{
public:
    virtual std::string_view GetSerializationType() const noexcept = 0;
    virtual void SerializeData(nlohmann::json& out) const {}
    virtual void DeserializeData(
        const nlohmann::json& data,
        LoadContext& context) {}
};
```

Example type identifiers:

- `tapi.drawable`
- `tapi.animator`
- `tapi.rigidbody`
- `tapi.collider`
- `tapi.camera`
- `tapi.point_light`
- `tapi.spot_light`
- `tapi.directional_light`
- `tapi.script`

### ComponentRegistry

The registry centralizes construction requirements such as `Graphics&`, asset paths, and script class names. It should replace the component-creation knowledge currently embedded in the editor switch inside `App`.

```cpp
struct LoadContext
{
    Graphics& graphics;
    // Future: AssetManager&, Physics&, ProjectContext&, etc.
};

using ComponentFactory = std::function<Component&(
    GameObject& owner,
    const nlohmann::json& data,
    LoadContext& context)>;
```

The same registry should support:

- Add Component UI
- Scene loading
- Future component duplication
- Future prefabs

## Scene format outline

```json
{
  "format": "TapiScene",
  "version": 1,
  "scene": {
    "name": "Example Scene",
    "objects": [
      {
        "id": "219ef7b3-8b18-44de-9938-d7126ad24306",
        "name": "Player",
        "static": false,
        "transform": {
          "position": [0.0, 1.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "components": [
          {
            "id": "a283df36-66aa-40b8-b65a-b3587d817273",
            "type": "tapi.camera",
            "data": {}
          }
        ],
        "children": []
      }
    ]
  }
}
```

## Persistent identity

Keep the existing numeric `GameObject::nextId` and `Component::nextId` values as transient runtime IDs. Add a separate persistent UUID to both classes.

UUIDs will support:

- Cross-object and cross-component references
- Editor undo/redo
- Prefab overrides
- Script reference fields
- Stable animation and event bindings

Because TapiEngine is currently Windows-specific, a small wrapper around `CoCreateGuid` is sufficient. A separate GUID library is optional unless portability is a goal.

## Data that should be serialized

### Scene

- Format and schema version
- Scene name
- Root object list
- Future scene-level asset settings such as skybox reference

### GameObject

- Persistent UUID
- Name
- Static flag
- Local transform
- Components
- Child objects

### Components

- Persistent UUID
- Stable component type identifier
- Editable configuration needed to reconstruct the component
- Project-relative asset references

## Data that should be rebuilt

Do not serialize:

- Owner and parent pointers
- Scene drawable cache
- BVH nodes and proxies
- `ScriptManager` lifecycle queues
- Box3D body and shape handles
- GPU buffers, textures, and constant buffers
- Cached camera and light pointers in `App`
- Editor selection and ImGui state
- Animator pose, clip bindings, status strings, and file-browser state
- Pending-destruction and pending-component-removal state

## Component-specific requirements

### Camera and lights

- Add serialization for editable camera and light properties.
- Lights must be created through factories because their constructors require `Graphics&`.
- Add getters where serialization cannot currently access private state, or implement serialization as member overrides.

### DrawableComponent

Serialize the drawable kind and reconstruction data.

For a model:

```json
{
  "kind": "model",
  "asset": "Graphics/Models/Humanoid/Body.fbx",
  "localTransform": {}
}
```

For a primitive, include:

- Shape
- Surface mode
- Material properties
- Texture path
- Normal-map path and enabled state
- Drawable-local transform

`Model` must retain its project-relative source path; the current imported `ModelAsset` alone is insufficient to reconstruct it later.

### Animator

Load Animator only after its target Drawable/Model exists.

Persist:

- Animation source paths
- Loop setting per source or clip
- Selected clip
- Playback speed
- Optional editor-configured initial playback state

Do not persist evaluated pose or clip bindings. `Animator` must retain animation source metadata because the current `ClipEntry` does not preserve its import path.

### Rigidbody and Collider

Persist authored configuration only:

- Rigidbody type
- Gravity scale
- Linear and angular damping
- Motion locks
- Collider shape and geometry
- Density, friction, and restitution

Recreate Box3D objects through the existing `AddComponent` and physics initialization path.

### CustomBehaviour scripts

Reuse `ScriptRegistry` and store the registered script class name.

Before reading or writing fields:

1. Clear `properties`.
2. Call `ExposeVariables()`.
3. Process each `ExposedProperty` according to `PropertyType`.

Persist the script enabled state and a `fields` object keyed by exposed property name. Unknown saved fields should produce a warning; missing fields should retain compiled defaults.

## Deserialization pipeline

### Pass 1: Parse and validate

- Parse the complete document.
- Verify `format == "TapiScene"`.
- Check the schema version.
- Run migrations for older supported versions.
- Validate required fields, UUID syntax, duplicate UUIDs, component types, and asset paths.
- Do not modify the active scene if structural validation fails.

### Pass 2: Create hierarchy

- Clear or replace the active scene only after validation succeeds.
- Create every root and child `GameObject` parent-first.
- Restore UUID, name, static flag, and local transform.
- Build a `UUID -> GameObject*` lookup table.

### Pass 3: Construct components

Create components through `ComponentRegistry` in dependency order:

1. Drawable/model components
2. Camera, lights, and independent components
3. Rigidbody and Collider
4. Animator
5. CustomBehaviour scripts

Build a `UUID -> Component*` lookup table as components are created.

### Pass 4: Restore data and references

- Apply component payloads.
- Resolve GameObject and Component UUID references.
- Report unresolved references with their JSON path.
- Run optional component `PostLoad()` hooks after all references exist.

### Pass 5: Rebuild runtime state

- Confirm drawable and script registration.
- Rebuild or synchronize BVH state.
- Recreate physics bodies and shapes.
- Refresh cached cameras and lights in `App`.
- Keep scripts inactive until normal play-mode lifecycle processing begins.

## Versioning and error policy

- Begin with schema version `1`.
- Store enum values as strings, not integer ordinals.
- Ignore unknown ordinary JSON fields to preserve forward compatibility.
- Treat an unknown component type as a load error by default; do not silently delete its data.
- Preserve compiled defaults when optional fields are missing.
- Include object name, UUID, component type, and JSON path in errors.
- Keep migrations as explicit `version N -> N+1` functions.

## Implementation stages

### Stage 1 — Serialization foundation

- Add `nlohmann/json`.
- Add `Guid`, `JsonMath`, `SceneFormat`, context, and error/result types.
- Add JSON converters for `XMFLOAT2/3/4`, `Transform`, and required enums.
- Add persistent UUIDs to `GameObject` and `Component`.
- Add Scene name getter/setter.

**Exit criterion:** Utility types compile and UUID/math round-trip tests pass.

### Stage 2 — Scene graph round trip

- Implement recursive Scene and GameObject serialization.
- Implement parsing, validation, and atomic file output.
- Restore hierarchy, transforms, names, static flags, and UUIDs.
- Add duplicate UUID and malformed transform validation.

**Exit criterion:** A component-free nested scene saves and reloads with semantic equality.

### Stage 3 — ComponentRegistry and basic components

- Add stable component type keys.
- Implement `ComponentRegistry`.
- Refactor the editor Add Component flow to use the registry.
- Add Camera, light, Rigidbody, and Collider serialization.

**Exit criterion:** Basic components reconstruct through the same factory path used by the editor.

### Stage 4 — Resource-backed components

- Add Drawable serialization for Model and Primitive.
- Store model source paths.
- Store primitive reconstruction and material data.
- Add Animator source metadata and dependency-aware loading.

**Exit criterion:** The existing humanoid model and animation setup survives a save/load round trip.

### Stage 5 — CustomBehaviour serialization

- Reuse `ScriptRegistry` for reconstruction.
- Serialize enabled state and exposed fields.
- Validate unique exposed-property names and compatible property types.
- Warn about unknown fields while retaining compiled defaults for missing fields.

**Exit criterion:** `ScriptTest` values and enabled state round-trip correctly.

### Stage 6 — Runtime integration

- Add editor Save Scene and Open Scene commands.
- Rebuild App camera/light caches after load.
- Ensure loading occurs outside active simulation updates.
- Save to the current `.scene` file before entering Play; untitled scenes require Save As.
- Replace hard-coded scene creation in `ResetSimulation()` with saved-file loading; use the pre-play in-memory document if file loading fails.
- Keep file commands in Edit mode, prompt for unsaved Open changes, and refresh caches after scene replacement.

**Exit criterion:** Scenes can be opened repeatedly without duplicate registrations, stale pointers, or premature script lifecycle calls.

### Stage 7 — Robustness and migration tests

- Add round-trip tests for every supported component.
- Test nested hierarchy and local/world transform preservation.
- Test missing assets, unknown component types, malformed JSON, duplicate UUIDs, and unsupported versions.
- Test Drawable-before-Animator dependency handling.
- Test BVH, physics, and script registration after repeated loads.
- Add a sample version migration test.

**Exit criterion:** Invalid files fail with actionable errors and supported scenes load deterministically.

## Initial test checklist

- Empty scene round trip
- Deep parent/child hierarchy round trip
- Transform precision within an agreed epsilon
- GameObject and Component UUID preservation
- Camera and each light type
- Rigidbody plus Collider in either serialized order
- Primitive with material and texture settings
- Model asset reconstruction
- Animator sources, loop settings, selection, and speed
- CustomBehaviour exposed `int`, `float`, `string`, `Vector3`, `Color`, and `bool`
- Missing optional fields use defaults
- Unknown saved script field warns without failing
- Missing script class or component type fails clearly
- Failed validation does not partially clear the current scene
- Repeated load does not duplicate drawable, script, BVH, or physics registration

## Later extensions

- Prefab serialization using the same object/component schema
- Asset GUIDs and an `AssetManager` instead of path-only references
- Script fields that reference GameObjects, Components, and assets
- Undo/redo based on persistent IDs and JSON patches
- Binary scene packaging for release builds
- Separate runtime save-game serialization for dynamic simulation state

## Recommended first deliverable

Implement Stages 1 and 2 first, supporting only Scene, GameObject, Transform, hierarchy, names, static flags, and UUIDs. This establishes a testable file format before component construction and resource dependencies are introduced.
