# Finish production content in Unreal

This is work still to do, not a description of existing assets. Preserve the
requested premium photorealistic direction; a procedural primitive car/garage
does not satisfy the visual acceptance gate.

## First vertical slice

Before attempting the whole scene, author one finished service bay and Aster H4.
Acquire legally shippable assets without buying anything without the user's
approval. Record the exact asset source, creator, license, attribution,
packaged-use rights and editable-source redistribution rights. No assets have
been acquired in this checkpoint; no purchase is needed to use the source code.
Do not mark an imported vehicle as an original-source asset in the register.

Inspect exterior and interior at real scale, engine bay, underbody, UV/materials,
panel thickness/gaps, separated components, pivots, collisions and all service
positions. A body shell with no interior or independent mechanical parts is
not acceptable. Supplement missing production parts to the same quality.

Import through the Editor's actual FBX/glTF/asset-package workflow as applicable
to the model and installed plugins. Do not fabricate `.uasset` or `.umap` files
or rely on a remote asset URL at game runtime. The checkpoint's bootstrap only
creates class Blueprints; it does not import geometry.

Create a skeletal chassis/wheel rig, proper PhysicsAsset and vehicle AnimBP.
Use verified wheel bone names and authored wheel classes with correct radius,
width, steering/drive/handbrake roles, travel, spring and damping. The base C++
wheel values are provisional hatchback defaults, not three calibrated vehicles.
Use an I4 hatchback, V8 coupe and appropriate I6 SUV arrangement. The coupe needs
two visible cylinder banks, covers, intake, belts and exhaust routing.

## Service binding

`Assets/gameplay-catalog.json` is exported from the C++ source. It contains all
32/33/32 stable part IDs, fitting SKUs, access dependencies and order faults.
Place independently rendered `AGaragePartActor` Blueprint instances attached to
the chassis at correct local pivots. Assign `VehicleId` and `PartId`. Remove
duplicated baked copies of those parts from the base chassis geometry.
Set a correctly shaped/pivoted real part mesh and a small traceable mount region.

Connect `OnInteract`/`OnToolAction` to the session's inspection, fastener,
removal and installation actions. Use one animation completion/commit point per
operation. Repeated requests must go through the core transaction checks. Show
the returned explanation when access, fitting, tool, fastener or safety checks
fail. Actor removal preserves a traceable mount for replacement installation.

Author visible bolt positions/tool rotations and inspection camera focus. These
are not implemented by an integer fastener count. Assist mode should label the
actual part and suggest valid next actions; it must be disableable. Show
condition numerically/textually as well as with color.

Wire real lift geometry/motion to `Lift`; confirm the parked chassis is in the
correct bay and the lifting area is clear before committing the state change.
Disable chassis simulation while properly attached to the moving lift, restore
it safely when lowered, and validate player/underbody access and collisions.
The core has lift safety flags; no moving lift scene exists yet.

The guided first order must connect battery symptoms, multimeter inspection,
bolt actions, replacement purchase/install, safe start verification, payment and
an interrupted save/load. Do not proceed to remaining art before this works in
an actual Unreal playthrough and Development packaged candidate.

## World, track, UI and audio

Author two bays, realistic tools/cabinets/shelves, diagnostic station, counter,
office/job board, doors and yard. Use warm daylight, readable practical lights,
neutral materials and restrained orange details. Build the acceleration/braking
lane, handling loop, slalom/skidpad, barriers and safe return/recovery transforms.

Use real material masters/instances for paint/clear coat, metal, rubber, leather,
glass, concrete and wear. Allocate close-up texture resolution according to
view distance and measured VRAM. Nanite is appropriate only where supported;
the skeletal vehicles and movable/translucent components need separate review.
Proposed Lumen/TSR/VSM configuration is unprofiled. Author and test a lower-cost
lighting/reflection preset; optional ray tracing/DLSS are not implemented.

Set distinct chassis mass/center of gravity, wheel positions/radius, differential,
torque curves/gears and suspension for each car. Validate stationary stability,
normal launches, grip, braking and camera clipping. Connect actual lap/test
telemetry to order verification; analytic estimates must remain labeled as
estimates. Transfer the same session state across maps and restore scene
placement rather than constructing a new vehicle state during travel.

Author UMG main menu, board, shop, inventory, diagnostics, repair/context tools,
settings, pause, completion summary, credits and photo mode. Apply the orange,
white and charcoal palette with readable industrial angular buttons and a
conventional body font. Main menu and Credits must both display exactly:

**Created by Amir Saeid Dehghan**

Connect graphics/audio/preferences to actual Engine systems. Persisted controls
must be converted to valid Engine keys and shown in context prompts. Refresh
active pawn mappings after a remap. Complete possession context cleanup,
controller navigation, focus loss, mouse capture and text/scaling layouts.
Author distinct vehicle engine audio and real tool/lift/workshop effects with
five independent volume controls. No audio, menu or photo mode asset exists yet.

## Gate and evidence

Only after authoring real packages, fill `Assets/release-content.json` with
local `/Game/...` package paths and verified model license IDs. Supply exactly
the service part IDs in the exported catalog. Editor validation loads packages,
checks binary package headers/types, matching vehicle state IDs, PhysicsAsset
assignment and service mount completeness. It cannot judge visual quality.

Keep manual gates false until actually tested. Record a Development/package
build ID and detailed report, then build a Shipping candidate. Perform the
clean-folder/offline tests on that exact Shipping candidate and record its own
results before release. No screenshots should be substituted from an image
generator or an unrelated automotive render.
