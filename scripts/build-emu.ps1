[CmdletBinding()]
param([string]$Cxx = 'clang++')

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$taskBuild = Join-Path $taskRoot 'build/nt-emu'
$taskOutput = Join-Path $taskRoot 'dist/nt_emu'
$taskCompiler = (Get-Command $Cxx -CommandType Application -ErrorAction Stop).Source
if ($taskCompiler -notmatch 'clang\+\+(\.exe)?$') {
    throw 'Use clang++ targeting the Windows x64 MSVC toolchain.'
}

function Assert-TaskDirectory([string]$Path) {
    $taskResolved = [IO.Path]::GetFullPath($Path)
    $taskPrefix = $taskRoot.TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
    if (-not $taskResolved.StartsWith($taskPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Build path outside the repository: $taskResolved"
    }
    $taskAncestor = $taskResolved
    while ($taskAncestor.Length -gt $taskRoot.Length) {
        if (Test-Path -LiteralPath $taskAncestor) {
            $taskItem = Get-Item -LiteralPath $taskAncestor -Force
            if (-not $taskItem.PSIsContainer -or ($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw "Build paths must be ordinary directories: $taskAncestor"
            }
        }
        $taskAncestor = Split-Path -Parent $taskAncestor
    }
}
Assert-TaskDirectory $taskBuild
Assert-TaskDirectory $taskOutput
$taskStage = Join-Path $taskBuild ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $taskStage, $taskOutput | Out-Null
$taskCompat = Join-Path $taskRoot 'emu/multi_burst_compat.h'
$taskCommon = @(
    '--target=x86_64-pc-windows-msvc', '-std=c++17', '-O2', '-Wall', '-Wextra',
    '-Werror', '-fno-rtti', '-fno-exceptions',
    '-I', (Join-Path $taskRoot 'include'),
    '-I', (Join-Path $taskRoot 'third_party/distingNT_API/include')
)
$taskObjects = @()
foreach ($taskSource in @('src/engine.cpp', 'src/multi_burst.cpp')) {
    $taskObject = Join-Path $taskStage ([IO.Path]::GetFileNameWithoutExtension($taskSource) + '.obj')
    & $taskCompiler @taskCommon '-include' $taskCompat '-c' (Join-Path $taskRoot $taskSource) '-o' $taskObject
    if ($LASTEXITCODE -ne 0) { throw "Native compilation failed: $taskSource" }
    $taskObjects += $taskObject
}
$taskBridge = Join-Path $taskStage 'nt_emu_bridge.obj'
& $taskCompiler @taskCommon '-Wno-dll-attribute-on-redeclaration' '-c' (Join-Path $taskRoot 'emu/nt_emu_bridge.cpp') '-o' $taskBridge
if ($LASTEXITCODE -ne 0) { throw 'Native host bridge compilation failed.' }
$taskPending = Join-Path $taskStage 'multi_burst.dll'
& $taskCompiler '--target=x86_64-pc-windows-msvc' '-shared' @taskObjects $taskBridge '-lpsapi' '-o' $taskPending
if ($LASTEXITCODE -ne 0) { throw 'Native Windows DLL link failed.' }
$taskDll = Join-Path $taskOutput 'multi_burst.dll'
if (Test-Path -LiteralPath $taskDll) {
    $taskItem = Get-Item -LiteralPath $taskDll -Force
    if ($taskItem.PSIsContainer -or ($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Native output must be a regular file: $taskDll"
    }
}
Move-Item -LiteralPath $taskPending -Destination $taskDll -Force
Write-Output "Built $taskDll (Windows x64, nt_emu API 8+, 28 buses; firmware source unchanged)."
