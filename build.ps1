# Builds the mod and installs it into the Geode CLI profile's mods folder.
# Restart Geometry Dash afterwards to load the new build.
$ErrorActionPreference = "Stop"

$geodeRoot = "C:\Users\Public\Documents\Geode"
$buildDir = "$geodeRoot\build\blitzkrieg-target"

$env:GEODE_SDK = "$geodeRoot\sdk"
$env:CPM_SOURCE_CACHE = "$geodeRoot\cpm-cache"
$env:PATH = "$geodeRoot\cli;$env:PATH"

# The Visual Studio install that ships CMake and Ninja next to MSVC
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
if (-not $vs) { throw "No Visual Studio install with the C++ CMake tools was found." }

$steps = @(
    "`"$vs\VC\Auxiliary\Build\vcvars64.bat`"",
    "cmake -G Ninja -S `"$PSScriptRoot`" -B `"$buildDir`" -DCMAKE_BUILD_TYPE=RelWithDebInfo",
    "cmake --build `"$buildDir`""
)
cmd /c ($steps -join " && ")
exit $LASTEXITCODE
