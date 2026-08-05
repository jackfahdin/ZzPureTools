#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"

for name in QT_MACOS_ARM64_ROOT QT_MACOS_X86_64_ROOT \
            APPLE_CLANG APPLE_CLANGXX; do
  [[ -n "${!name:-}" ]] || {
    echo "missing environment variable: $name" >&2
    exit 64
  }
done

presets=(
  macos-clang-release-arm64
  macos-clang-release-x86_64
  macos-clang-static-arm64
  macos-clang-static-x86_64
)
for preset in "${presets[@]}"; do
  expected=arm64
  [[ "$preset" == *x86_64 ]] && expected=x86_64
  qt_var=QT_MACOS_ARM64_ROOT
  [[ "$expected" == x86_64 ]] && qt_var=QT_MACOS_X86_64_ROOT
  qt_core="${!qt_var}/lib/QtCore.framework/QtCore"
  [[ -f "$qt_core" ]] || {
    echo "missing QtCore: $qt_core" >&2
    exit 1
  }
  [[ "$(lipo -archs "$qt_core")" == *"$expected"* ]] || {
    echo "$qt_var does not contain $expected" >&2
    exit 1
  }

  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset "$preset"
  cmake --build --preset "$preset" --target ZzClangTidy
  ctest --preset "$preset" --output-on-failure

  probe_count=0
  probe_path=
  while IFS= read -r candidate; do
    probe_path=$candidate
    probe_count=$((probe_count + 1))
  done < <(find "build/$preset" -type f \
    -name ZzPlatformCompileTest -perm -111)
  [[ $probe_count -eq 1 ]] || {
    echo "expected one platform probe for $preset" >&2
    exit 1
  }
  [[ "$(lipo -archs "$probe_path")" == "$expected" ]] || {
    echo "wrong probe architecture for $preset" >&2
    exit 1
  }
done
