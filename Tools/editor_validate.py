"""Editor-only validation. Syntax checked here; Unreal execution is unverified.

Invoked by Build-Windows.ps1 through the real PythonScript commandlet. Always
writes a receipt, so a zero process exit can never substitute for a gate pass.
"""
import hashlib
import json
from pathlib import Path
import re
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'Assets/release-content.json'
REPORT = ROOT / 'BuildArtifacts/content-validation.json'


def main():
    errors = []
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8-sig'))
    catalog = json.loads((ROOT / 'Assets/gameplay-catalog.json').read_text(encoding='utf-8'))
    register = json.loads((ROOT / 'Assets/license-register.json').read_text(encoding='utf-8'))
    licenses = {entry['id']: entry for entry in register['entries']}
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

    def require_asset(label, path, expected):
        if not isinstance(path, str) or not re.fullmatch(r'/Game/[A-Za-z0-9_/]+', path):
            errors.append(f'{label}: assign a real /Game package path.')
            return None
        local = ROOT / 'Content' / (path[6:] + ('.umap' if expected == unreal.World else '.uasset'))
        if not local.is_file():
            errors.append(f'{label}: real local Unreal binary package not found.')
            return None
        with local.open('rb') as package:
            header = package.read(4)
        if header != bytes.fromhex('c1832a9e'):
            errors.append(f'{label}: real local Unreal binary package not found.')
            return None
        loaded = assets.load_asset(path)
        if loaded is None or not isinstance(loaded, expected):
            errors.append(f'{label}: wrong asset class or load failed.')
            return None
        return loaded

    if manifest['status'] != 'production-ready':
        errors.append('The manifest is explicitly blocked; production content is incomplete.')
    for label, path in manifest['maps'].items():
        require_asset('map.' + label, path, unreal.World)
    for label, path in manifest['ui'].items():
        require_asset('ui.' + label, path, unreal.WidgetBlueprint)
    for label, path in manifest['audio'].items():
        require_asset('audio.' + label, path, unreal.SoundBase)
    for vehicle in catalog['vehicles']:
        v = manifest['vehicles'][vehicle['id']]
        prefix = vehicle['id'] + '.'
        blueprint = require_asset(prefix + 'blueprint', v['blueprint'], unreal.Blueprint)
        mesh = require_asset(prefix + 'skeletal_mesh', v['skeletal_mesh'], unreal.SkeletalMesh)
        physics = require_asset(prefix + 'physics_asset', v['physics_asset'], unreal.PhysicsAsset)
        require_asset(prefix + 'animation_blueprint', v['animation_blueprint'], unreal.AnimBlueprint)
        for axle in ('wheel_front', 'wheel_rear'):
            require_asset(prefix + axle, v[axle], unreal.Blueprint)
        required_ids = {p['id'] for p in vehicle['parts']}
        if set(v['service_meshes']) != required_ids:
            errors.append(prefix + 'service_meshes: provide every stable component ID from gameplay-catalog.json.')
        for part in required_ids:
            require_asset(prefix + part, v['service_meshes'].get(part), unreal.StaticMesh)
        license_record = licenses.get(v['license_id'])
        if not license_record or not license_record.get('packaged_game_permitted') or not license_record.get('rights_verified'):
            errors.append(prefix + 'model rights have not been documented/verified.')
        if mesh and physics and mesh.get_editor_property('physics_asset') != physics:
            errors.append(prefix + 'skeletal mesh is not assigned its validated PhysicsAsset.')
        if blueprint and mesh:
            cls = unreal.EditorAssetLibrary.load_blueprint_class(v['blueprint'])
            defaults = unreal.get_default_object(cls)
            if str(defaults.get_editor_property('vehicle_id')) != vehicle['id']:
                errors.append(prefix + 'Blueprint vehicle ID does not match persistent state.')
            if defaults.get_mesh().get_skeletal_mesh_asset() != mesh:
                errors.append(prefix + 'vehicle Blueprint does not use the declared production mesh.')
    if not errors:
        level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not level.load_level(manifest['maps']['garage']):
            errors.append('Garage level failed to load.')
        else:
            actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            actors = actor_system.get_all_level_actors()
            mounts = set()
            for actor in actors:
                if isinstance(actor, unreal.GaragePartActor):
                    key = (str(actor.get_editor_property('vehicle_id')), str(actor.get_editor_property('part_id')))
                    if key in mounts:
                        errors.append(f'Duplicate service mount: {key}')
                    mounts.add(key)
                    if actor.get_editor_property('part_mesh').get_static_mesh() is None:
                        errors.append(f'Missing visible part mesh: {key}')
            for vehicle in catalog['vehicles']:
                for part in vehicle['parts']:
                    if (vehicle['id'], part['id']) not in mounts:
                        errors.append(f'Missing authored service mount: {vehicle["id"]}/{part["id"]}')
    if not errors:
        # Only assign map references after actual local assets loaded correctly.
        engine_ini = ROOT / 'Config/DefaultEngine.ini'
        section = '\n[/Script/EngineSettings.GameMapsSettings]\n'
        text = engine_ini.read_text(encoding='utf-8')
        if '[/Script/EngineSettings.GameMapsSettings]' in text:
            import configparser
            config = configparser.ConfigParser(strict=False)
            config.read_string(text)
            assigned = config.get('/Script/EngineSettings.GameMapsSettings', 'GameDefaultMap', fallback='')
            if assigned.split('.', 1)[0] != manifest['maps']['main_menu']:
                errors.append('Default map differs from the validated main menu; set it in Project Settings.')
        else:
            engine_ini.write_text(text + section + 'GameDefaultMap=' + manifest['maps']['main_menu'] + '\nEditorStartupMap=' + manifest['maps']['garage'] + '\n', encoding='utf-8')
    return {'passed': not errors, 'errors': errors, 'manifest_sha256': hashlib.sha256(MANIFEST.read_bytes()).hexdigest(), 'gameplay_qa': 'manual evidence required; asset existence does not prove visual/gameplay quality'}


try:
    result = main()
except Exception:
    result = {'passed': False, 'errors': [traceback.format_exc()], 'manifest_sha256': hashlib.sha256(MANIFEST.read_bytes()).hexdigest()}
REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(result, indent=2), encoding='utf-8')
for error in result['errors']:
    unreal.log_error(error)
unreal.log('Content gate: ' + ('PASS' if result['passed'] else 'FAIL'))
