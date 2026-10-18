[CmdletBinding()]
param(
    [string]$ArmCompiler = 'arm-none-eabi-g++',
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$taskSdkLock = Get-Content -LiteralPath (Join-Path $taskRoot 'sdk.lock.json') -Raw | ConvertFrom-Json
$taskSdkDirectory = Join-Path $taskRoot 'third_party\distingNT_API'
$taskApiHeader = Join-Path $taskSdkDirectory 'include\distingnt\api.h'
if (-not (Test-Path -LiteralPath $taskApiHeader -PathType Leaf)) {
    throw 'Import the pinned official SDK into third_party/distingNT_API first.'
}
if ($taskSdkLock.api_version -ne 13) {
    throw 'The hardware build requires the reviewed API 13 contract in sdk.lock.json.'
}
$taskApiHeaderText = Get-Content -LiteralPath $taskApiHeader -Raw -Encoding UTF8
if ($taskApiHeaderText -notmatch '\bkNT_apiVersionCurrent\s*=\s*kNT_apiVersion13\b') {
    throw 'The hardware build requires an API 13 SDK header; restore the revision pinned in sdk.lock.json.'
}
$taskApiHeaderHash = (Get-FileHash -LiteralPath $taskApiHeader -Algorithm SHA256).Hash.ToLowerInvariant()
# Normalize checkout line endings for the SDK pin; retain the raw digest in the build manifest.
$taskSdkPinDigest = [Security.Cryptography.SHA256]::Create()
try {
    $taskApiHeaderLfBytes = [Text.Encoding]::UTF8.GetBytes($taskApiHeaderText.Replace("`r`n", "`n"))
    $taskApiHeaderLfHash = [BitConverter]::ToString($taskSdkPinDigest.ComputeHash($taskApiHeaderLfBytes)).Replace('-', '').ToLowerInvariant()
} finally { $taskSdkPinDigest.Dispose() }
if ($taskSdkLock.sdk_header_sha256_lf -notmatch '^[a-fA-F0-9]{64}$' -or $taskApiHeaderLfHash -ne $taskSdkLock.sdk_header_sha256_lf) {
    throw 'The SDK header digest differs from sdk.lock.json; restore the pinned SDK before building.'
}
if (Test-Path -LiteralPath (Join-Path $taskSdkDirectory '.git')) {
    $taskSdkHead = & git '-C' $taskSdkDirectory 'rev-parse' 'HEAD'
    if ($LASTEXITCODE -ne 0 -or $taskSdkHead -ne $taskSdkLock.sdk_commit) {
        throw 'The SDK checkout differs from sdk.lock.json; restore the pinned SDK commit before building.'
    }
}
$taskCompiler = (Get-Command $ArmCompiler -CommandType Application -ErrorAction Stop).Source
$taskToolDirectory = Split-Path -Parent $taskCompiler
$taskToolPrefix = Join-Path $taskToolDirectory 'arm-none-eabi-'
$taskPython = (Get-Command $Python -CommandType Application -ErrorAction Stop).Source
$taskApiDirectory = Join-Path $taskRoot 'third_party\distingNT_API\include'
$taskBuildDirectory = Join-Path $taskRoot 'build\arm'
$taskDistributionDirectory = Join-Path $taskRoot 'dist'
$taskPluginDirectory = Join-Path $taskDistributionDirectory 'programs\plug-ins'
$taskPlugin = Join-Path $taskPluginDirectory 'multi_burst.o'
$taskArchive = Join-Path $taskDistributionDirectory 'multi_burst_nt.zip'
$taskReport = Join-Path $taskBuildDirectory 'object_report.json'
$taskSources = @('src\engine.cpp', 'src\multi_burst.cpp')

foreach ($taskSource in $taskSources) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskRoot $taskSource) -PathType Leaf)) {
        throw "Missing source: $taskSource"
    }
}
foreach ($taskDirectory in @($taskBuildDirectory, $taskPluginDirectory)) {
    New-Item -ItemType Directory -Path $taskDirectory -Force | Out-Null
}

