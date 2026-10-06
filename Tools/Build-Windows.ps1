#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [ValidateSet('Preflight','Editor','Shipping')][string]$Mode = 'Editor',
    [string]$OutputDirectory = '',
    [string]$PythonExe = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ProjectFile = Join-Path $ProjectRoot 'GarageRush.uproject'
$EngineRoot = [IO.Path]::GetFullPath($EngineRoot)
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$Logs = Join-Path $ProjectRoot "BuildArtifacts\Windows-$Stamp"
New-Item -ItemType Directory -Force -Path $Logs | Out-Null

function Invoke-Checked([string]$File, [string[]]$Arguments, [string]$Log) {
    Write-Host "Running $([IO.Path]::GetFileName($File)); log: $Log"
    & $File @Arguments 2>&1 | Tee-Object -FilePath $Log
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit $LASTEXITCODE. Read $Log. No tested release is being claimed." }
}

try {
    if ($env:OS -ne 'Windows_NT') { throw 'A Windows x64 development host is required; this script is not a Linux cross-compiler.' }
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
    $BuildVersion = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $BuildVersion)) { throw "Unreal installation not found at $EngineRoot. Install UE 5.7.4 through Epic Games Launcher." }
    $Installed = Get-Content -LiteralPath $BuildVersion -Raw | ConvertFrom-Json
    $Lock = Get-Content -LiteralPath (Join-Path $ProjectRoot 'engine-lock.json') -Raw | ConvertFrom-Json
    if ($Installed.MajorVersion -ne $Lock.engine.major -or $Installed.MinorVersion -ne $Lock.engine.minor -or $Installed.PatchVersion -ne $Lock.engine.patch) {
        throw "Engine mismatch: installed $($Installed.MajorVersion).$($Installed.MinorVersion).$($Installed.PatchVersion), required 5.7.4. Do not silently migrate the checkpoint."
    }
    $BuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    $Uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    $Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    foreach ($File in @($BuildBat, $Uat, $Editor)) { if (-not (Test-Path -LiteralPath $File)) { throw "Incomplete engine installation: $File is absent." } }
    $PluginMetadata = @()
    foreach ($Plugin in @('EnhancedInput','ChaosVehiclesPlugin','PythonScriptPlugin','EditorScriptingUtilities')) {
        $Descriptor = @(Get-ChildItem -LiteralPath (Join-Path $EngineRoot 'Engine\Plugins') -Filter "$Plugin.uplugin" -File -Recurse)
        if ($Descriptor.Count -ne 1) { throw "Expected exactly one bundled $Plugin descriptor, found $($Descriptor.Count)." }
        $Data = Get-Content -LiteralPath $Descriptor[0].FullName -Raw | ConvertFrom-Json
        $PluginMetadata += @{ name=$Plugin; descriptor=$Descriptor[0].FullName; metadata=$Data }
    }
    $VsWhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $VsWhere)) { throw 'Visual Studio 2022 is missing. Install Game development with C++, MSVC v143 and Windows SDK.' }
    $VsRoot = & $VsWhere -latest -version '[17.14,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $VsRoot) { throw 'Visual Studio 2022 17.14+ with x64 C++ tools was not found.' }
    $CompilerFolders = @(Get-ChildItem -LiteralPath (Join-Path $VsRoot 'VC\Tools\MSVC') -Directory)
    if (-not ($CompilerFolders | Where-Object { [version]$_.Name -ge [version]'14.44.35214' })) { throw 'Install MSVC 14.44.35214 or a supported newer v143 version. UBT will also enforce its compiler compatibility rules.' }
    $Kits = (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
    $Sdks = @(Get-ChildItem -LiteralPath (Join-Path $Kits 'Include') -Directory | Where-Object { $_.Name -match '^10\.0\.\d+\.\d+$' -and [version]$_.Name -ge [version]'10.0.22621.0' })
    if (-not $Sdks) { throw 'Windows SDK 10.0.22621.0 or newer is required.' }
    @{ build_id=$Stamp; engine=$Installed; plugins=$PluginMetadata; visual_studio=$VsRoot; msvc=@($CompilerFolders.Name); sdks=@($Sdks.Name); mode=$Mode; tested_game=$false } |
        ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $Logs 'preflight.json') -Encoding UTF8
    Write-Host 'Engine and compiler prerequisites found. This is not a game QA pass.'
    if ($Mode -eq 'Preflight') { exit 0 }

    Invoke-Checked $BuildBat @('GarageRushEditor','Win64','Development',"-Project=$ProjectFile",'-WaitMutex','-NoHotReloadFromIDE') (Join-Path $Logs 'editor-build.log')
    if ($Mode -eq 'Editor') { Write-Host 'Editor target compiled. Production assets and gameplay are still required; no Windows game ZIP was produced.'; exit 0 }

    $Manifest = Get-Content -LiteralPath (Join-Path $ProjectRoot 'Assets\release-content.json') -Raw | ConvertFrom-Json
    if ($Manifest.status -ne 'production-ready') { throw 'Shipping blocked: production vehicle/garage/UI/audio assets are absent or unfinished. See Docs\CONTENT-INTEGRATION.md.' }
    foreach ($Check in $Manifest.manual_qa.PSObject.Properties) {
        if ($Check.Value -is [bool] -and -not $Check.Value) { throw "Shipping blocked: manual QA gate $($Check.Name) has no pass evidence." }
    }
    if (-not $Manifest.manual_qa.build_id -or -not $Manifest.manual_qa.report_file) { throw 'Shipping blocked: no actual playthrough build ID/report is recorded.' }
    $QaPath = Join-Path $ProjectRoot $Manifest.manual_qa.report_file
    if (-not (Test-Path -LiteralPath $QaPath)) { throw "Manual QA evidence file not found: $QaPath" }

    # Import the actual installed MSVC environment; this preserves Unicode/space
    # paths and avoids reconstructing INCLUDE/LIB manually.
    Import-Module (Join-Path $VsRoot 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $VsRoot -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
    $CoreExe = Join-Path $Logs 'CoreTests.exe'
    Push-Location $Logs
    try {
        Invoke-Checked 'cl.exe' @('/nologo','/std:c++20','/EHsc','/utf-8','/W4','/WX','/permissive-',
            "/I$(Join-Path $ProjectRoot 'Source\GarageRush\Public')",
            (Join-Path $ProjectRoot 'Source\GarageRush\Private\Core\GarageCore.cpp'),
            (Join-Path $ProjectRoot 'Tests\CoreTests.cpp'),"/Fe:$CoreExe") (Join-Path $Logs 'core-compile.log')
        Invoke-Checked $CoreExe @((Join-Path $Logs 'QA user with spaces')) (Join-Path $Logs 'core-tests.log')
    } finally { Pop-Location }
    $Validator = Join-Path $ProjectRoot 'Tools\editor_validate.py'
    $Receipt = Join-Path $ProjectRoot 'BuildArtifacts\content-validation.json'
    if (Test-Path -LiteralPath $Receipt) { Remove-Item -LiteralPath $Receipt }
    Invoke-Checked $Editor @($ProjectFile,'-run=pythonscript',"-script=`"$Validator`"",'-unattended','-NullRHI','-nosplash','-stdout','-FullStdOutLogOutput') (Join-Path $Logs 'editor-content-validation.log')
    if (-not (Test-Path -LiteralPath $Receipt)) { throw 'Editor did not emit a content validation result. Do not package.' }
    $Validation = Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json
    if (-not $Validation.passed) { throw "Content validation failed: $($Validation.errors -join '; ')" }
    $ManifestHash = (Get-FileHash -LiteralPath (Join-Path $ProjectRoot 'Assets\release-content.json') -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($Validation.manifest_sha256 -ne $ManifestHash) { throw 'Content changed after validation. Run the build again.' }

    if (-not $OutputDirectory) { $OutputDirectory = Join-Path $ProjectRoot "BuildArtifacts\Release-$Stamp" }
    $OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh output directory. Existing release files are never overwritten.' }
    New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
    $Archive = Join-Path $OutputDirectory 'Staged'
    Invoke-Checked $Uat @('BuildCookRun',"-project=$ProjectFile",'-noP4','-platform=Win64','-clientconfig=Shipping','-build','-cook','-stage','-pak','-iostore','-archive',"-archivedirectory=$Archive",'-prereqs','-utf8output','-unattended') (Join-Path $Logs 'shipping-build.log')
    $Candidates = @(Get-ChildItem -LiteralPath $Archive -Filter 'GarageRush.exe' -File -Recurse | Where-Object { $_.DirectoryName -notmatch '\\Binaries\\' })
    if ($Candidates.Count -ne 1) { throw 'Packaged launcher was not found uniquely in the staged folder. Inspect the UAT log.' }
    $GameRoot = $Candidates[0].DirectoryName
    $Cooked = @(Get-ChildItem -LiteralPath $GameRoot -Recurse -File | Where-Object { $_.Extension -in @('.pak','.ucas') })
    if (-not $Cooked) { throw 'Cooked game data is missing. A lone launcher is not a release.' }
    $Prereqs = @(Get-ChildItem -LiteralPath $GameRoot -Recurse -Filter 'UEPrereqSetup_x64.exe' -File)
    if (-not $Prereqs) { throw 'Offline prerequisite installer is absent. Include it before delivery.' }
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'Docs\PLAYER-README.md') -Destination (Join-Path $GameRoot 'README.txt')
    $Files = @(Get-ChildItem -LiteralPath $GameRoot -File -Recurse)
    $Bytes = ($Files | Measure-Object -Property Length -Sum).Sum
    @{ build_id=$Stamp; configuration='Shipping'; platform='Win64'; bytes=$Bytes; packaged=$true; launched=$false; full_qa_passed=$false } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $GameRoot 'build-status.json') -Encoding UTF8
    $Zip = Join-Path $OutputDirectory 'GarageRush-Windows11-x64.zip'
    if (-not $PythonExe) { $PythonExe = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe' }
    if (-not (Test-Path -LiteralPath $PythonExe)) { throw 'A Python 3.9+ executable is needed for Zip64 packaging. Pass -PythonExe with its path. Staged data remains available.' }
    Invoke-Checked $PythonExe @((Join-Path $ProjectRoot 'Tools\package_zip.py'),$GameRoot,$Zip) (Join-Path $Logs 'zip-packaging.log')
    Write-Host "Packaged candidate: $Zip ($Bytes uncompressed bytes). Run clean-folder/offline Windows acceptance now. Packaging alone is not a tested release."
} catch {
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $Logs 'failure.txt') -Encoding UTF8
    Write-Error "GARAGE RUSH build stopped: $($_.Exception.Message) Logs: $Logs" -ErrorAction Continue
    exit 1
}
