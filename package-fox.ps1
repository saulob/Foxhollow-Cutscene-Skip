$ErrorActionPreference = "Stop"

$RepoRoot = $PSScriptRoot
$SourceManifest = Join-Path $RepoRoot "mod.json"
$BuildPackageDir = Join-Path $RepoRoot "build\cutscene-skip"
$BuiltManifest = Join-Path $BuildPackageDir "mod.json"
$BuiltDll = Join-Path $BuildPackageDir "lib\windows-amd64\mod.dll"
$StagingDir = Join-Path $RepoRoot "build\fox-package"
$StagingLibDir = Join-Path $StagingDir "lib\windows-amd64"
$DistDir = Join-Path $RepoRoot "dist"

if (-not (Test-Path -LiteralPath $SourceManifest -PathType Leaf)) {
    throw "mod.json not found: $SourceManifest"
}

$Version = (Get-Content -LiteralPath $SourceManifest -Raw | ConvertFrom-Json).version
if ([string]::IsNullOrWhiteSpace($Version)) {
    throw "mod.json does not define a version: $SourceManifest"
}

if (-not (Test-Path -LiteralPath $BuiltManifest -PathType Leaf)) {
    throw "Built mod.json not found: $BuiltManifest (build the Release configuration first)"
}

if (-not (Test-Path -LiteralPath $BuiltDll -PathType Leaf)) {
    throw "Built mod.dll not found: $BuiltDll (build the Release configuration first)"
}

$BuiltVersion = (Get-Content -LiteralPath $BuiltManifest -Raw | ConvertFrom-Json).version
if ($BuiltVersion -ne $Version) {
    throw "Version mismatch: mod.json is $Version but $BuiltManifest is $BuiltVersion (rebuild before packaging)"
}

if (Test-Path -LiteralPath $StagingDir) {
    Remove-Item -LiteralPath $StagingDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $StagingLibDir | Out-Null
Copy-Item -LiteralPath $BuiltManifest -Destination (Join-Path $StagingDir "mod.json")
Copy-Item -LiteralPath $BuiltDll -Destination (Join-Path $StagingLibDir "mod.dll")

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$FoxPath = Join-Path $DistDir "cutscene-skip-$Version.fox"
if (Test-Path -LiteralPath $FoxPath) {
    Remove-Item -LiteralPath $FoxPath -Force
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$Files = [ordered]@{
    "mod.json" = Join-Path $StagingDir "mod.json"
    "lib/windows-amd64/mod.dll" = Join-Path $StagingLibDir "mod.dll"
}

$Archive = [System.IO.Compression.ZipFile]::Open($FoxPath, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    [void]$Archive.CreateEntry("lib/")
    [void]$Archive.CreateEntry("lib/windows-amd64/")
    foreach ($File in $Files.GetEnumerator()) {
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $Archive, $File.Value, $File.Key, [System.IO.Compression.CompressionLevel]::Optimal)
    }
}
finally {
    $Archive.Dispose()
}

Remove-Item -LiteralPath $StagingDir -Recurse -Force

Write-Host "Packaged Cutscene Skip $Version"
foreach ($Entry in $Files.Keys) {
    Write-Host "  $Entry"
}
Write-Host $FoxPath
