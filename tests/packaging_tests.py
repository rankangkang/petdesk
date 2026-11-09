#!/usr/bin/env python3
"""Exercise bundle preparation with a real PetDesk validator and a synthetic atlas."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

sys.dont_write_bytecode = True
VALIDATOR = Path(sys.argv.pop(1))
ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("bundle_preparation", ROOT / "scripts/prepare-bundled-pets.py")
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)
package_spec = importlib.util.spec_from_file_location("package_profile", ROOT / "scripts/package.py")
package = importlib.util.module_from_spec(package_spec)
package_spec.loader.exec_module(package)


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def make_pet(path: Path, name: str) -> bytes:
    path.mkdir()
    width, height = 1536, 2288
    row_stride = 1 + width * 4
    pixels = bytearray(row_stride * height)
    for row, count in enumerate((6, 8, 8, 4, 5, 8, 6, 6, 6, 8, 8)):
        for col in range(count):
            offset = (row * 208 + 2) * row_stride + 1 + (col * 192 + 2) * 4
            pixels[offset:offset + 4] = b"\x00\x00\xff\xff"
    image = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
             chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))
    (path / "spritesheet.png").write_bytes(image)
    (path / "pet.json").write_text(json.dumps({"displayName": name, "spritesheetPath": "spritesheet.png"}), encoding="utf-8")
    (path / "unrelated-source.txt").write_text("not needed in a distribution", encoding="utf-8")
    return image


class BundlePreparationTests(unittest.TestCase):
    def test_minimal_round_trip_and_protected_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); source = root / "source"; output = root / "bundle"
            image = make_pet(source, "测试宠物")
            bundle.prepare(VALIDATOR, output, [("example", source)])
            manifest = json.loads((output / "manifest.json").read_text())
            key = hashlib.sha256(hashlib.sha256(image).hexdigest().encode() + b"\0" + "测试宠物".encode()).hexdigest()
            self.assertEqual(manifest["pets"], [{"id": "example", "directory": "example", "key": key}])
            self.assertEqual((output / "example/spritesheet.png").read_bytes(), image)
            self.assertFalse((output / "example/unrelated-source.txt").exists())
            result = json.loads(subprocess.check_output([str(VALIDATOR), "--validate", str(output / "example")]))
            self.assertEqual(result["sha256"], hashlib.sha256(image).hexdigest())

    def test_invalid_duplicate_and_existing_outputs_are_not_modified(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); source = root / "source"; output = root / "bundle"
            make_pet(source, "Pet")
            with self.assertRaises(ValueError):
                bundle.prepare(VALIDATOR, output, [("one", source), ("two", source)])
            self.assertFalse(output.exists())
            with self.assertRaises(ValueError):
                bundle.prepare(VALIDATOR, output, [("../unsafe", source)])
            self.assertFalse(output.exists())
            output.mkdir(); marker = output / "existing"; marker.write_text("keep")
            with self.assertRaises(ValueError):
                bundle.prepare(VALIDATOR, output, [("one", source)])
            self.assertEqual(marker.read_text(), "keep")

    def test_profile_checks_toolchain_and_keeps_outputs_outside_repository(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); host = root / "qt-host"; target = root / "qt-win"; llvm = root / "llvm"
            for path in (host / "bin/qtpaths", host / "lib/cmake/Qt6/Qt6Config.cmake",
                         target / "lib/cmake/Qt6/Qt6Config.cmake", target / "bin/Qt6Core.dll",
                         target / "bin/Qt6Gui.dll", target / "bin/Qt6Widgets.dll", target / "bin/libc++.dll",
                         target / "bin/libunwind.dll", target / "plugins/platforms/qwindows.dll",
                         target / "plugins/imageformats/qwebp.dll", target / "plugins/imageformats/qjpeg.dll",
                         llvm / "bin/x86_64-w64-mingw32-clang++", llvm / "LICENSE.TXT"):
                path.parent.mkdir(parents=True, exist_ok=True); path.touch()
            qconfig = target / "mkspecs/qconfig.pri"; qconfig.parent.mkdir()
            qconfig.write_text("QT_VERSION = 6.9.3\nQT_CLANG_MAJOR_VERSION = 17\n")
            profile = {"qt_host": str(host), "qt_windows": str(target), "llvm_mingw": str(llvm),
                       "work_dir": str(root / "work"), "output_dir": str(root / "out")}
            config = root / "profile.json"; config.write_text(json.dumps(profile))
            def versions(command, **kwargs):
                return "6.9.3\n" if command[0].endswith("qtpaths") else "clang version 18.0.0\n"
            with patch.object(package.shutil, "which", return_value="/tool"), patch.object(package.subprocess, "check_output", side_effect=versions):
                with self.assertRaisesRegex(ValueError, "Clang 17"):
                    package.read_profile(config, "windows")
                profile["work_dir"] = str(ROOT / "build/private-test")
                config.write_text(json.dumps(profile))
                with self.assertRaisesRegex(ValueError, "outside the source repository"):
                    package.read_profile(config, "windows")


if __name__ == "__main__":
    unittest.main()
