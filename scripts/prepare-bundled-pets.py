#!/usr/bin/env python3
"""Build a minimal, validated bundled-pets directory outside the source tree."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def prepare(validator: Path, output: Path, pets: list[tuple[str, Path]]) -> None:
    if not pets or len(pets) > 100:
        raise ValueError("Provide between 1 and 100 pets")
    if output.exists() or output.is_symlink():
        raise ValueError(f"Output already exists: {output}")
    entries, ids, keys = [], set(), set()
    for pet_id, source in pets:
        if not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", pet_id) or pet_id in ids:
            raise ValueError(f"Invalid or duplicate pet id: {pet_id}")
        result = subprocess.run([str(validator), "--validate", str(source)], capture_output=True, text=True)
        if result.returncode:
            raise ValueError(f"Invalid pet {pet_id}: {result.stderr.strip()}")
        pet = json.loads(result.stdout)
        encoded = Path(pet["atlasPath"]).read_bytes()
        if hashlib.sha256(encoded).hexdigest() != pet["sha256"]:
            raise ValueError(f"Pet changed while preparing: {pet_id}")
        key = hashlib.sha256(pet["sha256"].encode() + b"\0" + pet["name"].encode()).hexdigest()
        if key in keys:
            raise ValueError(f"Duplicate pet content and name: {pet_id}")
        ids.add(pet_id); keys.add(key)
        entries.append((pet_id, pet, key, encoded))

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".bundled-pets-", dir=output.parent))
    try:
        manifest = []
        for pet_id, pet, key, encoded in entries:
            directory = temporary / pet_id
            directory.mkdir()
            atlas = f"spritesheet.{pet['format']}"
            (directory / atlas).write_bytes(encoded)
            metadata = {"id": pet["id"] or pet_id, "displayName": pet["name"],
                        "description": pet["description"], "spritesheetPath": atlas,
                        "spriteVersionNumber": pet["version"], "sha256": pet["sha256"]}
            (directory / "pet.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            manifest.append({"id": pet_id, "directory": pet_id, "key": key})
        (temporary / "manifest.json").write_text(json.dumps({"version": 1, "pets": manifest}, indent=2) + "\n", encoding="utf-8")
        if output.exists() or output.is_symlink():
            raise ValueError(f"Output already exists: {output}")
        os.rename(temporary, output)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validator", required=True, type=Path, help="Host PetDesk executable")
    parser.add_argument("--output", required=True, type=Path, help="New output directory")
    parser.add_argument("--pet", action="append", required=True, metavar="ID=PATH")
    args = parser.parse_args()
    pets = []
    for item in args.pet:
        if "=" not in item:
            parser.error("--pet requires ID=PATH")
        pet_id, path = item.split("=", 1)
        pets.append((pet_id, Path(path)))
    try:
        prepare(args.validator, args.output, pets)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Prepared {len(pets)} pets: {args.output}")


if __name__ == "__main__":
    main()
