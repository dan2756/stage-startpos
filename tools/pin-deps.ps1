# Geode CLI builds against the newest index version that matches each
# dependency in mod.json, whatever GD version that is for, unless a matching
# .geode is already in its cache (the temp folder). This puts the last versions
# for GD 2.2081 there first, so builds link against what players actually have.
$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$pins = [ordered]@{
    "cvolton.level-id-api" = "2.0.0"
    "geode.node-ids"       = "1.23.3"
    "zhulis.blitzkrieg"    = "2.3.5"
}

# Rust's temp_dir() reads TMP before TEMP, so cover every spelling of it
$dirs = @(@([IO.Path]::GetTempPath(), $env:TMP, $env:TEMP) |
    Where-Object { $_ } | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\') } | Select-Object -Unique)

foreach ($id in $pins.Keys) {
    $version = $pins[$id]
    $info = (Invoke-RestMethod "https://api.geode-sdk.org/v1/mods/$id/versions/$version" -UserAgent "stage-startpos-build").payload

    $download = Join-Path $dirs[0] "$id.geode.download"
    Invoke-WebRequest $info.download_link -OutFile $download -UseBasicParsing -UserAgent "stage-startpos-build"

    $sha = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($download)
    try { $hash = -join ($sha.ComputeHash($stream) | ForEach-Object { $_.ToString("x2") }) } finally { $stream.Dispose() }
    if ($hash -ne $info.hash) { throw "$id $version failed its hash check" }

    foreach ($dir in $dirs) { Copy-Item -LiteralPath $download -Destination (Join-Path $dir "$id.geode") -Force }
    Remove-Item -LiteralPath $download
    Write-Host "Pinned $id $version"
}
