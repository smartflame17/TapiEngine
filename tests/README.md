# Transform regression tests

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
