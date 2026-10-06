"""Run INSIDE a compiled Unreal 5.7.4 Editor: create actual Blueprint packages.

Not executed on the checkpoint host. These class assets alone are not finished
vehicles or levels. Existing artist-authored assets are never overwritten.
"""
from pathlib import Path
import unreal


def main():
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets.make_directory('/Game/GarageRush/Blueprints')
    definitions = [
        ('BP_GarageCharacter', '/Script/GarageRush.GarageCharacter', None),
        ('BP_ServicePart', '/Script/GarageRush.GaragePartActor', None),
        ('BP_AsterH4', '/Script/GarageRush.GarageVehiclePawn', 'hatch'),
        ('BP_VektorC8', '/Script/GarageRush.GarageVehiclePawn', 'coupe'),
        ('BP_TerraS6', '/Script/GarageRush.GarageVehiclePawn', 'suv'),
    ]
    for name, parent_path, vehicle_id in definitions:
        path = f'/Game/GarageRush/Blueprints/{name}'
        if assets.does_asset_exist(path):
            unreal.log(f'Preserved existing authored asset: {path}')
            continue
        parent = unreal.load_class(None, parent_path)
        if parent is None:
            raise RuntimeError(f'Compile GarageRushEditor before running bootstrap: missing {parent_path}')
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        blueprint = tools.create_asset(name, '/Game/GarageRush/Blueprints', unreal.Blueprint, factory)
        if blueprint is None:
            raise RuntimeError(f'Editor failed to create {path}')
        if vehicle_id:
            cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
            unreal.get_default_object(cls).set_editor_property('vehicle_id', vehicle_id)
        if not assets.save_loaded_asset(blueprint):
            raise RuntimeError(f'Editor failed to save binary package {path}')
    unreal.log('Real Blueprint class packages created by Editor. Import/calibrate production content and wire presentation next. No game release was generated.')


main()
