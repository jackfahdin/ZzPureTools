#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: package-linux-appimage.sh \
  --build-dir <path> --qt-root <path> --evidence-root <path> \
  --gnu-license-dir <path> --linuxdeploy <path> --qt-plugin <path> \
  --appimagetool <path> --output-dir <path> \
  --commit <40-lower-hex> --built-at-utc <YYYY-MM-DDTHH:MM:SSZ>
USAGE
}

build_dir_arg=
qt_root_arg=
evidence_root_arg=
gnu_license_dir_arg=
linuxdeploy_arg=
qt_plugin_arg=
appimagetool_arg=
output_dir_arg=
commit=
built_at_utc=

while [[ $# -gt 0 ]]; do
  [[ $# -ge 2 ]] || {
    usage
    exit 64
  }
  case $1 in
    --build-dir) build_dir_arg=$2 ;;
    --qt-root) qt_root_arg=$2 ;;
    --evidence-root) evidence_root_arg=$2 ;;
    --gnu-license-dir) gnu_license_dir_arg=$2 ;;
    --linuxdeploy) linuxdeploy_arg=$2 ;;
    --qt-plugin) qt_plugin_arg=$2 ;;
    --appimagetool) appimagetool_arg=$2 ;;
    --output-dir) output_dir_arg=$2 ;;
    --commit) commit=$2 ;;
    --built-at-utc) built_at_utc=$2 ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 64
      ;;
  esac
  shift 2
done

for value_name in \
  build_dir_arg qt_root_arg evidence_root_arg gnu_license_dir_arg \
  linuxdeploy_arg qt_plugin_arg appimagetool_arg output_dir_arg \
  commit built_at_utc; do
  [[ -n ${!value_name} ]] || {
    echo "missing required argument: $value_name" >&2
    usage
    exit 64
  }
done

for tool in cmake env file find grep ln mktemp mv readelf realpath rm sed xvfb-run; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 69
  }
done

source_dir=$(realpath "$(dirname "${BASH_SOURCE[0]}")/../..")
build_root=$(realpath "$source_dir/build")

resolve_directory() {
  local label=$1
  local input=$2
  [[ -d $input && ! -L $input ]] || {
    echo "$label must be a regular directory: $input" >&2
    exit 1
  }
  realpath "$input"
}

resolve_executable() {
  local label=$1
  local input=$2
  [[ -f $input && ! -L $input && -x $input ]] || {
    echo "$label must be a regular executable file: $input" >&2
    exit 1
  }
  realpath "$input"
}

build_dir=$(resolve_directory build-dir "$build_dir_arg")
qt_root=$(resolve_directory qt-root "$qt_root_arg")
evidence_root=$(resolve_directory evidence-root "$evidence_root_arg")
gnu_license_dir=$(resolve_directory gnu-license-dir "$gnu_license_dir_arg")
output_dir=$(resolve_directory output-dir "$output_dir_arg")
linuxdeploy=$(resolve_executable linuxdeploy "$linuxdeploy_arg")
qt_plugin=$(resolve_executable qt-plugin "$qt_plugin_arg")
appimagetool=$(resolve_executable appimagetool "$appimagetool_arg")

