# GARAGE RUSH — packaged candidate instructions

This file is copied into a future real Windows candidate by the build script.
It does not mean such a candidate exists in the source checkpoint.

Extract the complete game ZIP into a writable folder and launch the root
GarageRush.exe. Keep its Engine and cooked game data folders together. If the
runtime is missing, run the bundled UEPrereqSetup_x64.exe from the Engine
prerequisites folder. Unreal Editor and an online account are not player
requirements. Offline clean-machine launch must be verified for the final build.

Intended controls: WASD/mouse in the garage; E interact; left click tool action;
Tab job board/inventory; Esc pause. Driving: WASD throttle/steering/brake;
Space handbrake; C camera; R recovery. Xbox mappings must be displayed in-game.
Remapped input prompts override this default list.

Career accepts customer orders, diagnoses faults, replaces compatible parts,
secures assemblies, verifies repairs and earns payment/reputation. Sandbox
allows free purchases. These systems exist in C++ source; the actual interface
must be connected before a finished game can expose them to a player.

Per-user saves are intended at the Windows platform settings folder under
GarageRush/Saved. Keep the .bak files for recovery. Manual save and autosave
must be exercised in the final build. Do not edit a save while the game runs.

Reference player target: Windows 11 x64, Intel i7, RTX 4060 laptop GPU and 16 GB
RAM. 1080p/60 FPS is a target, not a verified requirement or measured result.
Low/Medium/High/Ultra/Custom presets must be applied and profiled in the final
build. Actual verified hardware/build size are recorded in the final QA report.

Created by Amir Saeid Dehghan
