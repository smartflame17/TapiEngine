# Serialization foundation (stage 1)

The existing `Tools/json.hpp` supplies nlohmann/json 3.12.0. Include `Json.h`
for UUID conversions, `JsonMath.h` for DirectX floats and `Transform`, and
`JsonEnums.h` for the authored component, physics, primitive, light, script
property and playback enums.

```cpp
const Guid id = Guid::Generate();
const auto saved = nlohmann::json(id).dump();
const Guid restored = nlohmann::json::parse(saved).get<Guid>();

nlohmann::json transformData = object.GetTransform();
object.SetTransform(transformData.get<Transform>());
object.SetId(restored);
scene.SetName("Example Scene");
```

`GameObject::GetId()` and `Component::GetId()` now return a `const Guid&`.
Creation generates an identity; `SetId()` restores one and rejects null UUIDs.
There is no numeric lifetime counter. Copying objects or components is disabled
so identities cannot accidentally be duplicated. The future scene loader must
validate scene-wide UUID uniqueness before assigning saved IDs.

UUID text uses 36 hexadecimal characters and hyphens, accepting either hex case
and writing lowercase. A default `Guid` is null, for optional runtime references.
Math arrays require exactly 2, 3 or 4 finite numeric values within float range.
Transforms require `position`, `rotation` and `scale`; extra fields are ignored.
Enums use explicit strings, including `tapi.*` component keys. Malformed values
throw exceptions, and failed decoding preserves the destination value.

`SceneFormat`, `LoadContext` and `LoadResult` provide version constants, loader
dependencies and UUID lookup maps, and contextual error/warning collections.
Scene traversal, files and component payloads are implemented in later stages.

Run utility tests with `powershell -File tests/RunSerializationTests.ps1 -Suite Utilities`.
After building the engine in x64, run the same script with `-Suite All` to also
check identity restoration, hierarchy operations and scene names. Both commands
accept `-Configuration Debug` or `-Configuration Release`.