$taskFlags = @(
    '-std=c++17', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-mthumb',
    '-fno-rtti', '-fno-exceptions', '-fno-threadsafe-statics', '-fPIC', '-Os',
    '-ffunction-sections', '-fdata-sections', '-fno-unwind-tables',
    '-fno-asynchronous-unwind-tables', '-Wall', '-Wextra', '-Werror',
    '-I', (Join-Path $taskRoot 'include'), '-I', $taskApiDirectory
)
$taskObjects = @()
foreach ($taskSource in $taskSources) {
    $taskObject = Join-Path $taskBuildDirectory ([IO.Path]::GetFileNameWithoutExtension($taskSource) + '.o')
    Write-Host "Compile ARM: $taskSource"
    & $taskCompiler @taskFlags '-c' (Join-Path $taskRoot $taskSource) '-o' $taskObject
    if ($LASTEXITCODE -ne 0) { throw "ARM compilation failed: $taskSource ($LASTEXITCODE)" }
    $taskObjects += $taskObject
}

Write-Host 'Link ARM relocatable plug-in'
& $taskCompiler '-mcpu=cortex-m7' '-mfpu=fpv5-d16' '-mfloat-abi=hard' '-mthumb' '-r' '-nostdlib' '-nostartfiles' '-nodefaultlibs' @taskObjects '-o' $taskPlugin
if ($LASTEXITCODE -ne 0) { throw "ARM relocatable link failed ($LASTEXITCODE)" }

& $taskPython (Join-Path $PSScriptRoot 'verify_object.py') $taskPlugin '--tool-prefix' $taskToolPrefix '--sdk-header' (Join-Path $taskApiDirectory 'distingnt\api.h') '--report' $taskReport
if ($LASTEXITCODE -ne 0) { throw "ARM object verification failed ($LASTEXITCODE)" }

# Include only the verified object, with the exact directory separators used on the card.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchiveStream = [IO.File]::Open($taskArchive, [IO.FileMode]::Create, [IO.FileAccess]::Write)
try {
    $taskZip = [IO.Compression.ZipArchive]::new($taskArchiveStream, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($taskZip, $taskPlugin, 'programs/plug-ins/multi_burst.o', [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    } finally { $taskZip.Dispose() }
} finally { $taskArchiveStream.Dispose() }

$taskCheckZip = [IO.Compression.ZipFile]::OpenRead($taskArchive)
try {
    if ($taskCheckZip.Entries.Count -ne 1 -or $taskCheckZip.Entries[0].FullName -ne 'programs/plug-ins/multi_burst.o') {
        throw 'Incorrect archive layout.'
    }
    $taskEntryStream = $taskCheckZip.Entries[0].Open()
    $taskDigest = [Security.Cryptography.SHA256]::Create()
    try {
        $taskArchiveObjectHash = [BitConverter]::ToString($taskDigest.ComputeHash($taskEntryStream)).Replace('-', '').ToLowerInvariant()
    } finally {
        $taskEntryStream.Dispose()
        $taskDigest.Dispose()
    }
} finally { $taskCheckZip.Dispose() }
$taskObjectHash = (Get-FileHash -LiteralPath $taskPlugin -Algorithm SHA256).Hash.ToLowerInvariant()
if ($taskArchiveObjectHash -ne $taskObjectHash) { throw 'Packaged object digest differs from inspected object.' }

$taskCompilerIdentity = & $taskCompiler '--version'
if ($LASTEXITCODE -ne 0) { throw 'Cannot read ARM compiler identity.' }
$taskManifest = [ordered]@{
    compiler = $taskCompilerIdentity[0]
    compiler_path = $taskCompiler
    flags = $taskFlags
    required_api_version = 13
    minimum_firmware = $taskSdkLock.minimum_firmware
    target_firmware = $taskSdkLock.target_firmware
    sdk_commit = $taskSdkLock.sdk_commit
    sdk_header_sha256 = $taskApiHeaderHash
    sdk_header_sha256_lf = $taskApiHeaderLfHash
    object = 'programs/plug-ins/multi_burst.o'
    object_sha256 = $taskObjectHash
    archive_sha256 = (Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    inspection_report = 'build/arm/object_report.json'
    hardware_tested = $false
}
$taskManifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskDistributionDirectory 'build_manifest.json') -Encoding UTF8
Write-Host "Built and inspected: $taskPlugin"
Write-Host "Archive: $taskArchive"
Write-Host "SHA-256: $taskObjectHash"
