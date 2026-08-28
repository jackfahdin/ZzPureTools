#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: package-macos.sh \
  --build-dir <path> --qt-root <path> --evidence-root <path> \
  --output-dir <path> --commit <40-lower-hex> \
  --built-at-utc <YYYY-MM-DDTHH:MM:SSZ> --architecture <arm64|x86_64>
USAGE
}

build_dir_arg=
qt_root_arg=
evidence_root_arg=
output_dir_arg=
commit=
built_at_utc=
architecture=

while [[ $# -gt 0 ]]; do
  [[ $# -ge 2 ]] || {
    usage
    exit 64
  }
  case $1 in
    --build-dir) build_dir_arg=$2 ;;
    --qt-root) qt_root_arg=$2 ;;
    --evidence-root) evidence_root_arg=$2 ;;
    --output-dir) output_dir_arg=$2 ;;
    --commit) commit=$2 ;;
    --built-at-utc) built_at_utc=$2 ;;
    --architecture) architecture=$2 ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 64
      ;;
  esac
  shift 2
done

for value_name in \
  build_dir_arg qt_root_arg evidence_root_arg output_dir_arg \
  commit built_at_utc architecture; do
  [[ -n ${!value_name} ]] || {
    echo "missing required argument: $value_name" >&2
    usage
    exit 64
  }
done

case $architecture in
  arm64|x86_64) ;;
  *)
    echo "architecture must be arm64 or x86_64" >&2
    exit 1
    ;;
esac
[[ $commit =~ ^[0-9a-f]{40}$ ]] || {
  echo "commit must be 40 lowercase hexadecimal characters" >&2
  exit 1
}
[[ $built_at_utc =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$ ]] || {
  echo "built-at-utc must use UTC YYYY-MM-DDTHH:MM:SSZ" >&2
  exit 1
}

for tool in awk bash cmake ditto env file find grep hdiutil install_name_tool \
            lipo ln mkdir mktemp mv otool realpath rm sed sw_vers; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 69
  }
done

