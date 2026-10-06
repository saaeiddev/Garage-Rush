# Reproduce the Windows workflow

**These steps have not been executed here.** They describe the next development
environment and commands, not proof that the Unreal source compiles. UHT/UBT
compatibility fixes, real content authoring and runtime QA may still be needed.

## Development prerequisites

1. Windows 11 x64 and Unreal Engine **5.7.4** through the Epic Games Launcher.
   Use the patch recorded in `engine-lock.json`; no Preview build is intended.
2. Visual Studio 2022 **17.14** with Game development with C++, supported MSVC
   v143 tools (Epic's 5.7 documentation specifies 14.44.35214), and Windows SDK
   10.0.22621.0 or newer. UBT remains the final compiler-compatibility check.
3. A compatible GPU and enough disk/RAM for an actual UE content project. The
   user's RTX 4060 laptop/16 GB is the player target, not tested development
   hardware. The final editor requirements must be measured separately.
4. The bundled EnhancedInput and ChaosVehiclesPlugin. PythonScriptPlugin and
   EditorScriptingUtilities are enabled only for the Editor target. Their real
   descriptor versions are recorded in `preflight.json` on the Windows host.
5. Python 3.9+ only for Zip64 packaging if the Engine-bundled Python executable
   is unavailable. The packaged **player** must never need Python or UE Editor.

Extract the source to a writable path. Keep all source directories together.
Do not rename the module or project while diagnosing the first build.

## Preflight and first Editor compile

PowerShell 5.1+ from the extracted project root:

```powershell
.\Tools\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -Mode Preflight
.\Tools\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -Mode Editor
```

The script refuses a different Engine patch and reports absent compiler/SDK
components. It writes timestamped logs and installed plugin metadata. If UHT
or UBT fails, fix the exact source/API issue and rerun Editor compilation;
do not describe Linux core compilation as an Unreal compile pass.

When the Editor target compiles, open `GarageRush.uproject`. Run
`Tools/bootstrap_editor.py` through **Tools > Execute Python Script**. This uses
the real asset factory to create actual Blueprint packages and preserves any
existing artist-authored class assets. It has not run in this checkpoint.
The generated classes contain no production mesh and are not finished content.

Finish the content workflow in `CONTENT-INTEGRATION.md`, including UMG and
actual track measurements. Run the Editor and Development packaged acceptance
before attempting a Shipping release. A first authored repair should prove the
component can be selected, removed, fitted, saved, restored and verified.

## Cook/stage/package a candidate after content exists

Populate `Assets/release-content.json` with real local asset package paths.
Record source rights and actual manual evidence. Leave any unperformed gate
false. The source checkpoint intentionally fails this production gate.

```powershell
.\Tools\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -Mode Shipping -OutputDirectory 'D:\Garage Rush Builds\Candidate-001'
```

If the Engine's standalone Python is not present:

```powershell
.\Tools\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -Mode Shipping -PythonExe 'C:\Python311\python.exe' -OutputDirectory 'D:\Garage Rush Builds\Candidate-002'
```

The script uses the real Engine Build.bat/UAT workflow. It compiles and runs the
core tests with installed MSVC, invokes the actual Editor content validator,
then calls `BuildCookRun` for Win64 Shipping with cooking, staging, pak/IoStore,
archive and prerequisites. It checks for a launcher, cooked data and the offline
UE prerequisite installer, and zips the **whole** staged game folder using Zip64.
The output is a candidate named `GarageRush-Windows11-x64.zip`.

Content validation checks local binary packages, asset class/loading, matching
vehicle IDs, physics asset assignment, service meshes/mounts and recorded
rights. It does not prove photorealism, collision quality, controller UX,
mechanical animations or correct physics. Manual evidence remains mandatory.

Packaging emits `build-status.json` with `launched=false` and
`full_qa_passed=false`. The script cannot claim either pass merely because UAT
succeeds. It does not generate an installer; a setup EXE is optional future work.

## Actual candidate acceptance

1. Extract to a **new** folder including spaces. Also test a real non-English
   Windows user profile and install path. Keep network disabled for gameplay.
2. If runtimes are missing, run the bundled `UEPrereqSetup_x64.exe` from the
   staged Engine prerequisites directory. Test a clean machine afterward.
3. Launch the root `GarageRush.exe`, with no Editor or developer server running.
4. Play every order end to end, inspect the cars/garage close up, drive and return,
   save during a removed-part/partially fastened repair, and resume precisely.
5. Exercise every menu, controller navigation, focus loss/Alt-Tab, recovery,
   remapping, settings, photo mode and Quit. Check the exact creator credit.
6. Test 1080p, 1440p, 4K, common display scales and ultrawide layouts. Record
   actual RTX 4060 laptop GPU power/VRAM, resolution/upscaler, preset, FPS,
   frame-time behavior, RAM/VRAM and shader/traversal stalls. Do not estimate FPS.
7. Preserve logs, build ID/hash, screenshots/recording and a dated manual report.
   Mark passed/failed/unperformed separately. Fix failures before delivery.

Only after this procedure may a packaged candidate be identified as a tested
game release. No step after Linux core tests has been performed here.

## Primary references checked for this checkpoint

- Epic 5.7/hotfix announcement: https://forums.unrealengine.com/t/unreal-engine-5-7-released/2673913
- Epic 5.7 C++ toolchain: https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.7
- Enhanced Input: https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine?application_version=5.7
- Editor Python workflow: https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.7
- Chaos setup overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/how-to-set-up-vehicles-in-unreal-engine

The public Chaos API pages available during review defaulted to 5.8. Setter
signatures were cross-checked against those primary API pages and older official
entries; compatibility with the pinned 5.7.4 headers still requires compilation.
