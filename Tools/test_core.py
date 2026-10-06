#!/usr/bin/env python3
"""Compile/run the actual C++ core checks. GCC/Clang on POSIX or MSVC on Windows."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--no-leak-check', action='store_true', help='For hosts where LeakSanitizer cannot inspect /proc; ASan/UBSan remain enabled.')
    parser.add_argument('--compiler')
    args = parser.parse_args()
    out = ROOT / 'BuildArtifacts' / ('Core-Sanitized' if args.sanitize else 'Core')
    out.mkdir(parents=True, exist_ok=True)
    compiler = args.compiler or os.environ.get('CXX') or ('cl' if os.name == 'nt' else 'g++')
    if not shutil.which(compiler):
        raise SystemExit(f'Missing C++ compiler: {compiler}. For MSVC run in x64 Native Tools Command Prompt.')
    exe = out / ('CoreTests.exe' if os.name == 'nt' else 'CoreTests')
    sources = [ROOT / 'Source/GarageRush/Private/Core/GarageCore.cpp', ROOT / 'Tests/CoreTests.cpp']
    if Path(compiler).stem.lower() == 'cl':
        if args.sanitize:
            raise SystemExit('This sanitizer runner is for GCC/Clang. Windows core checks use /W4 /WX /EHsc.')
        cmd = [compiler, '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/W4', '/WX', '/permissive-',
               '/I' + str(ROOT / 'Source/GarageRush/Public'), *map(str, sources), '/Fe:' + str(exe)]
    else:
        flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
        cmd = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror', *flags,
               '-I', str(ROOT / 'Source/GarageRush/Public'), *map(str, sources), '-o', str(exe)]
    print('Compiling C++ core checks:', compiler, flush=True)
    subprocess.run(cmd, cwd=out, check=True)
    env = os.environ.copy()
    if args.no_leak_check:
        env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
    result = subprocess.run([str(exe), str(out / 'QA user with spaces')], cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / 'test-results.txt').write_text(result.stdout, encoding='utf-8')
    print(result.stdout, end='')
    return result.returncode


if __name__ == '__main__':
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(f'C++ compilation failed (exit {exc.returncode}). No Unreal or Windows game build was produced.')
