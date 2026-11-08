"""Reject external SDK runtime dependencies in a deployed macOS app."""
import subprocess
import sys
import configparser
from pathlib import Path

bundle = Path(sys.argv[1]).resolve()
configuration = configparser.ConfigParser()
configuration.read(bundle / 'Contents/Resources/qt.conf')
if configuration.get('Paths', 'Plugins', fallback='') != 'PlugIns' or configuration.get('Paths', 'Prefix', fallback='') != '.':
    raise SystemExit('Bundle must restrict Qt plugin discovery to Contents/PlugIns through qt.conf.')
seen, failures = set(), []
for candidate in bundle.rglob('*'):
    path = candidate.resolve()
    if not path.is_file() or path in seen:
        continue
    with path.open('rb') as file:
        magic = file.read(4)
    if magic not in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe'):
        continue
    seen.add(path)
    own = subprocess.check_output(['otool', '-D', str(path)], text=True).splitlines()[1:]
    own = {line.strip() for line in own}
    linked = subprocess.check_output(['otool', '-L', str(path)], text=True)
    for line in linked.splitlines()[1:]:
        library = line.strip().split(' (compatibility version', 1)[0]
        if library in own:  # LC_ID_DYLIB names do not load another library.
            continue
        if library.startswith('/') and not library.startswith(('/System/Library/', '/usr/lib/')):
            failures.append(f'{path.relative_to(bundle)}: {library}')
if failures:
    raise SystemExit('\n'.join(failures))
print(f'Bundle dependency check passed: {len(seen)} Mach-O files, no external SDK libraries.')
