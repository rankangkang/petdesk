#!/bin/bash
# Cross-build a per-user Windows x64 installer using an external Qt/LLVM SDK.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
: "${PETDESK_LLVM_MINGW_ROOT:?Set PETDESK_LLVM_MINGW_ROOT}"
: "${PETDESK_QT_WINDOWS_ROOT:?Set PETDESK_QT_WINDOWS_ROOT}"
: "${QT_HOST_PATH:?Set QT_HOST_PATH to the matching host Qt SDK}"
version=$(sed -n 's/^project(PetDesk VERSION \([0-9.]*\) LANGUAGES CXX).*/\1/p' "$project_dir/CMakeLists.txt")
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { printf 'Invalid version\n' >&2; exit 1; }
suffix=${PETDESK_WINDOWS_SUFFIX:-}
[[ "$suffix" =~ ^[A-Za-z0-9-]*$ ]] || { printf 'Invalid output suffix\n' >&2; exit 1; }
build_dir=${PETDESK_WINDOWS_BUILD_DIR:-"$project_dir/build/windows-cross"}
work_dir=${PETDESK_WINDOWS_WORK_DIR:-"$project_dir/build/windows-package"}
output_dir=${PETDESK_WINDOWS_OUTPUT_DIR:-"$project_dir/dist"}
mkdir -p "$work_dir" "$output_dir"
work_dir=$(cd "$work_dir" && pwd)
output_dir=$(cd "$output_dir" && pwd)
stage_parent=$(mktemp -d "$work_dir/stage.XXXXXXXX")
stage="$stage_parent/files"
mkdir -p "$stage/plugins/platforms" "$stage/plugins/imageformats" "$stage/licenses/Qt/SBOM" "$stage/licenses/LLVM"

cmake -S "$project_dir" -B "$build_dir" \
  -DCMAKE_TOOLCHAIN_FILE="$project_dir/cmake/windows-llvm-mingw.cmake" \
  -DQT_HOST_PATH="$QT_HOST_PATH" -DQt6_DIR="$PETDESK_QT_WINDOWS_ROOT/lib/cmake/Qt6" \
  -DCMAKE_PREFIX_PATH="$PETDESK_QT_WINDOWS_ROOT" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build_dir" --parallel 4
cp "$build_dir/PetDesk.exe" "$stage/"
for dll in Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll libc++.dll libunwind.dll; do
  cp "$PETDESK_QT_WINDOWS_ROOT/bin/$dll" "$stage/"
done
cp "$PETDESK_QT_WINDOWS_ROOT/plugins/platforms/qwindows.dll" "$stage/plugins/platforms/"
cp "$PETDESK_QT_WINDOWS_ROOT/plugins/imageformats/qwebp.dll" "$stage/plugins/imageformats/"
cp "$project_dir/packaging/windows-qt.conf" "$stage/qt.conf"
cp "$project_dir/THIRD_PARTY_NOTICES.txt" "$stage/"
cp "$project_dir"/LICENSES/* "$stage/licenses/Qt/"
for module in qtbase qtimageformats; do
  for notice in "$PETDESK_QT_WINDOWS_ROOT/sbom/$module-"*.spdx*; do
    [ -f "$notice" ] && cp "$notice" "$stage/licenses/Qt/SBOM/"
  done
done
cp "$PETDESK_LLVM_MINGW_ROOT/LICENSE.TXT" "$stage/licenses/LLVM/"
if [ -n "${PETDESK_BUNDLED_PETS_DIR:-}" ]; then
  [ -d "$PETDESK_BUNDLED_PETS_DIR" ] && [ ! -L "$PETDESK_BUNDLED_PETS_DIR" ] && \
    [ -f "$PETDESK_BUNDLED_PETS_DIR/manifest.json" ] || { printf 'Invalid bundled-pets directory\n' >&2; exit 1; }
  cp -R "$PETDESK_BUNDLED_PETS_DIR" "$stage/bundled-pets"
fi

python3 "$project_dir/scripts/generate-nsis-uninstall.py" "$stage" "$stage_parent/uninstall-files.nsh"
installer="$output_dir/PetDesk-$version${suffix}-Windows-x64-Setup.exe"
makensis -V2 -DVERSION="$version" -DSTAGE_DIR="$stage" -DOUTPUT_FILE="$installer" \
  -DICON_FILE="$project_dir/assets/PetDesk.ico" -DUNINSTALL_INCLUDE="$stage_parent/uninstall-files.nsh" \
  "$project_dir/packaging/windows-installer.nsi"
printf 'Installer: %s\nStaged files: %s\n' "$installer" "$stage"
