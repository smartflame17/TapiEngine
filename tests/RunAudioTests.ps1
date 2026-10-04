param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('All','Commands','Native','Components')][string]$Suite = 'All')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools were not found.' }
$compilerVersion = (Get-ChildItem -LiteralPath "$vs\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$vc = "$vs\VC\Tools\MSVC\$compilerVersion"
$kit = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdk = (Get-ChildItem -LiteralPath "$kit\Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$out = Join-Path $repo "x64/AudioTests/$Configuration"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$includes = @("/I$vc/include", "/I$kit/Include/$sdk/ucrt", "/I$kit/Include/$sdk/shared", "/I$kit/Include/$sdk/um", "/I$kit/Include/$sdk/winrt")
$libraries = @("/LIBPATH:$vc/lib/x64", "/LIBPATH:$kit/Lib/$sdk/ucrt/x64", "/LIBPATH:$kit/Lib/$sdk/um/x64")
$runtime = if ($Configuration -eq 'Debug') { @('/MDd','/Od','/D_DEBUG') } else { @('/MD','/O2','/DNDEBUG') }
$common = @('/nologo','/EHsc','/std:c++17','/W4','/utf-8','/fp:fast') + $runtime + $includes
$cl = "$vc/bin/Hostx64/x64/cl.exe"
function Run-Test([string]$Name, [string]$Arguments = '') {
    $options = @{
        FilePath = "$out/$Name.exe"; WorkingDirectory = $repo; WindowStyle = 'Hidden'; PassThru = $true
        RedirectStandardOutput = "$out/$Name-stdout.log"; RedirectStandardError = "$out/$Name-stderr.log"
    }
    if ($Arguments) { $options.ArgumentList = $Arguments }
    $process = Start-Process @options
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw "$Name timed out." }
    $process.WaitForExit()
    Get-Content -LiteralPath "$out/$Name-stdout.log"
    Get-Content -LiteralPath "$out/$Name-stderr.log"
    if ($process.ExitCode) { throw "$Name failed (exit $($process.ExitCode))." }
    if ($Name -eq 'AudioSmokeTests' -and (Get-Item -LiteralPath "$out/$Name-stderr.log").Length) {
        throw 'Native smoke test reported asynchronous audio errors.'
    }
}
Push-Location $repo
try {
    if ($Suite -in @('All','Commands')) {
        & $cl @common "/I$repo/tests/AudioFake" tests/AudioTests.cpp Audio/Audio.cpp "/Fo$out/" "/Fe$out/AudioTests.exe" /link @libraries ole32.lib
        if ($LASTEXITCODE) { throw 'Audio command test compilation failed.' }
        Run-Test 'AudioTests'
    }
    if ($Suite -in @('All','Native','Components')) {
        [xml]$packages = Get-Content -LiteralPath "$repo/packages.config"
        $tkVersion = ($packages.packages.package | Where-Object { $_.id -eq 'directxtk_desktop_win10' }).version
        $tk = Join-Path $repo "packages/directxtk_desktop_win10.$tkVersion"
    }
    if ($Suite -in @('All','Native')) {
        & $cl @common "/I$tk/include" tests/AudioSmokeTests.cpp Audio/Audio.cpp "/Fo$out/" "/Fe$out/AudioSmokeTests.exe" /link @libraries "/LIBPATH:$tk/native/lib/x64/$Configuration" DirectXTK.lib ole32.lib uuid.lib
        if ($LASTEXITCODE) { throw 'Native audio smoke compilation failed.' }
        Run-Test 'AudioSmokeTests' ('"' + $out + '"')
    }
    if ($Suite -in @('All','Components')) {
        [xml]$project = Get-Content -LiteralPath "$repo/TapiEngine.vcxproj"
        $objects = @($project.Project.ItemGroup.ClCompile | Where-Object { $_.Include } | ForEach-Object {
            $name = [System.IO.Path]::GetFileNameWithoutExtension($_.Include)
            if ($name -notin @('App','SceneCommands','WinMain','Audio')) { Join-Path $repo "TapiEngine/x64/$Configuration/$name.obj" }
        })
        foreach ($object in $objects) {
            if (!(Test-Path -LiteralPath $object)) { throw "Build the engine in $Configuration x64 before running Components tests (missing $object)." }
        }
        $box3dName = if ($Configuration -eq 'Debug') { 'box3dd' } else { 'box3d' }
        $box3dDir = Join-Path $repo "Physics/box3d/$Configuration"
        $defines = if ($Configuration -eq 'Debug') { '/DIS_DEBUG=true' } else { '/DIS_DEBUG=false' }
        & $cl @common /std:c++20 $defines "/I$repo/tests/AudioFake" "/I$repo/spdlog/include" "/I$repo/assimp/include" "/I$tk/include" tests/AudioClipTests.cpp Audio/Audio.cpp "/Fo$out/" "/Fe$out/AudioClipTests.exe" /link @libraries @objects "/LIBPATH:$repo/assimp/lib" "/LIBPATH:$tk/native/lib/x64/$Configuration" "$box3dDir/$box3dName.lib" DirectXTK.lib assimp-vc143-mtd.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib
        if ($LASTEXITCODE) { throw 'AudioClip test compilation failed.' }
        Copy-Item -LiteralPath "$repo/assimp/bin/assimp-vc143-mtd.dll" -Destination $out -Force
        Copy-Item -LiteralPath "$box3dDir/$box3dName.dll" -Destination $out -Force
        Run-Test 'AudioClipTests'
    }
} finally { Pop-Location }
