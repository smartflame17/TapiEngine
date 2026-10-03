# Scene serialization (stages 1–4)

`SceneSerializer` reads and writes version 1 `TapiScene` JSON. It preserves the
scene name, root/child order, object names, static flags, local transforms,
component order and persistent UUIDs. `SerializeGameObject()` is the public
recursive subtree writer; it uses exactly the same traversal and object schema
as full scene export, so future prefab export can reuse it.

Scene files contain authored reconstruction metadata. Model vertices, skeletons,
animation tracks, image bytes, GPU objects and physics handles come from resource
loading or component initialization. Animation playback time/state, evaluated
poses, physics velocities, editor selection and lifecycle queues are not saved.

## Using the API

```cpp
#include "Serialization/SceneSerializer.h"

SerializationContext saveContext;
saveContext.projectRoot = projectDirectory;

auto document = SceneSerializer::Serialize(scene, saveContext);
auto subtree = SceneSerializer::SerializeGameObject(object, saveContext);
auto saved = SceneSerializer::SaveToFile(scene, sceneFile, saveContext);

LoadContext loadContext{ graphics };
loadContext.projectRoot = projectDirectory;
auto loaded = SceneSerializer::LoadFromFile(sceneFile, scene, loadContext);
// Or: SceneSerializer::Deserialize(document, scene, loadContext);
if (!loaded)
{
    for (const auto& error : loaded.errors)
        Report(error.jsonPath, error.message);
}
```

`projectRoot` defaults to the working directory. Use the same explicit project
root for save and load when the process runs elsewhere. Asset references are
UTF-8 project-relative paths, independent of the scene file's directory. Empty
texture references select a fallback; model and animation references are required.
Absolute references and paths that escape the project are rejected. Path
normalization does not require physical files, allowing virtual resource loaders.

In-memory serialization throws `SerializationException` with an attached
diagnostic. File operations and deserialization return `LoadResult`, whose
diagnostics include a JSON path, object name/UUID and component type/UUID when
available. Unknown ordinary fields are ignored; missing optional component fields
retain compiled defaults. Required structural fields, transforms, UUIDs, type
keys and payloads are validated before reconstruction. UUIDs must be non-null
and unique across both objects and components. Only schema version 1 is accepted.

Saving serializes first, then writes a temporary sibling file and atomically
replaces the destination. Loading constructs a detached candidate hierarchy in
dependency order. Validation or resource reconstruction failures retain the
active hierarchy and the caller's identity lookup maps. On success, the old
scene is replaced, drawables are registered, physics bodies/shapes are recreated,
and `loadContext.gameObjects`/`components` contain the restored UUID lookups.
Component construction order does not change authored inspector order.

Loading is synchronous and must run between updates/rendering. App camera/light
cache refresh, editor Save/Open commands and play-mode reset integration remain
stage 6 work; this layer does not update App's cached component pointers.

## Resource reconstruction and the future AssetManager

`LoadContext::assets` accepts an `AssetLoader`. The default `FileAssetLoader` uses
the existing model/animation importers and reads encoded image bytes from disk.
Replace it with an adapter around a future AssetManager without changing scene
JSON or the reconstruction pipeline:

```cpp
#include "Graphics/Assets/AssetLoader.h"

class ManagedAssets : public AssetLoader
{
public:
    std::shared_ptr<const ModelAsset> LoadModel(
        const std::filesystem::path& reference) override;
    std::vector<std::shared_ptr<const Animation::AnimationClip>> LoadAnimations(
        const std::filesystem::path& reference) override;
    std::shared_ptr<const TextureAsset> LoadTexture(
        const std::filesystem::path& reference) override;
};

ManagedAssets assets;
LoadContext context{ graphics };
context.projectRoot = projectDirectory;
context.assets = &assets;
auto result = SceneSerializer::Deserialize(document, scene, context);
```

