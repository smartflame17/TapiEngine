# Transform regression tests

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
