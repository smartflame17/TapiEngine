# Transform regression tests

## Serialization and editor workflow tests

Build Debug/Release x64, then run `./tests/RunSerializationTests.ps1 -Suite All
-Configuration Debug` and repeat with Release. The All suite covers utilities,
hierarchy/resources, all six exposed script types, field compatibility and
warnings, constructor failures, lifecycle registration, and editor commands.
Use `-Suite Scripts` or `-Suite App` for the new suites alone.

App checks use the production command handler and hidden D3D editor window.
They verify save-before-Play, Stop/Escape restoration, fresh physics resources,
camera/light caches, unsaved Open choices, overwrite/cancellation, disabled file
commands during Play, and file/backup recovery failures.

`-Suite Walkthrough` builds an interactive editor harness without starting it.
Launch its printed path to inspect ScriptTest on Material Cube, save/reopen a
`.scene`, enter Play, and verify Escape restores the cube's authored pose and
script values. It uses the real App UI and avoids modifying `imgui.ini`.

## Audio tests

For the audio service, run `./tests/RunAudioTests.ps1 -Configuration Debug` and
repeat with `Release`. The Commands suite checks FIFO ordering, worker/COM
ownership, loading outside the queue lock, idle updates, completion cleanup,
stale handles, device recovery, startup/worker failures, and shutdown using an
instrumented backend. The Native suite links real DirectXTK and exercises three
service lifetimes with a generated silent Unicode-named WAV, checking for
asynchronous errors without asserting audible output. Neither needs scene assets
or an engine build. Use `-Suite Commands` or `-Suite Native` to select one suite.

The default `All` suite also runs AudioClip component tests and requires the
corresponding x64 engine build. Use `-Suite Components` to run these alone. They
link the engine's component/scene objects with the instrumented audio backend,
checking script lookup, UTF-8 paths, live controls, overlapping one-shots, restart,
natural completion, global stops, component removal, subtree destruction, scene
reset, invalid settings, missing services and worker failures. Headless ImGui
frames exercise path input, sliders, transport buttons, multiple component IDs,
file browser opening, clearing and persistent error text. No audio device, scene
assets or visible editor window are required.

## Physics tests

Run `./tests/RunPhysicsTests.ps1 -Configuration Debug` and repeat with `Release`.
These headless tests exercise real Box3D worlds, singleton cleanup/reset,
falling-body motion, and fixed timing at different rendering rates and stalls.
No engine build or scene assets are required for this default Core suite.

After building the corresponding x64 engine configuration, add `-Suite Components`
for the headless component tests, `-Suite App` for the hidden-window App smoke
tests, or `-Suite All` for all three suites. App tests need
the default scene assets and D3D11. They cover Play/Pause/Resume/Stop, Escape,
script ordering and same-tick transform readback, pause stability, and world
lifetime during reset and shutdown. Component tests exercise both attachment
orders, pending removal/re-addition, subtree destruction, all three collider
shapes contacting a standalone floor, geometry/material/mass edits, immediate
setters, quaternion round trips, sleeping independent children, parent edits,
reparenting, scale suspension/recovery, inspector warnings and text edits, and
the gizmo transform entry point. They do not simulate a mouse drag in ImGuizmo.
See `Physics/README.md` for the service contract and timing behavior.

The Core suite also verifies debug wireframe geometry, all collider overlap
pairs, native contacts while resting/asleep/paused, draw bounds, visibility,
and cached-shape lifetime. Components checks cover debug geometry under scaled
and rotated parents, capsule-to-sphere clamping, suspension, and component
replacement. App checks read back the D3D11 render target for idle/collision
and custom colors, draw-through visibility, BVH depth behavior, Play/Pause
visibility, camera positions beyond Box3D's default drawing bounds, and line
buffer growth. App also activates the Physics and General settings tabs and
their visibility checkboxes. PNG captures are saved in
`x64/PhysicsTests/<configuration>/`.

## Skeletal animation tests

After building the engine in the corresponding x64 configuration, run:

```powershell
./tests/RunAnimationTests.ps1 -Configuration Debug
./tests/RunAnimationTests.ps1 -Configuration Release
```

Use `-Suite Cpu`, `-Suite Gpu`, or `-Suite Integration` to run only one suite.
The script locates the installed Visual Studio C++ compiler and Windows SDK.
Integration tests link the engine's compiled objects, so rebuild the engine
after source changes. They use a hidden test window, without launching the
interactive editor. Local Humanoid FBXs are optional; synthetic fixtures cover
the core animation, controls, removal, culling, and shadow behavior independently.

The GPU suite reads back the actual skinned vertex-shader output and compares it
with a CPU reference. Integration checks cover automatic playback in Edit mode,
global pause in Play mode, component removal/replacement, animated bounds and
picking, offscreen animation entering the frustum, and moving shadow silhouettes.
When the local assets exist, render captures are saved below
`x64/AnimationTests/<configuration>/`.

See `Graphics/Animation/README.md` for the asset structure and renderer pipeline.

## Standalone transform tests

Run from the repository root in a Visual Studio Developer PowerShell with the
C++ toolchain and Windows SDK installed:

```powershell
New-Item -ItemType Directory -Force x64/TransformTests | Out-Null
cl /nologo /EHsc /std:c++17 /W4 /O2 /fp:fast tests/TransformTests.cpp /Fo:x64/TransformTests/TransformTests.obj /Fe:x64/TransformTests/TransformTests.exe
if ($LASTEXITCODE -eq 0) { & ./x64/TransformTests/TransformTests.exe }
```

The executable returns a nonzero exit code on failure. It compares reconstructed
matrices, since multiple Euler angle triples can describe the same orientation.
Coverage includes combined rotations, nonuniform positive object scales, pitch
singularities, repeated matrix conversions, incremental local/world rotations,
and conversion through a rotated, uniformly scaled parent. These are numerical
tests of the transform path; they do not simulate mouse input into ImGuizmo.

The decomposition assumes an S/R/T matrix with positive, nonzero scales and no
shear. Rotating children under nonuniformly scaled parents can introduce shear,
which the current `Transform` representation cannot store exactly.
