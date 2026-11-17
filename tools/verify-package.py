#!/usr/bin/env python3
"""Verify an Vantage package without installing it; all output stays in the project."""
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

project = Path(__file__).resolve().parent.parent
if project in (Path('/'), Path.home()):
    raise SystemExit('Refusing verification outside a project')
package = Path(sys.argv[1]).resolve()
if project not in package.parents or not package.is_file():
    raise SystemExit('Package must be a file inside this project')
manifest = json.loads(Path(str(package) + '.payload.json').read_text())
env = dict(os.environ, TMPDIR=str(project / '.local-cache'))
with tempfile.TemporaryDirectory(prefix='pkg-verify.', dir=package.parent) as temporary:
    expanded = Path(temporary) / 'expanded'
    subprocess.run(['pkgutil', '--expand-full', str(package), str(expanded)], check=True, env=env)
    root = expanded / 'Payload'
    actual = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in root.rglob('*') if p.is_file()}
    if actual != manifest:
        raise SystemExit('Payload differs from the staged SHA-256 manifest')
    info = ET.parse(expanded / 'PackageInfo').getroot()
    if info.get('relocatable') != 'false' or list(info.find('relocate')):
        raise SystemExit('Package unexpectedly allows bundle relocation')
    if (expanded / 'Scripts').exists():
        raise SystemExit('Unexpected installer scripts')
    presets = list(root.rglob('*.orbitpreset'))
    if len(presets) != 24:
        raise SystemExit(f'Expected 24 presets, found {len(presets)}')
    bundles = [root / 'Applications/Vantage.app',
               root / 'Library/Audio/Plug-Ins/Components/Vantage.component',
               root / 'Library/Audio/Plug-Ins/VST3/Vantage.vst3']
    for bundle in bundles:
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True, env=env)
        binary = str(bundle / 'Contents/MacOS/Vantage')
        arches = subprocess.check_output(['lipo', '-archs', binary], text=True).split()
        if set(arches) != {'arm64', 'x86_64'}:
            raise SystemExit(f'Unexpected architectures: {arches}')
        minimums = re.findall(r'\bminos ([0-9.]+)', subprocess.check_output(['otool', '-l', binary], text=True))
        if len(minimums) != 2 or any(value != '11.0' for value in minimums):
            raise SystemExit(f'Unexpected deployment targets: {minimums}')
        metadata = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
        if metadata['CFBundleShortVersionString'] != info.get('version'):
            raise SystemExit(f'Bundle/package version mismatch: {bundle}')
    result = dict(package=str(package.relative_to(project)), version=info.get('version'),
                  sha256=hashlib.sha256(package.read_bytes()).hexdigest(), bytes=package.stat().st_size,
                  files_matched_sha256=len(actual), presets=len(presets), bundles_verified=len(bundles),
                  architectures=['arm64', 'x86_64'], minimum_macos='11.0', relocatable=False,
                  installer_scripts=False, installed=False)
    package.with_suffix('.validation.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
