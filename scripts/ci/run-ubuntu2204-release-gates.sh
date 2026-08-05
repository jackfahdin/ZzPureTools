#!/usr/bin/env bash
set -euo pipefail

source /etc/os-release
[[ "$ID" == ubuntu && "$VERSION_ID" == 22.04 ]] || {
  echo "release image must be Ubuntu 22.04" >&2
  exit 1
}

export QT_ROOT=/opt/qt
export GCC_13=/usr/bin/gcc-13
export GXX_13=/usr/bin/g++-13
export GCC_13_TOOLCHAIN_ROOT=/usr
for path in "$GCC_13" "$GXX_13" "$QT_ROOT/bin/qtpaths" \
            /opt/gcc-runtime-licenses/COPYING3 \
            /opt/gcc-runtime-licenses/COPYING.RUNTIME; do
  [[ -f "$path" && -s "$path" ]] || {
    echo "missing image input: $path" >&2
    exit 1
  }
done

gcc_version=$("$GXX_13" -dumpfullversion -dumpversion)
[[ "$(printf '%s\n%s\n' 13.1 "$gcc_version" | sort -V | head -n 1)" == 13.1 ]] || {
  echo "G++ 13.1+ is required, got $gcc_version" >&2
  exit 1
}
qt_version=$("$QT_ROOT/bin/qtpaths" --qt-version)
[[ "$(printf '%s\n%s\n' 6.8 "$qt_version" | sort -V | head -n 1)" == 6.8 ]] || {
  echo "Qt 6.8+ is required, got $qt_version" >&2
  exit 1
}

run_preset() {
  local preset=$1
  shift
  local build_dir="$PWD/build/$preset"
  [[ "$build_dir" == "$PWD/build/"* ]] || {
    echo "unsafe build dir" >&2
    exit 1
  }
  cmake -E remove_directory "$build_dir"
  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON "$@"
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure
}

run_preset linux-gcc-release
run_preset linux-static-release
run_preset linux-static-release-lto
run_preset linux-gcc-release-lto \
  -DZZ_BUNDLE_GNU_RUNTIME=ON \
  -DZZ_GNU_RUNTIME_LICENSE_DIR=/opt/gcc-runtime-licenses

install_root=$PWD/install/ubuntu2204-gcc13-release-lto
[[ "$install_root" == "$PWD/install/"* ]] || {
  echo "unsafe install root" >&2
  exit 1
}
cmake -E remove_directory "$install_root"
cmake --install build/linux-gcc-release-lto --prefix "$install_root"
bash scripts/ci/check-ubuntu2204-runtime.sh "$install_root" "$QT_ROOT"
