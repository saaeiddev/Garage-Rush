# QA report — source checkpoint 0.1.0

Date: 2026-10-06. This report concerns the actual C++ source checkpoint.
**No Unreal/Windows game build was produced or tested.**

## Passed checks

| Check | Actual result |
|---|---|
| GCC C++20 compile, `-Wall -Wextra -Wpedantic -Werror`, optimized `-O2` | Passed |
| Portable test suite | **42 passed, 0 failed** |
| ASan + UBSan, `-O1 -g`, leak checking explicitly disabled | **42 passed, 0 failed**; no ASan/UBSan diagnostic |
| Invalid action atomicity | Failed operations preserve serialized state; no charges/item/reward mutation |
| All eight order lifecycles | Fault, inspection, dependent removal, purchase/fitting, securing, state acceptance and one reward tested |
| One career with all eight orders, followed by a repeat order | Passed; reputation 104 after eight; new order instance on repeat |
| Parts/assembly | 32 hatch, 33 coupe, 32 SUV service IDs; dependency/fitting/fastener safety checked |
| Refunds/economy | Stock/funds checks; unused-purchase-only, one refund, used-part rejection, counter/economy limits |
| Upgrade effects | Core traction/braking/damping/power changed; this is not a Chaos driving measurement |
| Interrupted repair save roundtrip | Exact removed/installed serials, condition, fasteners, inventory, evidence, active order and settings retained |
| Save corruption/truncation | All truncated byte lengths rejected; checksum/schema/semantic failures cannot replace live state |
| Backup and file paths | Real Linux file saves in a folder with Persian characters/spaces; valid backup recovery/preservation |
| Recovery state | Core track recovery preserves mechanical revision, inventory and active order |
| Randomized operations | 5,000 seeded actions; state invariants and failed-transaction atomicity remained valid |
| Catalog export | Compiled C++ exporter produced three specifications, 97 component slots, eight orders, 63 SKUs |
| Python tools and JSON source files | Syntax/parse passed; Engine API behavior not exercised |
| Clean extracted source folder with spaces | Recompiled from the archive with 42 tests passed; host-only source reproducibility, not a Windows game launch |

Raw results are preserved in `core-tests.txt` and `core-asan-ubsan.txt`.
The extracted-source run is preserved in `extracted-source-tests.txt`.

## Failed/blocked diagnostic run

The first default sanitizer run could not run LeakSanitizer: the host denied
inspection of `/proc/<pid>/task` and reported a fatal LeakSanitizer limitation.
The core suite was then rerun with `detect_leaks=0` while ASan and UBSan stayed
enabled. Memory leaks are **unverified**, not passed. The diagnostic is preserved
in `leak-sanitizer-host-blocker.txt`.

Compiler warnings encountered during development were corrected before the
final runs. There are no failed final portable tests. That statement does not
include the unexecuted game tests below.

## Could not be performed

| Requested acceptance | Reason/status |
|---|---|
| UE UHT/UBT compile and Editor run | Unreal Editor/build tools absent; adapter source is uncompiled |
| Windows MSVC compile, cook, Shipping package | No Windows host, MSVC or Windows SDK |
| Windows PowerShell build tooling execution | No PowerShell/Windows tools; source review only |
| First playable repair in real scene | Production vehicle/workshop/UI assets and Blueprint wiring absent |
| Three modeled cars/interiors/V8/underbody closeup review | No production models imported or authored |
| Lift motion, panel/bolt/tool animation, collision/access | No authored level/geometry/animation |
| Chaos driving and repair effect on actual track telemetry | No skeletal vehicle/PhysicsAsset/track assembly or runtime |
| Main menu, Continue, Sandbox, UI, Settings, Credits, Quit, photo mode | Runtime presentation assets/logic not completed |
| Controller menu navigation, Alt-Tab, capture, remapping UX | No actual Engine/windowed playthrough |
| 1080p/1440p/4K, Windows scaling and ultrawide | No rendered game/window |
| Offline clean-folder launch and Unicode Windows user profile | No native Windows packaged candidate |
| Real-time save/load across authored maps and scene transforms | Core state tested; scene ownership/placement integration pending |
| End-to-end manual game playthrough | No playable game available; test automation is not a manual playthrough |
| Actual packaged-game screenshots/video | No game build; none fabricated or substituted |
| Optional Setup EXE | Not produced; no installation tooling workflow completed |
| 30–60 minutes initial playtime | Not measured; UI/world not playable |

## Performance

Actual host: Linux x86-64, kernel 6.18.44, glibc 2.39, GCC 13.3.0.
No Unreal renderer, Windows runtime or exposed NVIDIA profiling utility.
The user's i7/RTX 4060 laptop/16 GB configuration was unavailable.

| Metric | Recorded result |
|---|---|
| Game build ID/configuration | N/A; source checkpoint only |
| Garage/repair/track FPS and frame times | Not measured |
| Resolution, TSR/render scale, graphics preset | Not applicable; no rendering |
| Game RAM/VRAM, streaming/LOD/PSO/shader/traversal stutter | Not measured |
| Reference-machine 1080p High ~60 FPS | Unverified target |
| Windows game build size | N/A; no executable/cooked data |
| Source archive size/integrity | Recorded during final archive creation; not a game-build size |

## Precise remaining work

Compile the written UE source against 5.7.4 on Windows, fixing observed Engine
API/UHT issues. Import verified production assets; author and test the garage,
three vehicles, track, UI, animation, audio and real telemetry. Connect the
portable systems to actual presentation and complete the specified game QA.
Cook/package a native candidate; verify that exact build offline from a clean
folder, profile the requested laptop class, and capture real gameplay evidence.