source_dir=$(realpath "$(dirname "${BASH_SOURCE[0]}")/../..")
build_root=$(realpath "$source_dir/build")
architecture_policy="$source_dir/scripts/package/ZzMacosArchitecturePolicy.sh"
[[ -f $architecture_policy && ! -L $architecture_policy ]] || {
  echo "macOS architecture policy is unavailable: $architecture_policy" >&2
  exit 1
}
# shellcheck source=ZzMacosArchitecturePolicy.sh
source "$architecture_policy"

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
output_dir=$(resolve_directory output-dir "$output_dir_arg")
shopt -s nullglob dotglob
output_entries=("$output_dir"/*)
shopt -u nullglob dotglob
[[ ${#output_entries[@]} -eq 0 ]] || {
  echo "output-dir must be empty" >&2
  exit 1
}

case "$build_dir/" in
  "$build_root"/*) ;;
  *)
    echo "build-dir must be below the repository build directory" >&2
    exit 1
    ;;
esac
preset="macos-continuous-$architecture"
[[ $build_dir == "$build_root/$preset" && -f $build_dir/CMakeCache.txt ]] || {
  echo "build-dir must come from $preset" >&2
  exit 1
}

qmake=$(resolve_executable qmake "$qt_root/bin/qmake")
macdeployqt=$(resolve_executable macdeployqt "$qt_root/bin/macdeployqt")
queried_qt_root=$("$qmake" -query QT_INSTALL_PREFIX)
[[ $(realpath "$queried_qt_root") == "$qt_root" ]] || {
  echo "qmake prefix does not match qt-root" >&2
  exit 1
}
qt_version=$("$qmake" -query QT_VERSION)
qt_license_dir=$(resolve_directory qt-license-dir \
  "$evidence_root/qt-$qt_version/LICENSES")
qmake_xspec=$("$qmake" -query QMAKE_XSPEC)
[[ -n $qt_version && $qmake_xspec == macx-clang* ]] || {
  echo "qt-root is not a macOS Clang Qt kit" >&2
  exit 1
}
qt_core="$qt_root/lib/QtCore.framework/QtCore"
if [[ ! -f $qt_core ]]; then
  echo "QtCore architecture check failed: binary is missing" >&2
  printf 'QtCore path: %s\nexpected architecture: %s\n' \
    "$qt_core" "$architecture" >&2
  exit 1
fi
set +e
qt_core_file_output=$(file "$qt_core" 2>&1)
qt_core_file_status=$?
qt_core_archs=$(lipo -archs "$qt_core" 2>&1)
qt_core_lipo_status=$?
set -e
if [[ $qt_core_file_status -ne 0 || $qt_core_lipo_status -ne 0 ]] \
    || ! zz_macos_arch_list_contains "$qt_core_archs" "$architecture"; then
  echo "QtCore architecture check failed" >&2
  printf '%s\n' \
    "QtCore path: $qt_core" \
    "file exit: $qt_core_file_status" \
    "file output: $qt_core_file_output" \
    "lipo exit: $qt_core_lipo_status" \
    "lipo architectures: $qt_core_archs" \
    "expected architecture: $architecture" >&2
  exit 1
fi
offscreen_plugin="$qt_root/plugins/platforms/libqoffscreen.dylib"
[[ -f $offscreen_plugin ]] || {
  echo "Qt kit lacks the offscreen platform plugin" >&2
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

require_cache_false() {
  local key=$1
  local actual
  actual=$(cache_value "$key")
  case $actual in
    0|OFF|NO|FALSE|N|IGNORE) ;;
    *)
      echo "$key must be disabled; found $actual" >&2
      exit 1
      ;;
  esac
}

require_cache_value CMAKE_BUILD_TYPE Release
require_cache_value CMAKE_OSX_ARCHITECTURES "$architecture"
require_cache_true BUILD_SHARED_LIBS
require_cache_true ZZ_ENABLE_LTO
require_cache_true ZZ_BUILD_TESTS
require_cache_true ZZ_BUILD_EXAMPLES
require_cache_true ZZ_RELEASE_BUILD
require_cache_false ZZ_BUILD_BENCHMARKS
require_cache_false ZZ_ENABLE_CLANG_TIDY

cached_qt_root=$(realpath "$(cache_value ZZ_QT_PREFIX)")
cached_evidence_root=$(realpath "$(cache_value ZZ_RELEASE_EVIDENCE_ROOT)")
[[ $cached_qt_root == "$qt_root" ]] || {
  echo "qt-root does not match configured ZZ_QT_PREFIX" >&2
  exit 1
}
[[ $cached_evidence_root == "$evidence_root" ]] || {
  echo "evidence-root does not match configured release evidence" >&2
  exit 1
}

compiler_state=
compiler_state_count=0
while IFS= read -r candidate; do
  compiler_state=$candidate
  compiler_state_count=$((compiler_state_count + 1))
done < <(find "$build_dir/CMakeFiles" -name CMakeCXXCompiler.cmake -type f)
[[ $compiler_state_count -eq 1 ]] || {
  echo "expected exactly one CMakeCXXCompiler.cmake" >&2
  exit 1
}
compiler_id=$(sed -n \
  's/^set(CMAKE_CXX_COMPILER_ID "\([^"]*\)")$/\1/p' \
  "$compiler_state")
compiler_version=$(sed -n \
  's/^set(CMAKE_CXX_COMPILER_VERSION "\([^"]*\)")$/\1/p' \
  "$compiler_state")
[[ $compiler_id == AppleClang && -n $compiler_version ]] || {
  echo "configured compiler is not AppleClang" >&2
  exit 1
}

install_root=
app_bundle=
app_executable=
app_resources=
working_package=
mount_dir=
mounted=false
published=false

install_component() {
  local component=$1
  cmake --install "$build_dir" \
    --prefix "$install_root" \
    --config Release \
    --component "$component"
}

stage_first_party_libraries() {
  local bundle=$1
  local framework_dir="$bundle/Contents/Frameworks"
  local library
  local regular_library_count=0
  local -a source_libraries=()
  shopt -s nullglob
  source_libraries=("$install_root/lib"/libZz*.dylib)
  shopt -u nullglob
  [[ ${#source_libraries[@]} -gt 0 ]] || {
    echo "installed runtime contains no first-party libraries" >&2
    exit 1
  }
  mkdir -p "$framework_dir"
  for library in "${source_libraries[@]}"; do
    [[ -f $library ]] || {
      echo "invalid first-party runtime library: $library" >&2
      exit 1
    }
    ditto "$library" "$framework_dir/${library##*/}"
    if [[ ! -L $library ]]; then
      regular_library_count=$((regular_library_count + 1))
    fi
  done
  [[ $regular_library_count -gt 0 ]] || {
    echo "installed runtime lacks regular first-party library files" >&2
    exit 1
  }
}

