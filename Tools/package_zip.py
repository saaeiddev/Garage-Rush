#!/usr/bin/env python3
"""Zip64 release packaging: include full staged data, never synthesize an EXE."""
import argparse
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('folder', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    folder = args.folder.resolve()
    output = args.output.resolve()
    if not (folder / 'GarageRush.exe').is_file():
        raise SystemExit('No real packaged GarageRush.exe launcher found.')
    if not any(folder.rglob('*.ucas')) and not any(folder.rglob('*.pak')):
        raise SystemExit('Cooked game data is absent; refusing an incomplete ZIP.')
    if output.exists() or output.is_relative_to(folder):
        raise SystemExit('Use a new ZIP path outside the staged game folder.')
    files = sorted(p for p in folder.rglob('*') if p.is_file())
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as archive:
        for path in files:
            if path.is_symlink():
                raise SystemExit(f'Refusing an unresolved symlink in release: {path.name}')
            archive.write(path, path.relative_to(folder).as_posix())
    with zipfile.ZipFile(output) as archive:
        bad = archive.testzip()
        if bad:
            output.unlink()
            raise SystemExit(f'ZIP integrity failed at {bad}.')
    print(f'Packaged candidate: {output.name}; {len(files)} files; {output.stat().st_size} compressed bytes. Windows launch/QA remains required.')


if __name__ == '__main__':
    main()