case "$build_dir/" in
  "$build_root"/*) ;;
  *)
    echo "build-dir must be below the repository build directory" >&2
    exit 1
    ;;
esac
[[ $build_dir != "$build_root" && -f $build_dir/CMakeCache.txt ]] || {
  echo "build-dir must contain CMakeCache.txt" >&2
  exit 1
}
[[ $build_dir == "$build_root/linux-continuous-release" ]] || {
  echo "build-dir must come from linux-continuous-release" >&2
  exit 1
}
[[ -x $qt_root/bin/qmake ]] || {
  echo "qt-root must contain qmake" >&2
  exit 1
}
qt_version=$("$qt_root/bin/qmake" -query QT_VERSION)
[[ -n $qt_version ]] || {
  echo "qmake failed to query QT_VERSION" >&2
  exit 1
}
qt_license_dir=$(resolve_directory qt-license-dir \
  "$evidence_root/qt-$qt_version/LICENSES")
for license_file in COPYING3 COPYING.RUNTIME; do
  [[ -s $gnu_license_dir/$license_file && ! -L $gnu_license_dir/$license_file ]] || {
    echo "GNU runtime license is unavailable: $license_file" >&2
    exit 1
  }
done
[[ $commit =~ ^[0-9a-f]{40}$ ]] || {
  echo "commit must be 40 lowercase hexadecimal characters" >&2
  exit 1
}
[[ $built_at_utc =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$ ]] || {
  echo "built-at-utc must use UTC YYYY-MM-DDTHH:MM:SSZ" >&2
  exit 1
}
[[ -z $(find "$output_dir" -mindepth 1 -maxdepth 1 -print -quit) ]] || {
  echo "output-dir must be empty" >&2
  exit 1
}

cache_value() {
  local key=$1
  sed -n "s/^${key}:[^=]*=//p" "$build_dir/CMakeCache.txt"
}

require_cache_value() {
  local key=$1
  local expected=$2
  local actual
  actual=$(cache_value "$key")
  [[ $actual == "$expected" ]] || {
    echo "$key must equal $expected; found $actual" >&2
    exit 1
  }
}

require_cache_true() {
  local key=$1
  local actual
  actual=$(cache_value "$key")
  case $actual in
    1|ON|YES|TRUE|Y) ;;
    *)
      echo "$key must be enabled; found $actual" >&2
      exit 1
      ;;
  esac
}

require_cache_value CMAKE_BUILD_TYPE Release
require_cache_true BUILD_SHARED_LIBS
require_cache_true ZZ_ENABLE_LTO
require_cache_true ZZ_BUILD_TESTS
require_cache_true ZZ_BUILD_EXAMPLES
require_cache_true ZZ_RELEASE_BUILD
require_cache_true ZZ_BUNDLE_GNU_RUNTIME

cached_qt_root=$(realpath "$(cache_value ZZ_QT_PREFIX)")
cached_evidence_root=$(realpath "$(cache_value ZZ_RELEASE_EVIDENCE_ROOT)")
cached_gnu_license_dir=$(realpath "$(cache_value ZZ_GNU_RUNTIME_LICENSE_DIR)")
[[ $cached_qt_root == "$qt_root" ]] || {
  echo "qt-root does not match configured ZZ_QT_PREFIX" >&2
  exit 1
}
[[ $cached_evidence_root == "$evidence_root" ]] || {
  echo "evidence-root does not match configured release evidence" >&2
  exit 1
}
[[ $cached_gnu_license_dir == "$gnu_license_dir" ]] || {
  echo "gnu-license-dir does not match configured runtime licenses" >&2
  exit 1
}

source /etc/os-release
[[ ${ID:-} == ubuntu && ${VERSION_ID:-} == 22.04 ]] || {
  echo "AppImage packaging must run on Ubuntu 22.04" >&2
  exit 1
}

package_name="ZzPureToolsExample-continuous-linux-x86_64.AppImage"
final_package="$output_dir/$package_name"
final_checksum="$final_package.sha256"
final_build_info="$output_dir/build-info.json"
work_dir=$(mktemp -d "$output_dir/.linux-appimage.XXXXXX")
published=false

cleanup() {
  local status=$?
  trap - EXIT
  case "$work_dir/" in
    "$output_dir"/.linux-appimage.*) rm -rf -- "$work_dir" ;;
    *)
      echo "refusing to clean unexpected work directory: $work_dir" >&2
      status=1
      ;;
  esac
  if [[ $published != true ]]; then
    rm -f -- "$final_package" "$final_checksum" "$final_build_info"
  fi
  exit "$status"
}
trap cleanup EXIT

appdir="$work_dir/AppDir"
mkdir -p "$appdir/usr"
cmake --install "$build_dir" --prefix "$appdir/usr" --component Runtime
cmake --install "$build_dir" --prefix "$appdir/usr" --component ExampleRuntime

example_executable="$appdir/usr/bin/ZzPureToolsExample"
desktop_file="$appdir/usr/share/applications/io.github.jackfahdin.ZzPureToolsExample.desktop"
icon_file="$appdir/usr/share/icons/hicolor/256x256/apps/io.github.jackfahdin.ZzPureToolsExample.png"
for installed_file in "$example_executable" "$desktop_file" "$icon_file"; do
  [[ -s $installed_file && ! -L $installed_file ]] || {
    echo "installed Example runtime file is unavailable: $installed_file" >&2
    exit 1
  }
done
find "$appdir/usr/lib" -maxdepth 1 -type f -name 'libZzLog.so.*' -print -quit |
  grep -q . || {
    echo "Runtime component did not install libZzLog" >&2
    exit 1
  }

plugin_dir="$work_dir/tools"
mkdir -p "$plugin_dir"
ln -s "$qt_plugin" "$plugin_dir/linuxdeploy-plugin-qt"
PATH="$plugin_dir:$PATH" \
QMAKE="$qt_root/bin/qmake" \
APPIMAGE_EXTRACT_AND_RUN=1 \
  "$linuxdeploy" \
    --appdir "$appdir" \
    --executable "$example_executable" \
    --desktop-file "$desktop_file" \
    --icon-file "$icon_file" \
    --plugin qt

cmake \
  "-DZZ_STAGE_ROOT=$appdir" \
  "-DZZ_QT_LICENSE_DIR=$qt_license_dir" \
  "-DZZ_GNU_RUNTIME_LICENSE_DIR=$gnu_license_dir" \
  -P "$source_dir/scripts/package/StageRuntimeLicenses.cmake"

package_path="$work_dir/$package_name"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 \
  "$appimagetool" "$appdir" "$package_path"
[[ -s $package_path && -x $package_path ]] || {
  echo "appimagetool did not produce an executable AppImage" >&2
  exit 1
}

extract_dir="$work_dir/extracted"
mkdir -p "$extract_dir"
(
  cd "$extract_dir"
  "$package_path" --appimage-extract >/dev/null
)
extracted_root="$extract_dir/squashfs-root"
[[ -d $extracted_root && -s $extracted_root/THIRD_PARTY_NOTICES.md ]] || {
  echo "extracted AppImage lacks staged license notices" >&2
  exit 1
}
"$source_dir/scripts/ci/check-ubuntu2204-runtime.sh" \
  "$extracted_root/usr" "$qt_root"

xvfb-run -a env \
  APPIMAGE_EXTRACT_AND_RUN=1 \
  ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1500 \
  "$package_path" --smoke-test

compiler_path=$(realpath "$(cache_value CMAKE_CXX_COMPILER)")
compiler_version=$("$compiler_path" -dumpfullversion -dumpversion)
cmake \
  "-DZZ_PACKAGE_PATH=$package_path" \
  -DZZ_PLATFORM_ID=linux-x86_64 \
  "-DZZ_COMMIT=$commit" \
  -DZZ_DIRTY=false \
  "-DZZ_BUILT_AT_UTC=$built_at_utc" \
  "-DZZ_RUNNER_OS=${PRETTY_NAME:-Ubuntu 22.04}" \
  -DZZ_ARCHITECTURE=x86_64 \
  "-DZZ_QT_VERSION=$qt_version" \
  -DZZ_COMPILER_ID=GNU \
  "-DZZ_COMPILER_VERSION=$compiler_version" \
  -DZZ_PRESET=linux-continuous-release \
  -DZZ_LINKAGE=shared \
  -DZZ_LTO=true \
  -P "$source_dir/scripts/package/WriteBuildInfo.cmake"

mv -- "$package_path.sha256" "$final_checksum"
mv -- "$work_dir/build-info.json" "$final_build_info"
mv -- "$package_path" "$final_package"
published=true
echo "PASS Linux AppImage package: $final_package"
