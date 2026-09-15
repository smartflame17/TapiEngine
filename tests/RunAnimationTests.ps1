param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('All','Cpu','Gpu','Integration')][string]$Suite = 'All')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools were not found.' }
$compilerVersion = (Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$vc = "$vs\VC\Tools\MSVC\$compilerVersion"
$kit = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdk = (Get-ChildItem -LiteralPath "$kit\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$out = Join-Path $repo "x64\AnimationTests\$Configuration"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$includes = @("/I$vc\include", "/I$kit\Include\$sdk\ucrt", "/I$kit\Include\$sdk\shared", "/I$kit\Include\$sdk\um", "/I$kit\Include\$sdk\winrt", "/I$repo\assimp\include", "/I$repo\spdlog\include")
$libraries = @("/LIBPATH:$vc\lib\x64", "/LIBPATH:$kit\Lib\$sdk\ucrt\x64", "/LIBPATH:$kit\Lib\$sdk\um\x64", "/LIBPATH:$repo\assimp\lib")
$runtime = if ($Configuration -eq 'Debug') { '/MDd' } else { '/MD' }
$common = @('/nologo','/EHsc','/std:c++20','/W4','/utf-8','/fp:fast', $runtime) + $includes
$cl = "$vc\bin\Hostx64\x64\cl.exe"
Push-Location $repo
try {
    if ($Suite -in @('All','Cpu')) {
    & $cl @common tests/AnimationTests.cpp Graphics/Animation/Animation.cpp Graphics/Assets/AssimpImporter.cpp "/Fo$out\" "/Fe$out\AnimationTests.exe" /link @libraries assimp-vc143-mtd.lib
    if ($LASTEXITCODE) { throw 'Animation test compilation failed.' }
    Copy-Item -LiteralPath "$repo\assimp\bin\assimp-vc143-mtd.dll" -Destination $out -Force
    & "$out\AnimationTests.exe"
    if ($LASTEXITCODE) { throw 'Animation tests failed.' }
    & $cl @common tests/TransformTests.cpp "/Fo$out\TransformTests.obj" "/Fe$out\TransformTests.exe" /link @libraries
    if ($LASTEXITCODE) { throw 'Transform test compilation failed.' }
    & "$out\TransformTests.exe"
    if ($LASTEXITCODE) { throw 'Transform tests failed.' }
    }
    if ($Suite -in @('All','Gpu')) {
    & $cl @common tests/SkinningGpuTests.cpp Graphics/Animation/Animation.cpp "/Fo$out\" "/Fe$out\SkinningGpuTests.exe" /link @libraries d3d11.lib d3dcompiler.lib
    if ($LASTEXITCODE) { throw 'GPU test compilation failed.' }
    & "$out\SkinningGpuTests.exe"
    if ($LASTEXITCODE) { throw 'GPU skinning tests failed.' }
    }
    if ($Suite -in @('All','Integration')) {
        $tk = Join-Path $repo 'packages/directxtk_desktop_win10.2025.7.10.1'
        [xml]$project = Get-Content -LiteralPath "$repo/TapiEngine.vcxproj"
        $objects = @($project.Project.ItemGroup.ClCompile | Where-Object { $_.Include } | ForEach-Object {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($_.Include)
            if ($name -notin @('App','WinMain')) { Join-Path $repo "TapiEngine/x64/$Configuration/$name.obj" }
        })
        if (!$objects.Count) { throw "Build the engine in $Configuration x64 before running integration tests." }
        $defines = if ($Configuration -eq 'Debug') { @('/D_DEBUG','/DIS_DEBUG=true') } else { @('/DNDEBUG','/DIS_DEBUG=false') }
        & $cl @common @defines "/I$tk/include" tests/AnimationIntegrationTests.cpp "/Fo$out\AnimationIntegrationTests.obj" "/Fe$out\AnimationIntegrationTests.exe" /link @libraries "/LIBPATH:$tk/native/lib/x64/$Configuration" @objects DirectXTK.lib assimp-vc143-mtd.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib
        if ($LASTEXITCODE) { throw 'Integration test compilation failed.' }
        Copy-Item -LiteralPath "$repo\assimp\bin\assimp-vc143-mtd.dll" -Destination $out -Force
        & "$out\AnimationIntegrationTests.exe" $out
        if ($LASTEXITCODE) { throw 'Animation integration tests failed.' }
    }
} finally { Pop-Location }
