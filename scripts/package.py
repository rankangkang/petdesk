#!/usr/bin/env python3
"""Check a local packaging profile and build an isolated Mac or Windows package."""
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Optional

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent


def path_value(value: str, base: Path) -> Path:
    if not isinstance(value, str) or not value:
        raise ValueError("Paths must be non-empty strings")
    path = Path(value).expanduser()
    return (path if path.is_absolute() else base / path).resolve()


def read_profile(filename: Path, target: str) -> dict:
    filename = filename.expanduser().resolve()
    config = json.loads(filename.read_text(encoding="utf-8"))
    if not isinstance(config, dict):
        raise ValueError("Profile must be a JSON object")
    base = filename.parent
    for key in ("qt_host", "work_dir", "output_dir"):
        config[key] = path_value(config[key], base)
    for key in ("work_dir", "output_dir"):
        if config[key] == ROOT or ROOT in config[key].parents:
            raise ValueError(f"{key} must be outside the source repository")
    required = [config["qt_host"] / "bin/qtpaths", config["qt_host"] / "lib/cmake/Qt6/Qt6Config.cmake"]
    if target == "windows":
        for key in ("qt_windows", "llvm_mingw"):
            config[key] = path_value(config[key], base)
        qt = config["qt_windows"]
        required.extend(qt / relative for relative in (
            "lib/cmake/Qt6/Qt6Config.cmake", "bin/Qt6Core.dll", "bin/Qt6Gui.dll", "bin/Qt6Widgets.dll",
            "bin/libc++.dll", "bin/libunwind.dll", "plugins/platforms/qwindows.dll", "plugins/imageformats/qwebp.dll",
            "plugins/imageformats/qjpeg.dll"))
        required.extend([config["llvm_mingw"] / "bin/x86_64-w64-mingw32-clang++", config["llvm_mingw"] / "LICENSE.TXT"])
        if not shutil.which("makensis"):
            raise ValueError("NSIS makensis was not found")
    else:
        if sys.platform != "darwin":
            raise ValueError("The macOS package must be built on macOS")
        required.extend(config["qt_host"] / relative for relative in
                        ("bin/macdeployqt", "plugins/platforms/libqcocoa.dylib", "plugins/imageformats/libqwebp.dylib",
                         "plugins/imageformats/libqjpeg.dylib", "plugins/styles/libqmacstyle.dylib"))
    for path in required:
        if not path.is_file():
            raise ValueError(f"Required tool or Qt file not found: {path}")
    if not shutil.which("cmake") or not shutil.which("ctest"):
        raise ValueError("CMake and CTest are required")
    version = subprocess.check_output([str(config["qt_host"] / "bin/qtpaths"), "--qt-version"], text=True).strip()
    if target == "windows":
        qconfig = (config["qt_windows"] / "mkspecs/qconfig.pri").read_text(encoding="utf-8")
        match = re.search(r"^QT_VERSION\s*=\s*(\S+)", qconfig, re.MULTILINE)
        if not match or match.group(1) != version:
            raise ValueError(f"Host and Windows Qt versions must match (host: {version})")
        sdk_clang = re.search(r"^QT_CLANG_MAJOR_VERSION\s*=\s*(\d+)", qconfig, re.MULTILINE)
        compiler = subprocess.check_output([str(config["llvm_mingw"] / "bin/x86_64-w64-mingw32-clang++"),
                                            "--version"], text=True)
        compiler_clang = re.search(r"clang version\s+(\d+)", compiler)
        if sdk_clang and (not compiler_clang or sdk_clang.group(1) != compiler_clang.group(1)):
            raise ValueError(f"Windows Qt requires LLVM-MinGW Clang {sdk_clang.group(1)}")
    config["qt_version"] = version
    pets = config.get("pets", [])
    if not isinstance(pets, list) or len(pets) > 100:
        raise ValueError("pets must be an array with at most 100 entries")
    ids = set()
    for pet in pets:
        if not isinstance(pet, dict) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", pet.get("id", "")) or pet["id"] in ids:
            raise ValueError("Each pet needs a unique, lowercase id")
        ids.add(pet["id"])
        pet["path"] = path_value(pet["path"], base)
        if not pet["path"].is_dir():
            raise ValueError(f"Pet directory not found: {pet['path']}")
    icons = config.get("icons")
    if icons:
        if not isinstance(icons, dict):
            raise ValueError("icons must be an object")
        for key in ("application", "status"):
            icons[key] = path_value(icons[key], base)
            if not icons[key].is_file():
                raise ValueError(f"Icon source not found: {icons[key]}")
        if target == "macos" and not shutil.which("iconutil"):
            raise ValueError("macOS iconutil was not found")
    for suffix_name in ("windows_suffix", "macos_suffix"):
        suffix = config.get(suffix_name, "")
        if not isinstance(suffix, str) or not re.fullmatch(r"[A-Za-z0-9-]*", suffix):
            raise ValueError(f"{suffix_name} may only contain letters, digits, or hyphens")
    return config


