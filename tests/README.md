# Transform regression tests

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
