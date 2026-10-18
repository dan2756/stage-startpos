# Builds the mod and installs it into the Geode CLI profile's mods folder.
# Restart Geometry Dash afterwards to load the new build.
$ErrorActionPreference = "Stop"

$geodeRoot = "$env:USERPROFILE\Geode"
$buildDir = "$PSScriptRoot\build"

$env:GEODE_SDK = "$geodeRoot\sdk"
$env:CPM_SOURCE_CACHE = "$geodeRoot\cpm-cache"
# The bindings the installed loader was built with, pinned so a bindings update
# can't break the build or change member layouts under the mod.
if (Test-Path "$geodeRoot\bindings") { $env:GEODE_BINDINGS_REPO_PATH = "$geodeRoot\bindings" }
$env:PATH = "$geodeRoot\cli;$env:PATH"

# The Visual Studio install that ships CMake and Ninja next to MSVC
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
if (-not $vs) { throw "No Visual Studio install with the C++ CMake tools was found." }

$steps = @(
    "`"$vs\VC\Auxiliary\Build\vcvars64.bat`"",
    # Codegen is built from the pinned bindings: the prebuilt download is always
    # the newest one, which can be ahead of the SDK
    "cmake -G Ninja -S `"$PSScriptRoot`" -B `"$buildDir`" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSKIP_BUILDING_CODEGEN=OFF",
    "cmake --build `"$buildDir`""
)
cmd /c ($steps -join " && ")
exit $LASTEXITCODE
