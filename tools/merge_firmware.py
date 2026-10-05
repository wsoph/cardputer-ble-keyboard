"""Make a clean standalone image. No device reads or flashing."""
import argparse
import struct
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', required=True, type=Path)
parser.add_argument('--framework-dir', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
if args.output.exists():
    raise SystemExit('Output already exists; preserving it.')
parts = [(0, args.build_dir / 'bootloader.bin'),
         (0x8000, args.build_dir / 'partitions.bin'),
         (0xE000, args.framework_dir / 'tools/partitions/boot_app0.bin'),
         (0x10000, args.build_dir / 'firmware.bin')]
for i, (offset, path) in enumerate(parts):
    if not path.is_file() or path.stat().st_size == 0:
        raise SystemExit('Missing or empty component: ' + path.name)
    limit = parts[i+1][0] if i+1 < len(parts) else 0x340000
    if offset + path.stat().st_size > limit:
        raise SystemExit('Overlapping/oversized component: ' + path.name)
entries = []
table = (args.build_dir / 'partitions.bin').read_bytes()
for offset in range(0, len(table), 32):
    row = table[offset:offset+32]
    if len(row) != 32 or row[:2] != b'\xaa\x50':
        break
    magic, kind, subtype, address, size, label, flags = struct.unpack('<HBBII16sI', row)
    entries.append((kind, subtype, address, size, label.rstrip(b'\0')))
if (0, 0x10, 0x10000, 0x330000, b'app0') not in entries:
    raise SystemExit('Unexpected standalone app partition.')
if (1, 2, 0x9000, 0x5000, b'nvs') not in entries:
    raise SystemExit('Unexpected NVS partition.')
command = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', 'merge_bin',
           '--output', str(args.output), '--fill-flash-size', '8MB']
for offset, path in parts:
    command += [hex(offset), str(path)]
subprocess.run(command, check=True)
merged = args.output.read_bytes()
if len(merged) != 8388608:
    raise SystemExit('Standalone size is not exactly 8MB.')
for i, (offset, path) in enumerate(parts):
    data = path.read_bytes()
    if merged[offset:offset+len(data)] != data:
        raise SystemExit('Merged component mismatch: ' + path.name)
    end = offset + len(data)
    following = parts[i+1][0] if i+1 < len(parts) else len(merged)
    if merged[end:following] != b'\xff' * (following-end):
        raise SystemExit('Unexpected data outside build components.')
print('Verified 8MB standalone: exact components, blank NVS and unused ranges.')