def run(command: list[str], env: Optional[dict] = None) -> None:
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], env=env, check=True)


def build(config: dict, target: str, skip_tests: bool, check_content: bool, dmg: bool) -> None:
    work = config["work_dir"]
    work.mkdir(parents=True, exist_ok=True)
    host_id = hashlib.sha256(f"{config['qt_host']}:{config['qt_version']}".encode()).hexdigest()[:12]
    host_build = work / f"host-{host_id}"
    run(["cmake", "-S", ROOT, "-B", host_build, "-DCMAKE_BUILD_TYPE=Release",
         f"-DCMAKE_PREFIX_PATH={config['qt_host']}", f"-DQt6_DIR:PATH={config['qt_host'] / 'lib/cmake/Qt6'}",
         f"-DPETDESK_ICON_DIR={ROOT / 'assets'}",
         "-DBUILD_TESTING=ON"])
    if skip_tests:
        run(["cmake", "--build", host_build, "--target", "PetDesk", "--parallel", "4"])
    else:
        run(["cmake", "--build", host_build, "--parallel", "4"])
        run(["ctest", "--test-dir", host_build, "--output-on-failure"])
    validator = host_build / ("PetDesk.app/Contents/MacOS/PetDesk" if sys.platform == "darwin" else "PetDesk")

    with tempfile.TemporaryDirectory(prefix="petdesk-package-", dir=work) as temporary:
        stage = Path(temporary)
        env = os.environ.copy()
        pets = config.get("pets", [])
        if pets:
            spec = importlib.util.spec_from_file_location("prepare_bundled_pets", ROOT / "scripts/prepare-bundled-pets.py")
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            module.prepare(validator, stage / "bundled-pets", [(pet["id"], pet["path"]) for pet in pets])
            env["PETDESK_BUNDLED_PETS_DIR"] = str(stage / "bundled-pets")
        else:
            env.pop("PETDESK_BUNDLED_PETS_DIR", None)
        icons = config.get("icons")
        if icons:
            run(["cmake", "--build", host_build, "--target", "petdesk_icon_generator", "--parallel", "4"])
            generated = stage / "icons"; generated.mkdir()
            iconset = generated / "PetDesk.iconset"
            run([host_build / "petdesk_icon_generator", icons["application"], icons["status"],
                 generated / "PetDesk.png", iconset, generated / "PetDesk-status.png",
                 generated / "PetDesk.ico", "--preserve"])
            if target == "macos":
                run(["iconutil", "-c", "icns", iconset, "-o", generated / "PetDesk.icns"])
            env["PETDESK_ICON_DIR"] = str(generated)
        else:
            env["PETDESK_ICON_DIR"] = str(ROOT / "assets")
        if check_content:
            print("Pet contents and icon artwork verified; no package created.")
            return
        output = stage / "output"; output.mkdir()
        if target == "windows":
            windows_id = hashlib.sha256(f"{config['qt_windows']}:{config['llvm_mingw']}:{config['qt_version']}".encode()).hexdigest()[:12]
            env.update({"PETDESK_LLVM_MINGW_ROOT": str(config["llvm_mingw"]),
                        "PETDESK_QT_WINDOWS_ROOT": str(config["qt_windows"]), "QT_HOST_PATH": str(config["qt_host"]),
                        "PETDESK_WINDOWS_BUILD_DIR": str(work / f"windows-{windows_id}"),
                        "PETDESK_WINDOWS_WORK_DIR": str(stage / "windows-stage"),
                        "PETDESK_WINDOWS_OUTPUT_DIR": str(output),
                        "PETDESK_WINDOWS_SUFFIX": config.get("windows_suffix", "")})
            run(["bash", ROOT / "scripts/package-windows.sh"], env)
            artifacts = list(output.glob("*.exe"))
        else:
            env.update({"PETDESK_MACOS_BUILD_DIR": str(host_build), "PETDESK_MACOS_OUTPUT_DIR": str(output)})
            run(["bash", ROOT / "scripts/package-macos.sh"], env)
            artifacts = [output / "PetDesk.app"]
            if dmg:
                contents = stage / "dmg-contents"; contents.mkdir()
                run(["ditto", artifacts[0], contents / "PetDesk.app"])
                (contents / "Applications").symlink_to("/Applications")
                version_match = re.search(r"^project\(PetDesk VERSION\s+([0-9.]+)",
                                          (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), re.MULTILINE)
                if not version_match:
                    raise ValueError("Could not determine the PetDesk version")
                architectures = subprocess.check_output(["lipo", "-archs", str(artifacts[0] / "Contents/MacOS/PetDesk")], text=True).split()
                if len(architectures) != 1 or architectures[0] not in ("arm64", "x86_64"):
                    raise ValueError("The DMG needs a single supported macOS architecture")
                filename = f"PetDesk-{version_match.group(1)}{config.get('macos_suffix', '')}-macOS-{architectures[0]}.dmg"
                image = output / filename
                run(["hdiutil", "create", "-volname", "PetDesk", "-srcfolder", contents,
                     "-format", "UDZO", "-fs", "HFS+", image])
                artifacts = [image]
        if len(artifacts) != 1 or not artifacts[0].exists():
            raise ValueError("Packaging did not produce exactly one artifact")
        config["output_dir"].mkdir(parents=True, exist_ok=True)
        destination = Path(tempfile.mkdtemp(prefix=f"{target}-{datetime.now():%Y%m%d-%H%M%S}-", dir=config["output_dir"]))
        final = destination / artifacts[0].name
        if target == "macos" and not dmg:
            run(["ditto", artifacts[0], final])
            run(["codesign", "--verify", "--deep", "--strict", final])
        else:
            shutil.copy2(artifacts[0], final)
            if dmg:
                run(["hdiutil", "verify", final])
        print(f"\nPackage: {final}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=("macos", "windows"))
    parser.add_argument("--config", type=Path,
                        default=Path(os.environ.get("PETDESK_PACKAGE_PROFILE", "~/.config/petdesk/packaging.json")),
                        help="Profile path (default: ~/.config/petdesk/packaging.json)")
    checks = parser.add_mutually_exclusive_group()
    checks.add_argument("--dry-run", action="store_true", help="Check configuration without building or packaging")
    checks.add_argument("--check-content", action="store_true", help="Build host tools and validate artwork without packaging")
    parser.add_argument("--skip-tests", action="store_true", help="Skip host tests for a faster repeat build")
    parser.add_argument("--dmg", action="store_true", help="Create a macOS drag-to-Applications disk image")
    args = parser.parse_args()
    try:
        if args.dmg and (args.target != "macos" or not shutil.which("hdiutil")):
            raise ValueError("--dmg requires macOS and hdiutil")
        config = read_profile(args.config, args.target)
        print(f"Target: {args.target}; Qt: {config['qt_version']}; bundled pets: {len(config.get('pets', []))}")
        print(f"Work: {config['work_dir']}\nOutput: {config['output_dir']}")
        print(f"Icons: {'custom artwork' if config.get('icons') else 'project defaults'}")
        if args.dry_run:
            print("Configuration paths OK; pet contents will be validated when packaging. No build or package created.")
        else:
            build(config, args.target, args.skip_tests, args.check_content, args.dmg)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")


if __name__ == "__main__":
    main()
