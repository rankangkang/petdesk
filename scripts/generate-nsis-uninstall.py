#!/usr/bin/env python3
"""Generate an uninstaller for installed files without recursively deleting user data."""
import os
import pathlib
import re
import sys

stage = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
safe_name = re.compile(r"^[A-Za-z0-9_.+ -]+$")
files, directories = [], []
for current, names, filenames in os.walk(stage, topdown=False, followlinks=False):
    directory = pathlib.Path(current)
    for name in names + filenames:
        path = directory / name
        if path.is_symlink() or not safe_name.fullmatch(name) or name in (".", ".."):
            raise SystemExit(f"Unsupported installer path: {path}")
    for name in filenames:
        files.append((directory / name).relative_to(stage))
    if directory != stage:
        directories.append(directory.relative_to(stage))

with output.open("w", encoding="utf-8", newline="\n") as handle:
    for path in sorted(files):
        handle.write(f'Delete "$INSTDIR\\{str(path).replace(os.sep, chr(92))}"\n')
    for path in sorted(directories, key=lambda value: (-len(value.parts), str(value))):
        handle.write(f'RMDir "$INSTDIR\\{str(path).replace(os.sep, chr(92))}"\n')
