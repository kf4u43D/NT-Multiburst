[CmdletBinding()]
param(
    [string]$RackSdkPath = $env:RACK_SDK_PATH,
    [string]$RackDirectory = "$env:ProgramFiles/VCV/Rack2Free",
    [string]$RackPluginsPath = "$env:LOCALAPPDATA/Rack2/plugins-win-x64",
    [string]$Compiler = 'C:/msys64/mingw64/bin/g++.exe',
    [string]$PatchPath,
    [switch]$PresetPack,
    [string]$PackDirectory,
    [int[]]$PackSlots = @(0, 6, 7, 10, 12)
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $RackSdkPath) {
    foreach ($taskSdkCandidate in @(
        (Join-Path $taskRoot 'Rack-SDK'),
        (Join-Path $taskRoot 'third_party/Rack-SDK'),
        (Join-Path $taskRoot 'third_party/nt_emu/vcv-plugin/Rack-SDK')
    )) {
        if ((Test-Path -LiteralPath (Join-Path $taskSdkCandidate 'include/rack.hpp') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $taskSdkCandidate 'libRack.dll.a') -PathType Leaf)) {
            $RackSdkPath = $taskSdkCandidate
            break
        }
    }
}
if (-not $RackSdkPath -or
    -not (Test-Path -LiteralPath (Join-Path $RackSdkPath 'include/rack.hpp') -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $RackSdkPath 'libRack.dll.a') -PathType Leaf)) {
    throw 'Rack SDK not found or incomplete. Pass -RackSdkPath <Rack-SDK directory> or set RACK_SDK_PATH; the directory must contain include/rack.hpp and libRack.dll.a.'
}
$RackSdkPath = [IO.Path]::GetFullPath($RackSdkPath)
$taskTargets = @()
if ($PresetPack) {
    if ($PatchPath) { throw 'Use either -PresetPack or -PatchPath.' }
    if (-not $PackDirectory) { $PackDirectory = Join-Path $taskRoot 'dist/preset-pack/patches' }
    $taskPackFiles = @(Get-ChildItem -LiteralPath $PackDirectory -Filter '*.vcv' -File | Sort-Object Name)
    if ($taskPackFiles.Count -ne 16) { throw 'The exported preset pack must contain exactly 16 Rack patches.' }
    $taskPackBySlot = @{}
    foreach ($taskFile in $taskPackFiles) {
        $taskPackPatch = Get-Content -LiteralPath $taskFile.FullName -Raw | ConvertFrom-Json
        $taskPackNt = @($taskPackPatch.modules | Where-Object { $_.plugin -eq 'nt_emu' })
        if ($taskPackNt.Count -ne 1 -or -not $taskPackNt[0].data.PSObject.Properties['presetPackSlot']) {
            throw "Missing unique preset pack marker: $($taskFile.Name)"
        }
        $taskSlot = [int]$taskPackNt[0].data.presetPackSlot
        if ($taskSlot -lt 0 -or $taskSlot -gt 15 -or $taskPackBySlot.ContainsKey($taskSlot)) {
            throw "Invalid or duplicate preset pack slot: $($taskFile.Name)"
        }
        $taskMatrix = @($taskPackNt[0].data.parameterValues.routingMatrix)
        if ($taskMatrix.Count -ne 256 -or $taskMatrix[167] -ne $taskSlot -or $taskMatrix[168] -ne 0) {
            throw "Incomplete parameter restoration or active pack load action: $($taskFile.Name)"
        }
        # The public host indices are append-only; the entire 169-value scene
        # and its idle load action must be saved in both nt_emu host formats.
        $taskRouting = @($taskPackNt[0].data.routing)
        for ($taskParameter = 0; $taskParameter -lt 169; ++$taskParameter) {
            $taskRoute = @($taskRouting | Where-Object { $_.param -eq $taskParameter })
            if ($taskRoute.Count -ne 1 -or $taskRoute[0].bus -ne $taskMatrix[$taskParameter]) {
                throw "Host routing copies disagree at parameter $taskParameter in $($taskFile.Name)"
            }
        }
        $taskModuleIds = @{}
        foreach ($taskModule in $taskPackPatch.modules) {
            $taskId = [string]$taskModule.id
            if ($taskModuleIds.ContainsKey($taskId)) { throw "Duplicate module ID in $($taskFile.Name)" }
            $taskModuleIds[$taskId] = $taskModule
        }
        $taskCableIds = @{}; $taskDestinations = @{}
        foreach ($taskCable in $taskPackPatch.cables) {
            $taskCableId = [string]$taskCable.id
            $taskDestination = "$($taskCable.inputModuleId):$($taskCable.inputId)"
            if ($taskCableIds.ContainsKey($taskCableId) -or $taskDestinations.ContainsKey($taskDestination) -or
                -not $taskModuleIds.ContainsKey([string]$taskCable.outputModuleId) -or
                -not $taskModuleIds.ContainsKey([string]$taskCable.inputModuleId)) {
                throw "Invalid or duplicate cable in $($taskFile.Name)"
            }
            $taskCableIds[$taskCableId] = $true; $taskDestinations[$taskDestination] = $true
        }
        $taskNtId = $taskPackNt[0].id
        $taskMixAudition = @($taskPackPatch.cables | Where-Object {
            $_.outputModuleId -eq $taskNtId -and $_.outputId -eq 7 -and $_.inputId -eq 0 -and
            $taskModuleIds[[string]$_.inputModuleId].model -eq 'VCA-1'
        })
        $taskStep = @($taskPackPatch.cables | Where-Object {
            $_.inputModuleId -eq $taskNtId -and $_.inputId -eq 6 -and $_.outputId -eq 2 -and
            $taskModuleIds[[string]$_.outputModuleId].model -eq 'Pulses'
        })
        if ($taskMixAudition.Count -ne 1 -or $taskStep.Count -ne 1) {
            throw "Missing physical Mix output 8 audition or Step All input 7: $($taskFile.Name)"
        }
        if (-not (Test-Path -LiteralPath ([string]$taskPackNt[0].data.pluginPath) -PathType Leaf)) {
            throw "Missing pack native DLL: $($taskPackNt[0].data.pluginPath)"
        }
        $taskPackBySlot[$taskSlot] = $taskFile.FullName
    }
    foreach ($taskSlot in ($PackSlots | Select-Object -Unique)) {
        if (-not $taskPackBySlot.ContainsKey($taskSlot)) { throw "Invalid representative pack slot: $taskSlot" }
        $taskTargets += $taskPackBySlot[$taskSlot]
    }
    if (-not $taskTargets.Count) { throw 'Select at least one representative pack slot.' }
    Write-Output 'All 16 exported Rack scenes: saved controls, unique graph, Mix audition and Step All routing PASS'
} else {
    if (-not $PatchPath) { $PatchPath = Join-Path $taskRoot 'patches/Multi_Burst_NT-nt_emu.vcv' }
    $taskTargets += [IO.Path]::GetFullPath($PatchPath)
}
foreach ($taskTarget in $taskTargets) {
    if (-not (Test-Path -LiteralPath $taskTarget -PathType Leaf)) { throw "Missing Rack patch: $taskTarget" }
}
$taskPatch = Get-Content -LiteralPath $taskTargets[0] -Raw | ConvertFrom-Json
$taskNtModules = @($taskPatch.modules | Where-Object { $_.plugin -eq 'nt_emu' })
if ($taskNtModules.Count -ne 1 -or -not $taskNtModules[0].data.pluginPath) {
    throw 'The patch must contain one nt_emu with a native DLL path.'
}
foreach ($taskRequiredFile in @(
    "$RackSdkPath/include/rack.hpp", "$RackSdkPath/libRack.dll.a",
    "$RackDirectory/libRack.dll", "$RackPluginsPath/nt_emu/plugin.dll",
    "$RackPluginsPath/Fundamental/plugin.dll", [string]$taskNtModules[0].data.pluginPath, $Compiler
)) {
    if (-not (Test-Path -LiteralPath $taskRequiredFile -PathType Leaf)) { throw "Missing Rack smoke test dependency: $taskRequiredFile" }
}
$taskMotionProfile = [IO.Path]::GetFileNameWithoutExtension($taskTargets[0]) -like '*motion*'
$taskPackProfile = $PresetPack -or $null -ne $taskNtModules[0].data.PSObject.Properties['presetPackSlot']
$taskDirectory = Join-Path $taskRoot $(if ($taskPackProfile) { 'build/rack-pack-smoke' } elseif ($taskMotionProfile) { 'build/rack-motion-smoke' } else { 'build/rack-smoke' })
New-Item -ItemType Directory -Path $taskDirectory -Force | Out-Null
$taskExe = Join-Path $taskDirectory 'rack_patch_smoke.exe'
$taskPreviousPath = $env:PATH
Push-Location $taskRoot
try {
    $env:PATH = "$(Split-Path -Parent $Compiler);$RackDirectory;$taskPreviousPath"
    & $Compiler '-std=gnu++17' '-O2' '-Wall' '-Wextra' `
        '-isystem' "$RackSdkPath/include" '-isystem' "$RackSdkPath/dep/include" `
        '-I' (Join-Path $taskRoot 'include') 'tests/rack_patch_smoke.cpp' `
        "-L$RackSdkPath" '-lRack' '-o' $taskExe `
        '-static-libgcc' '-static-libstdc++' '-Wl,-Bstatic' '-lwinpthread' '-Wl,-Bdynamic'
    if ($LASTEXITCODE -ne 0) { throw 'Rack smoke test compilation failed.' }
    # Resolve installed plugin DLLs against Rack's matching runtime first.
    $env:PATH = "$RackDirectory;$(Split-Path -Parent $Compiler);$taskPreviousPath"
    if ($PresetPack) {
        foreach ($taskFile in $taskPackFiles) {
            $taskLayoutResult = [IO.Path]::GetFileNameWithoutExtension($taskFile.Name) + '-layout.txt'
            & $taskExe $taskDirectory $RackDirectory $RackPluginsPath $taskFile.FullName '--layout-only' 2>&1 |
                Tee-Object -FilePath (Join-Path $taskDirectory $taskLayoutResult)
            if ($LASTEXITCODE -ne 0) { throw "Installed Rack layout/state check failed for $($taskFile.Name). See $taskDirectory." }
        }
    }
    foreach ($taskTarget in $taskTargets) {
        $taskResult = if ($taskPackProfile) { [IO.Path]::GetFileNameWithoutExtension($taskTarget) + '-result.txt' } else { 'result.txt' }
        & $taskExe $taskDirectory $RackDirectory $RackPluginsPath $taskTarget 2>&1 |
            Tee-Object -FilePath (Join-Path $taskDirectory $taskResult)
        if ($LASTEXITCODE -ne 0) { throw "Real Rack/nt_emu smoke test failed for $taskTarget. See $taskDirectory." }
    }
    Write-Output "Real Rack/nt_emu smoke test passed: $taskDirectory"
} finally {
    $env:PATH = $taskPreviousPath
    Pop-Location
}