The loader receives normalized absolute paths representing project references;
it may resolve them through a cache or virtual resource store. It returns actual
CPU resource data, separate from the serialized metadata. The injection points
are `Model(Graphics&, shared_ptr<const ModelAsset>, sourcePath, AssetLoader*)`,
`Primitive::SetResources()`, `Mesh::SetResources()`, `Texture::SetAsset()` and
`Animator::AddClip(shared_ptr<const AnimationClip>, sourcePath, sourceClip, loop)`.
Model texture dependencies also use the injected loader. Texture reconstruction
creates GPU textures from supplied bytes without reading the referenced path.
Components retain shared resources; the loader itself is borrowed only during
reconstruction. Cache and unload policy can therefore belong to the AssetManager.

Model files retain their source reference; in-memory models must receive one to
be saved. Animator imports retain source path and source clip index, including
files containing multiple clips. Source-less synthetic clips cannot be saved.
Resources must remain compatible with their saved node/mesh/clip indices.

## Supported component metadata

| Stable key | Saved configuration |
| --- | --- |
| `tapi.camera` | Movement speed and rotation sensitivity; pose belongs to the object transform |
| `tapi.point_light` | Color, intensity, attenuation and gizmo radius |
| `tapi.spot_light` | Color, intensity, attenuation, cone angles in radians and gizmo radius |
| `tapi.directional_light` | Color and intensity; direction follows object rotation |
| `tapi.rigidbody` | Body type, gravity scale, damping and motion locks |
| `tapi.collider` | Shape, center, dimensions, density, friction and restitution |
| `tapi.drawable` | Model asset reference or primitive shape/surface, material properties, texture/normal-map references, normal-map enabled state, sampler and drawable-local transform |
| `tapi.animator` | Source references and clip indices, loop flags, selected clip and playback speed |

Models save authored static-node and mesh material/texture/sampler overrides;
imported geometry, bind poses and animated node poses are rebuilt. Animator loads
after its Drawable and resumes in the stopped state at time zero. Rigidbody loads
before Collider through the engine's existing `AddComponent` physics path.

Script field serialization (stage 5), runtime integration (stage 6), schema
migrations and the broader robustness suite (stage 7) are outside this delivery.
Scripts and AudioClip keep their editor factories but cause a clear serialization
error until their payloads are supported; they are never silently omitted.
Scene-level skybox settings are reserved for future scene metadata.

## Registry and component hooks

`ComponentRegistry::Builtins()` owns the stable keys, pure metadata validators,
factories and construction priority. Both the editor's Add Component flow and
scene loading call `ComponentRegistry::Create()`. A registration may be
editor-only (`sceneSupported = false`) and may restrict its type to one component
per object. Lower `loadOrder` values construct first.

Components implement `GetSerializationType()`, `SerializeData()` and
`DeserializeData()`. To extend the system, copy the built-in registry, register
a unique stable key with a factory and validator, then assign that registry to
both contexts. Validators must inspect metadata without loading resources or
mutating the scene. Factories construct through `GameObject::AddComponent()` and
apply payloads using the context's dependencies. Default hooks reject unsupported
components so a save cannot silently lose them.

## Foundation types and checks

The vendored `Tools/json.hpp` provides nlohmann/json 3.12.0. Include `Json.h` for
Guid conversion, `JsonMath.h` for DirectX floats/Transform, and `JsonEnums.h` for
explicit string enum values. Object/component creation generates a UUID;
`SetId()` restores it and rejects null. There is no global numeric ID counter.
Math arrays require finite values of the correct length; transforms require
position, rotation and scale. UUID output is canonical lowercase text.

Build the engine in x64, then run:

```powershell
msbuild TapiEngine.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
powershell -File tests/RunSerializationTests.ps1 -Suite All -Configuration Debug
```

Use `-Suite Utilities` for standalone UUID/math/enum checks, `-Suite Scene` for
foundation object checks, or `-Suite RoundTrip` for stages 2–4. `-Configuration Release`
is also supported after building Release. The round-trip suite covers hierarchy/file
output, basic component reconstruction, the existing humanoid and animations,
primitive materials/textures, resource injection using virtual model and UTF-8
texture references, and failures required by this file format.
