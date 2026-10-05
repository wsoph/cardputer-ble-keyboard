"""Validate and package build artifacts. No device access or publication."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from public_source import public_files

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', required=True, type=Path)
parser.add_argument('--framework-dir', required=True, type=Path)
parser.add_argument('--package-root', required=True, type=Path)
parser.add_argument('--cover', type=Path)
args = parser.parse_args()
try:
    import esptool
except ModuleNotFoundError:
    raise SystemExit('Missing esptool. Install requirements.txt before packaging.')
root = Path(__file__).resolve().parents[1]
match = re.search(r'CARDKEY_VERSION\s+"(\d+\.\d+\.\d+)"', (root / 'include/version.h').read_text())
if not match:
    raise SystemExit('Firmware version missing.')
version = match[1]
image = args.build_dir / 'firmware.bin'
source = public_files(root)
compiled = list((root / 'src').glob('*')) + list((root / 'include').glob('*')) + [root / 'platformio.ini']
if any(path.stat().st_mtime_ns > image.stat().st_mtime_ns for path in compiled if path.is_file()):
    raise SystemExit('Build is older than a firmware source.')
for name in ('firmware.bin', 'bootloader.bin'):
    check = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32s3',
                            'image_info', '--version', '2', str(args.build_dir / name)],
                           capture_output=True, text=True, check=True)
    if not re.search(r'Validation Hash:.*\(valid\)', check.stdout, re.IGNORECASE):
        raise SystemExit('Invalid ESP32-S3 image: ' + name)
destination = args.package_root.resolve() / ('cardputer-keyboard-' + version)
if destination.exists():
    raise SystemExit('Versioned package exists; preserve it.')
destination.mkdir(parents=True)
app_name = f'cardputer-keyboard-{version}-app.bin'
standalone_name = f'cardputer-keyboard-{version}-standalone-8mb.bin'
shutil.copy2(image, destination / app_name)
for name in ('firmware.elf', 'firmware.map', 'partitions.bin', 'bootloader.bin'):
    shutil.copy2(args.build_dir / name, destination / name)
subprocess.run([sys.executable, str(root / 'tools/merge_firmware.py'),
                '--build-dir', str(args.build_dir), '--framework-dir', str(args.framework_dir),
                '--output', str(destination / standalone_name)], check=True)
for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md'):
    shutil.copy2(root / name, destination / name)
shutil.copy2(root / 'README.md', destination / 'guide.md')
shutil.copytree(root / 'LICENSES', destination / 'LICENSES')
shutil.copytree(root / 'release', destination / 'm5burner')
if args.cover:
    shutil.copy2(args.cover, destination / 'm5burner/cover.png')
archive_name = f'cardputer-keyboard-{version}-source.zip'
with zipfile.ZipFile(destination / archive_name, 'x', zipfile.ZIP_DEFLATED) as archive:
    for path in source:
        archive.write(path, f'cardputer-ble-keyboard-{version}/' + path.relative_to(root).as_posix())
files = {path.relative_to(destination).as_posix(): {'bytes': path.stat().st_size, 'sha256': sha(path)}
         for path in sorted(destination.rglob('*')) if path.is_file()}
manifest = {
    'version': version, 'created_utc': datetime.now(timezone.utc).isoformat(),
    'target': 'M5Stack Cardputer-ADV / ESP32-S3', 'image_validated': True,
    'device_tested': False, 'standalone_device_tested': False, 'credentials_embedded': False,
    'keyboard_native_checks': 214,
    'installation': {'launcher_app': app_name, 'standalone': standalone_name,
                     'standalone_address': '0x0000', 'standalone_flash_bytes': 8388608,
                     'standalone_nvs_blank': True},
    'files': files, 'sources': {path.relative_to(root).as_posix(): sha(path) for path in source},
    'dependencies': {'espressif32': '6.12.0', 'arduino_esp32': '2.0.17',
        'M5Cardputer': '2d4fa6646e4e5b47e0af96214b003aa7b15b8d81',
        'M5Unified': 'e126f900d74fc4f759a1def65ebd84a4c6451b76', 'M5GFX': '0.2.32', 'IRremote': '4.4.1'},
}
(destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
checksums = files | {'manifest.json': {'sha256': sha(destination / 'manifest.json')}}
(destination / 'SHA256SUMS.txt').write_text(''.join(
    record['sha256'] + '  ' + name + '\n' for name, record in sorted(checksums.items())), encoding='utf-8')
for name, record in files.items():
    path = destination / name
    if path.stat().st_size != record['bytes'] or sha(path) != record['sha256']:
        raise SystemExit('Package readback mismatch: ' + name)
print('Package verified:', destination)
print('App bytes:', image.stat().st_size)
print('App SHA256:', files[app_name]['sha256'])
print('Standalone SHA256:', files[standalone_name]['sha256'])
