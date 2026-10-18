[CmdletBinding()]
param([string]$Compiler = '')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$taskBuildDirectory = Join-Path $taskRoot 'build\native'
New-Item -ItemType Directory -Path $taskBuildDirectory -Force | Out-Null
$taskOriginalPath = $env:PATH

try {
    $taskCandidates = @()
    if ($Compiler) {
        $taskCandidates += (Get-Command $Compiler -CommandType Application -ErrorAction Stop).Source
    } else {
        foreach ($taskName in @('clang++', 'g++')) {
            $taskCommand = Get-Command $taskName -CommandType Application -ErrorAction SilentlyContinue
            if ($taskCommand) { $taskCandidates += $taskCommand.Source }
        }
        foreach ($taskKnownCompiler in @('C:\Program Files\LLVM\bin\clang++.exe', 'C:\msys64\ucrt64\bin\g++.exe', 'C:\msys64\mingw64\bin\g++.exe', 'C:\mingw64\bin\g++.exe')) {
            if (Test-Path -LiteralPath $taskKnownCompiler -PathType Leaf) { $taskCandidates += $taskKnownCompiler }
        }
    }

    $taskChosenCompiler = $null
    $taskProbe = '#include <cstdio>
#include <cmath>
#include <new>
int main() { std::puts("Native C++ runtime available"); return 0; }'
    foreach ($taskCandidate in ($taskCandidates | Select-Object -Unique)) {
        $env:PATH = (Split-Path -Parent $taskCandidate) + [IO.Path]::PathSeparator + $taskOriginalPath
        $taskProbeExecutable = Join-Path $taskBuildDirectory 'toolchain_probe.exe'
        $taskProbe | & $taskCandidate '-std=c++17' '-x' 'c++' '-' '-o' $taskProbeExecutable
        if ($LASTEXITCODE -ne 0) { Write-Host "Compiler or runtime unavailable: $taskCandidate"; continue }
        & $taskProbeExecutable
        if ($LASTEXITCODE -ne 0) { Write-Host "Native runtime failed: $taskCandidate"; continue }
        $taskChosenCompiler = $taskCandidate
        break
    }
    if (-not $taskChosenCompiler) { throw 'No working native clang++/g++ toolchain and C++ runtime found. Specify -Compiler with a usable compiler path.' }
    Write-Host "Native compiler: $taskChosenCompiler"
    & $taskChosenCompiler '--version'
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read native compiler identity.' }

    $taskFlags = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-rtti', '-fno-exceptions', '-I', (Join-Path $taskRoot 'include'), '-I', (Join-Path $taskRoot 'third_party\distingNT_API\include'))
    # clang-tidy uses the exact compiler, includes and language flags exercised here.
    # Production translation units receive individual compile commands; linking
    # the native host stubs remains specific to the test executables below.
    $taskCompileCommands = @()
    foreach ($taskProductionSource in @('src\engine.cpp', 'src\multi_burst.cpp')) {
        $taskProductionSourcePath = Join-Path $taskRoot $taskProductionSource
        $taskCompileCommands += [ordered]@{
            directory = $taskRoot
            file = $taskProductionSourcePath
            arguments = @($taskChosenCompiler) + $taskFlags + @('-c', $taskProductionSourcePath)
        }
    }
    $taskCompileDatabasePath = Join-Path $taskBuildDirectory 'compile_commands.json'
    $taskCompileDatabaseJson = ConvertTo-Json -InputObject $taskCompileCommands -Depth 4
    [IO.File]::WriteAllText($taskCompileDatabasePath, $taskCompileDatabaseJson + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    Write-Host "Native compile database: $taskCompileDatabasePath"
    $taskSuites = @(
        @{ Name = 'engine_tests'; Sources = @('src\engine.cpp', 'tests\engine_tests.cpp') },
        @{ Name = 'plugin_tests'; Sources = @('src\engine.cpp', 'src\multi_burst.cpp', 'tests\plugin_tests.cpp') }
    )
    foreach ($taskSuite in $taskSuites) {
        $taskSuiteSources = @()
        foreach ($taskSource in $taskSuite.Sources) {
            $taskSourcePath = Join-Path $taskRoot $taskSource
            if (-not (Test-Path -LiteralPath $taskSourcePath -PathType Leaf)) { throw "Missing test source: $taskSource" }
            $taskSuiteSources += $taskSourcePath
        }
        $taskExecutable = Join-Path $taskBuildDirectory ($taskSuite.Name + '.exe')
        Write-Host "Compile native: $($taskSuite.Name)"
        & $taskChosenCompiler @taskFlags @taskSuiteSources '-o' $taskExecutable
        if ($LASTEXITCODE -ne 0) { throw "Native compilation failed: $($taskSuite.Name) ($LASTEXITCODE)" }
        & $taskExecutable
        if ($LASTEXITCODE -ne 0) { throw "Native tests failed: $($taskSuite.Name) ($LASTEXITCODE)" }
    }
    Write-Host 'PASS: native engine and plug-in behavior tests'
} finally { $env:PATH = $taskOriginalPath }
