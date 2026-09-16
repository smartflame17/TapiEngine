param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('Core','App','All')][string]$Suite = 'Core')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools were not found.' }
$compilerVersion = (Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$vc = "$vs\VC\Tools\MSVC\$compilerVersion"
$kit = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdk = (Get-ChildItem -LiteralPath "$kit\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$out = Join-Path $repo "x64/PhysicsTests/$Configuration"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$box3dName = if ($Configuration -eq 'Debug') { 'box3dd' } else { 'box3d' }
$box3dDir = Join-Path $repo "Physics/box3d/$Configuration"
$includes = @("/I$vc/include", "/I$kit/Include/$sdk/ucrt", "/I$kit/Include/$sdk/shared", "/I$kit/Include/$sdk/um", "/I$kit/Include/$sdk/winrt", "/I$repo/Physics/box3d/include")
$libraries = @("/LIBPATH:$vc/lib/x64", "/LIBPATH:$kit/Lib/$sdk/ucrt/x64", "/LIBPATH:$kit/Lib/$sdk/um/x64", "$box3dDir/$box3dName.lib")
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/Od','/D_DEBUG','/DIS_DEBUG=true') } else { @('/MD','/O2','/DNDEBUG','/DIS_DEBUG=false') }
$common = @('/nologo','/EHsc','/std:c++20','/W4','/utf-8','/fp:fast') + $runtime + $includes
$cl = "$vc/bin/Hostx64/x64/cl.exe"
Push-Location $repo
try {
    Copy-Item -LiteralPath "$box3dDir/$box3dName.dll" -Destination $out -Force
    if ($Suite -in @('Core','All')) {
        & $cl @common tests/PhysicsTests.cpp Physics/Physics.cpp "/Fo$out\" "/Fe$out/PhysicsTests.exe" /link @libraries
        if ($LASTEXITCODE) { throw 'Physics test compilation failed.' }
        & "$out/PhysicsTests.exe"
        if ($LASTEXITCODE) { throw 'Physics tests failed.' }
    }
    if ($Suite -in @('App','All')) {
        $tk = Join-Path $repo 'packages/directxtk_desktop_win10.2025.7.10.1'
        [xml]$project = Get-Content -LiteralPath "$repo/TapiEngine.vcxproj"
        $objects = @($project.Project.ItemGroup.ClCompile | Where-Object { $_.Include } | ForEach-Object {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($_.Include)
            if ($name -ne 'WinMain') { Join-Path $repo "TapiEngine/x64/$Configuration/$name.obj" }
        })
        foreach ($object in $objects) {
            if (!(Test-Path -LiteralPath $object)) { throw "Build the engine in $Configuration x64 before running App tests (missing $object)." }
        }
        & $cl @common "/I$repo/assimp/include" "/I$repo/spdlog/include" "/I$tk/include" tests/PhysicsAppTests.cpp "/Fo$out/PhysicsAppTests.obj" "/Fe$out/PhysicsAppTests.exe" /link @libraries "/LIBPATH:$repo/assimp/lib" "/LIBPATH:$tk/native/lib/x64/$Configuration" @objects DirectXTK.lib assimp-vc143-mtd.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib
        if ($LASTEXITCODE) { throw 'Physics App test compilation failed.' }
        Copy-Item -LiteralPath "$repo/assimp/bin/assimp-vc143-mtd.dll" -Destination $out -Force
        $process = Start-Process -FilePath "$out/PhysicsAppTests.exe" -WorkingDirectory $repo -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput "$out/app-stdout.log" -RedirectStandardError "$out/app-stderr.log"
        Get-Content -LiteralPath "$out/app-stdout.log"
        Get-Content -LiteralPath "$out/app-stderr.log"
        if ($process.ExitCode) { throw "Physics App tests failed (exit $($process.ExitCode))." }
    }
} finally { Pop-Location }