stage_offscreen_plugin() {
  local bundle=$1
  local plugin_dir="$bundle/Contents/PlugIns/platforms"
  local plugin="$plugin_dir/libqoffscreen.dylib"
  mkdir -p "$plugin_dir"
  ditto "$offscreen_plugin" "$plugin"
  [[ -f $plugin && ! -L $plugin ]] || {
    echo "failed to stage the offscreen platform plugin" >&2
    exit 1
  }
}

invoke_macdeployqt() {
  local bundle=$1
  local framework_dir="$bundle/Contents/Frameworks"
  local plugin="$bundle/Contents/PlugIns/platforms/libqoffscreen.dylib"
  [[ -d $framework_dir && ! -L $framework_dir &&
     -f $plugin && ! -L $plugin ]] || {
    echo "staged macOS deployment inputs are unavailable" >&2
    exit 1
  }
  "$macdeployqt" "$bundle" \
    -always-overwrite \
    "-libpath=$framework_dir" \
    "-executable=$plugin"
}

strip_transient_rpaths() {
  local bundle=$1
  local binary description load_commands rpath forbidden_path
  while IFS= read -r -d '' binary; do
    description=$(file -b "$binary")
    case $description in
      *Mach-O*) ;;
      *) continue ;;
    esac
    load_commands=$(otool -l "$binary")
    while IFS= read -r rpath; do
      [[ -n $rpath ]] || continue
      for forbidden_path in "$source_dir" "$build_dir" "$qt_root"; do
        case $rpath in
          "$forbidden_path"|"$forbidden_path"/*)
            install_name_tool -delete_rpath "$rpath" "$binary"
            break
            ;;
        esac
      done
    done < <(printf '%s\n' "$load_commands" | awk '
      $1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
      want_path && $1 == "path" { print $2; want_path = 0 }
    ')
  done < <(find "$bundle" -type f -print0)
}

audit_app_bundle() {
  local bundle=$1
  local macho_count=0
  local first_party_count=0
  local qt_framework_count=0
  local binary description archs links load_commands dependency rpath

  while IFS= read -r -d '' binary; do
    description=$(file -b "$binary")
    case $description in
      *Mach-O*) ;;
      *) continue ;;
    esac
    macho_count=$((macho_count + 1))
    case $binary in
      */Contents/Frameworks/libZz*.dylib)
        first_party_count=$((first_party_count + 1))
        ;;
      */Contents/Frameworks/Qt*.framework/Versions/*/Qt*)
        qt_framework_count=$((qt_framework_count + 1))
        ;;
    esac

    archs=$(lipo -archs "$binary")
    case $binary in
      */Contents/MacOS/ZzPureToolsExample|*/Contents/Frameworks/libZz*.dylib)
        zz_macos_arch_list_is_exact "$archs" "$architecture" || {
          echo "unexpected first-party architecture in $binary: $archs" >&2
          exit 1
        }
        ;;
      *)
        zz_macos_arch_list_contains "$archs" "$architecture" || {
          echo "missing $architecture architecture in $binary: $archs" >&2
          exit 1
        }
        ;;
    esac
    links=$(otool -L "$binary" | awk '/^[[:space:]]/ { print }')
    load_commands=$(otool -l "$binary")
    for forbidden_path in "$source_dir" "$build_dir" "$qt_root"; do
      if [[ $links == *"$forbidden_path"* ]]; then
        echo "build dependency path leaked into $binary: $forbidden_path" >&2
        printf '%s\n' "$links" | grep -F "$forbidden_path" >&2
        exit 1
      fi
      if [[ $load_commands == *"$forbidden_path"* ]]; then
        echo "build load-command path leaked into $binary: $forbidden_path" >&2
        printf '%s\n' "$load_commands" | grep -F "$forbidden_path" >&2
        exit 1
      fi
    done

    while IFS= read -r dependency; do
      dependency=$(printf '%s\n' "$dependency" |
        sed 's/^[[:space:]]*//;s/[[:space:]]*(compatibility version.*//')
      [[ -n $dependency && $dependency != "$binary:" ]] || continue
      [[ $dependency != *[Qq][Ww]indow[Kk]it* ]] || {
        echo "QWindowKit leaked into $binary: $dependency" >&2
        exit 1
      }
      case $dependency in
        @rpath/*|@loader_path/*|@executable_path/*|/System/Library/*|/usr/lib/*) ;;
        *)
          echo "unexpected dependency in $binary: $dependency" >&2
          exit 1
          ;;
      esac
    done <<< "$links"

    while IFS= read -r rpath; do
      [[ -n $rpath ]] || continue
      case $rpath in
        @rpath/*|@loader_path/*|@executable_path/*|/System/Library/*|/usr/lib/*) ;;
        *)
          echo "unexpected LC_RPATH in $binary: $rpath" >&2
          exit 1
          ;;
      esac
    done < <(printf '%s\n' "$load_commands" | awk '
      $1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
      want_path && $1 == "path" { print $2; want_path = 0 }
    ')
  done < <(find "$bundle" -type f -print0)

  [[ $macho_count -gt 0 &&
     $first_party_count -gt 0 &&
     $qt_framework_count -gt 0 ]] || {
    echo "deployed app lacks required Mach-O payloads: " \
      "all=$macho_count first-party=$first_party_count Qt=$qt_framework_count" >&2
    exit 1
  }
}

invoke_app_smoke() {
  local executable=$1
  [[ -x $executable && ! -L $executable ]] || {
    echo "app smoke executable is unavailable: $executable" >&2
    exit 1
  }
  QT_QPA_PLATFORM=offscreen \
  ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1500 \
    "$executable" --smoke-test
}

stage_runtime_licenses() {
  local bundle=$1
  local contents="$bundle/Contents"
  local resources="$contents/Resources"
  [[ -d $contents && -d $resources ]] || {
    echo "app bundle lacks Contents/Resources" >&2
    exit 1
  }
  cmake \
    "-DZZ_STAGE_ROOT=$contents" \
    "-DZZ_QT_LICENSE_DIR=$qt_license_dir" \
    -P "$source_dir/scripts/package/StageRuntimeLicenses.cmake"
  [[ ! -e $resources/licenses &&
     ! -e $resources/THIRD_PARTY_NOTICES.md ]] || {
    echo "app Resources already contains license staging outputs" >&2
    exit 1
  }
  mv -- "$contents/licenses" "$resources/licenses"
  mv -- "$contents/THIRD_PARTY_NOTICES.md" \
    "$resources/THIRD_PARTY_NOTICES.md"
}

create_dmg() {
  local bundle=$1
  local dmg_root="$work_dir/dmg-root"
  mkdir -p "$dmg_root"
  ditto "$bundle" "$dmg_root/ZzPureToolsExample.app"
  ln -s /Applications "$dmg_root/Applications"
  hdiutil create \
    -volname ZzPureToolsExample \
    -srcfolder "$dmg_root" \
    -format UDZO \
    -ov "$working_package"
  [[ -s $working_package && ! -L $working_package ]] || {
    echo "hdiutil did not create a regular DMG" >&2
    exit 1
  }
}

attach_dmg() {
  local package=$1
  hdiutil attach "$package" \
    -readonly -nobrowse -mountpoint "$mount_dir" >/dev/null
  mounted=true
}

detach_dmg() {
  local mounted_path=$1
  hdiutil detach "$mounted_path" >/dev/null
  mounted=false
}

write_build_info() {
  local package=$1
  local runner_os
  runner_os="macOS $(sw_vers -productVersion)"
  cmake \
    "-DZZ_PACKAGE_PATH=$package" \
    "-DZZ_PLATFORM_ID=$platform_id" \
    "-DZZ_COMMIT=$commit" \
    -DZZ_DIRTY=false \
    "-DZZ_BUILT_AT_UTC=$built_at_utc" \
    "-DZZ_RUNNER_OS=$runner_os" \
    "-DZZ_ARCHITECTURE=$architecture" \
    "-DZZ_QT_VERSION=$qt_version" \
    "-DZZ_COMPILER_ID=$compiler_id" \
    "-DZZ_COMPILER_VERSION=$compiler_version" \
    "-DZZ_PRESET=$preset" \
    -DZZ_LINKAGE=shared \
    -DZZ_LTO=true \
    -P "$source_dir/scripts/package/WriteBuildInfo.cmake"
}

if [[ $architecture == arm64 ]]; then
  platform_id=macos-arm64
else
  platform_id=macos-x86_64
fi
short_commit=${commit:0:12}
package_name="ZzPureToolsExample-continuous-${platform_id}-${short_commit}.dmg"
final_package="$output_dir/$package_name"
final_checksum="$final_package.sha256"
final_build_info="$output_dir/build-info.json"
work_dir=$(mktemp -d "$output_dir/.macos-package.XXXXXX")
install_root="$work_dir/install"
mount_dir="$work_dir/mount"
working_package="$work_dir/$package_name"
mkdir -p "$install_root" "$mount_dir"

cleanup() {
  local status=$?
  trap - EXIT
  if [[ $mounted == true ]]; then
    if hdiutil detach -force "$mount_dir" >/dev/null 2>&1; then
      mounted=false
    else
      echo "failed to detach DMG mount: $mount_dir" >&2
      status=1
    fi
  fi
  if [[ $mounted == true ]]; then
    echo "preserving work directory for active mount: $work_dir" >&2
  else
    case "$work_dir/" in
      "$output_dir"/.macos-package.*) rm -rf -- "$work_dir" ;;
      *)
        echo "refusing to clean unexpected work directory: $work_dir" >&2
        status=1
        ;;
    esac
  fi
  if [[ $published != true ]]; then
    rm -f -- "$final_package" "$final_checksum" "$final_build_info"
  fi
  exit "$status"
}
trap cleanup EXIT

install_component Runtime
install_component ExampleRuntime

app_bundle="$install_root/ZzPureToolsExample.app"
app_executable="$app_bundle/Contents/MacOS/ZzPureToolsExample"
app_resources="$app_bundle/Contents/Resources"
[[ -d $app_bundle && -x $app_executable && -d $install_root/lib ]] || {
  echo "installed macOS application payload is incomplete" >&2
  exit 1
}

stage_first_party_libraries "$app_bundle"
stage_offscreen_plugin "$app_bundle"
invoke_macdeployqt "$app_bundle"
strip_transient_rpaths "$app_bundle"
audit_app_bundle "$app_bundle"
invoke_app_smoke "$app_executable"
stage_runtime_licenses "$app_bundle"
create_dmg "$app_bundle"
attach_dmg "$working_package"

mounted_executable="$mount_dir/ZzPureToolsExample.app/Contents/MacOS/ZzPureToolsExample"
invoke_app_smoke "$mounted_executable"
detach_dmg "$mount_dir"
write_build_info "$working_package"

mv -- "$working_package.sha256" "$final_checksum"
mv -- "$work_dir/build-info.json" "$final_build_info"
mv -- "$working_package" "$final_package"
published=true
echo "PASS macOS $architecture DMG: $final_package"
