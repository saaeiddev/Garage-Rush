# GARAGE RUSH — source checkpoint 0.1.0

**Created by Amir Saeid Dehghan**

This is an implementation checkpoint, **not a playable Windows game**. The
portable C++ gameplay core was compiled and tested on Linux. Unreal integration
source and Windows build scripts are provided but have not been compiled or run
with Unreal. There is no `GarageRush.exe`, cooked content, photorealistic garage,
modeled car, authored level, installer or gameplay screenshot in this archive.

## What actually exists

| System | Implemented work | Verified here |
|---|---|---|
| Vehicle mechanical state | Three fictional specifications; 32/33/32 service slots; unique persistent item serials, condition, fitting and fasteners | C++ host tests |
| Repair actions | Hood/lift access, correct tools, removal/install dependencies, bolt release and torquing, safe assembly | C++ host tests |
| Career | Eight fault-seeded orders, inspection evidence, verification revision, one payment per instance, repeat orders and reputation | C++ host tests |
| Shop/inventory | 63 compatible standard/performance SKUs, cents-based payments, finite stock, refunds only for unused purchases | C++ host tests |
| Diagnostics/tuning | Measurements derived from installed part state; upgrades alter grip, braking, damping and torque/power parameters | C++ host tests |
| Saves | Version 1 bounded binary format, CRC32, validation before replacing live state, durable temporary write, valid backup recovery | Linux file tests |
| Preferences | Input names, FOV, sensitivity, text scale, separate camera effects and five audio volumes | Serialization tests; no renderer/audio QA |
| Unreal adapter | Blueprint-exposed game-instance subsystem, service mount actors, first-person input source, four-wheel Chaos pawn/cameras | Written; UHT/UBT and runtime **unverified** |
| Engine content workflow | Editor-only Blueprint bootstrap and binary-asset validator; no fabricated Unreal packages | Python syntax only |
| Windows workflow | Engine/toolchain/plugin preflight, Editor compilation, gated Shipping cook/stage, full-data Zip64 packaging | Written; PowerShell execution **unverified** |

Catalogs define mechanical data, not 3D assets. Having 32 service IDs does not
mean 32 parts have been modeled. State-derived track estimates are not recorded
driving results; real telemetry and connected track verification remain missing.

## Concrete blocker and next action

The host is Linux x64 with GCC 13.3.0. Unreal Editor/UBT/UAT, MSVC, Windows SDK,
PowerShell, Wine and a Windows runtime are absent. No licensed production car or
workshop asset library is available in the project. Development cannot advance
to the specified visually finished first bay or an actual Windows build here.

Continue on a Windows x64 development machine with **Unreal Engine 5.7.4** and
**Visual Studio 2022 17.14** plus supported v143 C++ tools and Windows SDK
10.0.22621.0 or newer. Compile the Editor target, acquire/import verified assets,
author the scene/UI/audio and connect the remaining gameplay. Then cook/package
and run the complete Windows acceptance procedure. Scripts do not create the
missing art or turn this checkpoint into a game automatically.

## Run the checks that are available

Python 3 and GCC/Clang on Linux/macOS:

```bash
python3 Tools/test_core.py
python3 Tools/test_core.py --sanitize
```

On hosts where LeakSanitizer cannot inspect `/proc`:

```bash
python3 Tools/test_core.py --sanitize --no-leak-check
```

The latter still runs AddressSanitizer and UndefinedBehaviorSanitizer. It does
**not** verify memory leaks. The release checkpoint's results are in `Docs/QA`:
42 checks passed in the normal and ASan/UBSan configurations, including 5,000
random transactions. Full LeakSanitizer execution was blocked by the host.

Windows core-only checks can run from the x64 Native Tools Command Prompt:

```powershell
python Tools/test_core.py
```

That Windows command has not been executed here. It produces a **test runner**,
not `GarageRush.exe` or a game release.

## Unreal and Windows build

See `Docs/WINDOWS-BUILD.md`. Default build mode compiles the Editor target:

```powershell
.\Tools\Build-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -Mode Editor
```

`engine-lock.json` pins the intended patch. The `.uproject` engine association
is necessarily minor-only; preflight checks `Build.version` for exactly 5.7.4.
Bundled plugin descriptor versions are captured from the actual installation.
No installation-derived descriptor numbers are invented. DLSS is not included.

Shipping is deliberately blocked while `Assets/release-content.json` records
missing production content and failed/unperformed game QA gates. Once real
content and evidence exist, `-Mode Shipping` can produce a packaged **candidate**
with the launcher, cooked data and prerequisite installer. A candidate still
needs a clean extracted-folder playthrough before being called a tested release.

## Layout

- `Source/GarageRush/Public/Core` and `Private/Core`: engine-independent C++.
- Other `Source/GarageRush` files: uncompiled Unreal integration source.
- `Tests`: actual portable tests, not fabricated results.
- `Config`: source configuration with no nonexistent default-level references.
- `Assets/gameplay-catalog.json`: exported from compiled C++ catalogs.
- `Assets/release-content.json`: missing-content requirements and QA gates.
- `Assets/THIRD-PARTY.csv` and `license-register.json`: acquisition/rights status.
- `Tools`: host tests, Editor authoring/validation and Windows packaging.
- `Docs`: architecture, content integration, build instructions and actual QA.

The original requested photorealistic visual direction remains the target.
No blockout geometry has been substituted and described as finished content.
The RTX 4060 laptop 1080p/60 FPS target, 30–60 minute playtime, controller UI,
resolution/scaling layouts and all packaged-game requirements remain unverified.
