#!/bin/bash
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
qt_prefix=$(qtpaths --query QT_INSTALL_PREFIX)
plugin_dir=$(qtpaths --query QT_INSTALL_PLUGINS)
bundle="$project_dir/dist/PetDesk.app"

cmake --build "$project_dir/build" --parallel 4
mkdir -p "$project_dir/dist"
# dist is generated output. Replace this app only; do not touch installed apps.
if [ -d "$bundle" ]; then rm -rf "$bundle"; fi
ditto "$project_dir/build/PetDesk.app" "$bundle"
mkdir -p "$bundle/Contents/PlugIns/platforms" "$bundle/Contents/PlugIns/imageformats" "$bundle/Contents/PlugIns/styles"
ditto "$plugin_dir/platforms/libqcocoa.dylib" "$bundle/Contents/PlugIns/platforms/libqcocoa.dylib"
ditto "$plugin_dir/imageformats/libqwebp.dylib" "$bundle/Contents/PlugIns/imageformats/libqwebp.dylib"
ditto "$plugin_dir/styles/libqmacstyle.dylib" "$bundle/Contents/PlugIns/styles/libqmacstyle.dylib"
"$qt_prefix/bin/macdeployqt" "$bundle" -no-plugins -always-overwrite -verbose=1 \
  "-executable=$bundle/Contents/PlugIns/platforms/libqcocoa.dylib" \
  "-executable=$bundle/Contents/PlugIns/imageformats/libqwebp.dylib" \
  "-executable=$bundle/Contents/PlugIns/styles/libqmacstyle.dylib"
mkdir -p "$bundle/Contents/Resources/licenses"
cp "$project_dir/THIRD_PARTY_NOTICES.txt" "$bundle/Contents/Resources/"
ditto "$project_dir/LICENSES" "$bundle/Contents/Resources/licenses/Qt"

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
python3 "$project_dir/scripts/check_bundle.py" "$bundle"
codesign --force --deep --sign - "$bundle"
codesign --verify --deep --strict "$bundle"
printf 'Packaged: %s\n' "$bundle"
