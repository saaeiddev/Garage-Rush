# State ownership and simplifications

## Portable core

`gr::State` owns every vehicle, installed slot, inventory item, stock count,
money balance, career completion count, active order and preference. Each
component instance has a never-reused serial. Installed items and inventory
are disjoint sets. The global validator enforces identity, fitting, assembly
prerequisites, finite condition values, bounds and exact catalog membership.

Actions validate prerequisites before mutation. A failed normal action never
charges money, consumes an item or changes progression. Purchases and refunds
use integer cents. A new customer order has a new instance ID even when its
order template has been completed before. Rewards are consumed by closing the
active order, so a repeated click cannot claim the same payment twice.

`removeFirst` describes access blockers. `installFirst` describes secured
supporting parts required for fitting. For example, the wheel must be removed
before brake work; pads must be removed before the disc; the disc must be
installed/secured before pads; all support parts must be secured before the
wheel can be reinstalled. An installed assembly cannot reference a removed or
unsecured prerequisite. Lift and engine safety are state constraints.

Fasteners are an integer count of secured attachment operations. Individual
bolt positions, per-bolt rotation and visible tool feedback are **not** in the
core and must be authored on the production service mesh. The count is saved
exactly even when the player interrupts an operation.

Any mechanical change increments the vehicle revision. Verification records
the current revision. Collecting payment requires that exact revision, resolved
faults, inspection evidence, complete assembly and a stopped vehicle at the bay.
Opening/closing a hood or changing location does not erase mechanical identity.

## Diagnostic model

Condition is 0–1. Simplified formulas expose weak starting, charging output,
ignition wear, restricted intake, cooling efficiency, brake wear, tire grip and
damper damage. These are readable game rules, not an engineering-grade scanner,
fluid model, battery discharge simulation or real engine-cycle simulation.

Reference acceleration/braking results are power/traction-based analytic
estimates. They are explicitly named `Reference*` in the Unreal adapter. They
must not be displayed as measured track laps, an actual acceleration run or
real braking telemetry. The current core acceptance protocol tests part state
at the correct location, not simulated track execution. Connect timed gates,
actual speeds and measured distances in the finished Chaos scene before release.

Upgrades alter core grip/braking/damping/torque parameters. The written Chaos
bridge applies per-wheel tire/brake/damper effects and peak torque. It has not
been compiled or validated in the selected Engine. Model-specific mass, tire
dimensions, wheel locations, center of gravity, differential, gear ratios,
torque curves and suspension tuning must be authored and validated separately.

Stock is replenished to its catalog minimum after a completed career order.
Sandbox purchases cost nothing and cannot be refunded for career money.
Reputation is the sum of completed order rewards. This is modest progression;
level-unlocked bay expansion, staff hiring and deep engine teardown are absent.

Current fitting groups are per fictional vehicle and component kind. The four
corners share a tire/wheel/brake/damper family, and the two hoses share a hose
family. This is a deliberate catalog simplification, not an assertion that real
front/rear hardware or upper/lower hoses are interchangeable. After inspecting
the chosen production models, split fitting groups and SKUs where their actual
dimensions, brackets or front/rear assemblies differ; add corresponding fitting
tests before representing the finished game as mechanically accurate.

## Save schema

The file begins with `GRSAVE01`, schema 1, payload length, a deterministic
little-endian payload and a CRC32. Collections and strings are bounded and
every float is semantically validated. CRC32 detects accidental corruption;
it is not cryptographic anti-cheat or an authentication system.

Loading decodes into a temporary state. The live session is replaced only if
the entire payload validates. A corrupt, truncated or unsupported primary
falls back to `.bak`; if both fail, the live session remains intact and the
caller gets a readable error. Schema 1 is versioned; migration from any other
schema is not implemented and must be added explicitly if schema 2 ships.

Saving writes a temporary, flushes it, preserves a previously valid primary as
the recovery backup and atomically replaces the primary. Windows code uses
wide path APIs and `MoveFileExW`; POSIX uses rename/fsync. Windows execution and
power-loss fault injection are untested. A corrupted primary never overwrites
an existing valid backup. The first save has no previous version to back up.

The API is intended for single-threaded session actions. Do not concurrently
write the same slot. The UE adapter autosaves after successful mutations and
exposes `LastSaveError`; an autosave failure must be shown by the final UMG UI.
Synchronous small saves have not been profiled in Unreal. If writing affects
frame time, serialize an immutable snapshot on the game thread and save that
snapshot on a serialized worker queue. Do not mutate live state from that worker.

## Unreal ownership

`UGarageSessionSubsystem` survives level travel and exposes Blueprint-friendly
view models and actions. Windows slot paths are based on the platform's per-user
settings directory: `GarageRush/Saved/Career.grsave` and `Sandbox.grsave`.

`AGaragePartActor` binds a real service mesh to a vehicle/part ID. A traceable
service mount remains when the part is removed, allowing installation at its
mount. Blueprints must connect inspect/remove/fastener/install presentation to
the subsystem. Actor selection is written; actual assembly and animation are
not authored. Avoid double geometry: remove the serviceable part's baked copy
from the base vehicle mesh before adding the independently removable actor.

`AGarageCharacter` uses runtime Enhanced Input contexts. `AGarageVehiclePawn`
defines contextual driving source, two cameras and a Chaos tuning bridge.
Controller menu navigation, focus-loss handling, context cleanup across
possession and all visual interactions still need Engine integration and QA.
The vehicle recovery event must be connected to an actual safe spawn transform;
the core does not teleport a scene actor by itself.

UMG widgets, main-menu scene, photo mode, graphic preset application, audio
mixing, lift motion, cockpit gauges, panel animation, physical diagnostics,
vehicle map placement and saved scene transforms are outstanding presentation
work. Saving their intended settings or core location does not implement them.
