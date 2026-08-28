#!/usr/bin/env bash
set -euo pipefail

source_dir=${1:?source directory is required}
policy="$source_dir/scripts/package/ZzMacosBundlePolicy.sh"
[[ -f $policy && ! -L $policy ]] || {
  echo "macOS bundle policy is unavailable: $policy" >&2
  exit 1
}

test_root=$(mktemp -d "${TMPDIR:-/tmp}/zz-macos-bundle-policy.XXXXXX")
cleanup() {
  rm -rf -- "$test_root"
}
trap cleanup EXIT

fake_bin="$test_root/fake-bin"
mkdir -p "$fake_bin"

cat >"$fake_bin/file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
target=${!#}
case $target in
  *.dylib|*/Contents/MacOS/*) printf '%s\n' 'Mach-O universal binary' ;;
  *) printf '%s\n' 'ASCII text' ;;
esac
EOF

cat >"$fake_bin/lipo" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
if [[ $1 == -archs ]]; then
  if grep -q '^thin:' "$2"; then
    sed -n 's/^thin://p' "$2"
  else
    printf '%s\n' 'x86_64 arm64'
  fi
  exit 0
fi
[[ $# -eq 5 && $2 == -thin && $4 == -output ]] || exit 64
printf 'thin:%s\n' "$3" >"$5"
EOF

cat >"$fake_bin/otool" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 4 && $1 == -arch && $3 == -l ]] || exit 64
while IFS= read -r rpath; do
  printf '%s\n' \
    'Load command 0' \
    '          cmd LC_RPATH' \
    '      cmdsize 96' \
    "         path $rpath (offset 12)"
done <"$4.rpaths"
EOF

cat >"$fake_bin/install_name_tool" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
capture=${ZZ_INSTALL_NAME_CAPTURE:?}
printf '<%s>\n' "$@" >>"$capture"
EOF

chmod +x "$fake_bin/file" "$fake_bin/lipo" "$fake_bin/otool" \
  "$fake_bin/install_name_tool"
PATH="$fake_bin:$PATH"
export PATH

# shellcheck source=/dev/null
source "$policy"

thin_bundle="$test_root/thin/ZzPureToolsExample.app"
thin_binary="$thin_bundle/Contents/PlugIns/platforms/libqcocoa.dylib"
mkdir -p "${thin_binary%/*}"
printf '%s\n' fat >"$thin_binary"
chmod +x "$thin_binary"
mkdir -p "$test_root/thin-work"
zz_macos_thin_bundle "$thin_bundle" arm64 "$test_root/thin-work"
[[ $(<"$thin_binary") == thin:arm64 && -x $thin_binary ]] || {
  echo "fat Mach-O was not reduced to the target architecture" >&2
  exit 1
}

strip_bundle="$test_root/strip/ZzPureToolsExample.app"
strip_binary="$strip_bundle/Contents/Frameworks/libExample.dylib"
staging_root="$test_root/My Workspace/staging"
staging_rpath="$staging_root/install/ZzPureToolsExample.app/Contents/Frameworks"
mkdir -p "${strip_binary%/*}" "$staging_root"
printf '%s\n' thin:arm64 >"$strip_binary"
printf '%s\n' "$staging_rpath" >"$strip_binary.rpaths"
install_name_capture="$test_root/install-name-tool.log"
ZZ_INSTALL_NAME_CAPTURE=$install_name_capture
export ZZ_INSTALL_NAME_CAPTURE
zz_macos_strip_transient_rpaths "$strip_bundle" arm64 \
  "$source_dir" "$source_dir/build/test" "$test_root/Qt" "$staging_root"
expected_install_name_log=$(printf '<%s>\n<%s>\n<%s>' \
  -delete_rpath "$staging_rpath" "$strip_binary")
[[ $(<"$install_name_capture") == "$expected_install_name_log" ]] || {
  echo "staging RPATH with spaces was not passed as one complete argument" >&2
  exit 1
}

smoke_bundle="$test_root/smoke/ZzPureToolsExample.app"
smoke_executable="$smoke_bundle/Contents/MacOS/ZzPureToolsExample"
smoke_plugin_dir="$smoke_bundle/Contents/PlugIns/platforms"
smoke_capture="$test_root/smoke.log"
mkdir -p "${smoke_executable%/*}" "$smoke_plugin_dir"
printf '%s\n' cocoa >"$smoke_plugin_dir/libqcocoa.dylib"
cat >"$smoke_executable" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
{
  printf 'QT_PLUGIN_PATH=%s\n' "${QT_PLUGIN_PATH-<unset>}"
  printf 'QT_QPA_PLATFORM_PLUGIN_PATH=%s\n' "${QT_QPA_PLATFORM_PLUGIN_PATH-<unset>}"
  printf 'QML2_IMPORT_PATH=%s\n' "${QML2_IMPORT_PATH-<unset>}"
  printf 'QML_IMPORT_PATH=%s\n' "${QML_IMPORT_PATH-<unset>}"
  printf 'DYLD_LIBRARY_PATH=%s\n' "${DYLD_LIBRARY_PATH-<unset>}"
  printf 'DYLD_FRAMEWORK_PATH=%s\n' "${DYLD_FRAMEWORK_PATH-<unset>}"
  printf 'DYLD_FALLBACK_LIBRARY_PATH=%s\n' "${DYLD_FALLBACK_LIBRARY_PATH-<unset>}"
  printf 'DYLD_FALLBACK_FRAMEWORK_PATH=%s\n' "${DYLD_FALLBACK_FRAMEWORK_PATH-<unset>}"
  printf 'DYLD_INSERT_LIBRARIES=%s\n' "${DYLD_INSERT_LIBRARIES-<unset>}"
  printf 'QT_QPA_PLATFORM=%s\n' "${QT_QPA_PLATFORM-<unset>}"
  printf 'AUTO_CLOSE=%s\n' "${ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS-<unset>}"
  printf 'ARG=%s\n' "$1"
} >"${ZZ_SMOKE_CAPTURE:?}"
EOF
chmod +x "$smoke_executable"

QT_PLUGIN_PATH=/poison/plugins \
QT_QPA_PLATFORM_PLUGIN_PATH=/poison/platforms \
QML2_IMPORT_PATH=/poison/qml2 \
QML_IMPORT_PATH=/poison/qml \
DYLD_LIBRARY_PATH=/poison/lib \
DYLD_FRAMEWORK_PATH=/poison/frameworks \
DYLD_FALLBACK_LIBRARY_PATH=/poison/fallback-lib \
DYLD_FALLBACK_FRAMEWORK_PATH=/poison/fallback-frameworks \
DYLD_INSERT_LIBRARIES=/poison/injected.dylib \
ZZ_SMOKE_CAPTURE=$smoke_capture \
  zz_macos_invoke_app_smoke "$smoke_bundle"

expected_smoke_log=$(cat <<EOF
QT_PLUGIN_PATH=<unset>
QT_QPA_PLATFORM_PLUGIN_PATH=$smoke_plugin_dir
QML2_IMPORT_PATH=<unset>
QML_IMPORT_PATH=<unset>
DYLD_LIBRARY_PATH=<unset>
DYLD_FRAMEWORK_PATH=<unset>
DYLD_FALLBACK_LIBRARY_PATH=<unset>
DYLD_FALLBACK_FRAMEWORK_PATH=<unset>
DYLD_INSERT_LIBRARIES=<unset>
QT_QPA_PLATFORM=cocoa
AUTO_CLOSE=1500
ARG=--smoke-test
EOF
)
[[ $(<"$smoke_capture") == "$expected_smoke_log" ]] || {
  echo "Cocoa smoke inherited external Qt or DYLD state" >&2
  diff -u <(printf '%s\n' "$expected_smoke_log") "$smoke_capture" >&2 || true
  exit 1
}

rm -- "$smoke_plugin_dir/libqcocoa.dylib" "$smoke_capture"
if ZZ_SMOKE_CAPTURE=$smoke_capture \
    zz_macos_invoke_app_smoke "$smoke_bundle" >/dev/null 2>&1; then
  echo "Cocoa smoke accepted a bundle without libqcocoa.dylib" >&2
  exit 1
fi
[[ ! -e $smoke_capture ]] || {
  echo "application ran despite missing bundled Cocoa plugin" >&2
  exit 1
}

echo "PASS macOS bundle policy"
