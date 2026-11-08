"""Keep only the app's architecture in copied universal SDK binaries."""
from pathlib import Path
import subprocess
import sys

bundle = Path(sys.argv[1]).resolve()
main = bundle / 'Contents/MacOS/PetDesk'
architecture = subprocess.check_output(['lipo', '-archs', str(main)], text=True).split()
if len(architecture) != 1:
    raise SystemExit('The macOS package expects one selected application architecture.')
seen = set()
count = 0
for item in bundle.rglob('*'):
    item = item.resolve()
    if not item.is_file() or item in seen:
        continue
    seen.add(item)
    with item.open('rb') as stream:
        magic = stream.read(4)
    if magic not in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        continue
    slices = subprocess.check_output(['lipo', '-archs', str(item)], text=True).split()
    if architecture[0] not in slices:
        raise SystemExit(f'{item.name} does not contain {architecture[0]}')
    if len(slices) > 1:
        temporary = item.with_name(item.name + '.thin')
        subprocess.run(['lipo', str(item), '-thin', architecture[0], '-output', str(temporary)], check=True)
        temporary.chmod(item.stat().st_mode)
        temporary.replace(item)
        count += 1
print(f'Kept {architecture[0]} in {count} universal SDK binaries.')
