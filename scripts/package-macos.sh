#!/bin/bash
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
build_dir=${PETDESK_MACOS_BUILD_DIR:-"$project_dir/build"}
output_dir=${PETDESK_MACOS_OUTPUT_DIR:-"$project_dir/dist"}
icon_dir=${PETDESK_ICON_DIR:-"$project_dir/assets"}
[ -f "$build_dir/CMakeCache.txt" ] || { printf 'Configure the host Qt build first.\n' >&2; exit 1; }
qt_package_dir=$(sed -n 's/^Qt6_DIR:PATH=//p' "$build_dir/CMakeCache.txt")
[ -n "$qt_package_dir" ] && [ -d "$qt_package_dir" ] || { printf 'Configure the host Qt build first.\n' >&2; exit 1; }
qt_prefix=$(cd "$qt_package_dir/../../.." && pwd)
plugin_dir=$("$qt_prefix/bin/qtpaths" --query QT_INSTALL_PLUGINS)
bundle="$output_dir/PetDesk.app"
[ ! -e "$bundle" ] && [ ! -L "$bundle" ] || { printf 'Output already exists: %s\n' "$bundle" >&2; exit 1; }
if [ -n "${PETDESK_BUNDLED_PETS_DIR:-}" ]; then
  [ -d "$PETDESK_BUNDLED_PETS_DIR" ] && [ ! -L "$PETDESK_BUNDLED_PETS_DIR" ] && \
    [ -f "$PETDESK_BUNDLED_PETS_DIR/manifest.json" ] || { printf 'Invalid bundled-pets directory\n' >&2; exit 1; }
fi

cmake -S "$project_dir" -B "$build_dir" -DPETDESK_ICON_DIR="$icon_dir"
cmake --build "$build_dir" --parallel 4
mkdir -p "$output_dir"
ditto "$build_dir/PetDesk.app" "$bundle"
if [ -n "${PETDESK_BUNDLED_PETS_DIR:-}" ]; then
  ditto "$PETDESK_BUNDLED_PETS_DIR" "$bundle/Contents/MacOS/bundled-pets"
fi
mkdir -p "$bundle/Contents/PlugIns/platforms" "$bundle/Contents/PlugIns/imageformats" "$bundle/Contents/PlugIns/styles"
ditto "$plugin_dir/platforms/libqcocoa.dylib" "$bundle/Contents/PlugIns/platforms/libqcocoa.dylib"
ditto "$plugin_dir/imageformats/libqwebp.dylib" "$bundle/Contents/PlugIns/imageformats/libqwebp.dylib"
ditto "$plugin_dir/imageformats/libqjpeg.dylib" "$bundle/Contents/PlugIns/imageformats/libqjpeg.dylib"
ditto "$plugin_dir/styles/libqmacstyle.dylib" "$bundle/Contents/PlugIns/styles/libqmacstyle.dylib"
"$qt_prefix/bin/macdeployqt" "$bundle" -no-plugins -always-overwrite -verbose=1 \
  "-executable=$bundle/Contents/PlugIns/platforms/libqcocoa.dylib" \
  "-executable=$bundle/Contents/PlugIns/imageformats/libqwebp.dylib" \
  "-executable=$bundle/Contents/PlugIns/imageformats/libqjpeg.dylib" \
  "-executable=$bundle/Contents/PlugIns/styles/libqmacstyle.dylib"
mkdir -p "$bundle/Contents/Resources/licenses"
cp "$project_dir/packaging/qt.conf" "$bundle/Contents/Resources/qt.conf"
cp "$project_dir/THIRD_PARTY_NOTICES.txt" "$bundle/Contents/Resources/"
ditto "$project_dir/LICENSES" "$bundle/Contents/Resources/licenses/Qt"
if [ -d "$qt_prefix/sbom" ]; then
  mkdir -p "$bundle/Contents/Resources/licenses/Qt/SBOM"
  for module in qtbase qtimageformats; do
    for notice in "$qt_prefix/sbom/$module-"*.spdx.json; do
      if [ -f "$notice" ]; then cp "$notice" "$bundle/Contents/Resources/licenses/Qt/SBOM/"; fi
    done
  done
fi

# Keep the installed notices for dynamic Homebrew dependencies in the bundle.
for formula in qt glib libpng harfbuzz graphite2 freetype md4c pcre2 gettext brotli webp jpeg-turbo libtiff zstd xz; do
  cellar=$(brew --cellar "$formula" 2>/dev/null || true)
  if [ -d "$cellar" ]; then
    while IFS= read -r notice; do
      version_dir=${notice#"$cellar/"}
      version_dir=${version_dir%%/*}
      relative=${notice#"$cellar/$version_dir/"}
      destination="$bundle/Contents/Resources/licenses/$formula/$relative"
      mkdir -p "$(dirname "$destination")"
      cp "$notice" "$destination"
    done < <(find "$cellar" -type f \( -iname 'COPYING*' -o -iname 'LICENSE*' -o -iname 'LGPL*' \) 2>/dev/null)
  fi
done
python3 "$project_dir/scripts/thin-macos.py" "$bundle"
python3 "$project_dir/scripts/check_bundle.py" "$bundle"
codesign --force --deep --sign - "$bundle"
codesign --verify --deep --strict "$bundle"
printf 'Packaged: %s\n' "$bundle"
