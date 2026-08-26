#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"

require_env() {
  [[ -n "${!1:-}" ]] || {
    echo "missing environment variable: $1" >&2
    exit 64
  }
}

for name in QT_ROOT GCC_13 GXX_13 GCC_13_TOOLCHAIN_ROOT CLANG_17 CLANGXX_17 \
            ZZ_RUNNER_IMAGE_DIGEST ZZ_GPU_IDENTITY; do
  require_env "$name"
done

export ZZ_BENCHMARK_COMMIT
ZZ_BENCHMARK_COMMIT=$(git rev-parse --verify HEAD)

cmake -DZZ_PRESETS_FILE="$source_dir/CMakePresets.json" \
  -P tests/Platform/PresetMatrixContract.cmake

# 编译器最低版本门禁的负向契约：需要一个必然被拒的旧编译器。
# 未安装 g++-12 的机器跳过（跳过不是通过），安装了就必须验证拒绝行为。
if command -v g++-12 >/dev/null 2>&1; then
  cmake \
    -DZZ_SOURCE_DIR="$source_dir" \
    -DZZ_QT_PREFIX="$QT_ROOT" \
    -DZZ_REJECTED_CXX="$(command -v g++-12)" \
    -DZZ_WORK_DIR="${ZZ_COMPILER_CONTRACT_WORK_DIR:-/tmp/zzpuretoolspro-gcc12-contract}" \
    -P tests/Platform/CompilerCapabilitiesContract.cmake
else
  echo "note: g++-12 unavailable; compiler capabilities contract not executed" >&2
fi

run_preset() {
  local preset=$1
  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure
}

run_preset linux-gcc-debug

for preset in linux-clang-tidy-release linux-clang-tidy-static; do
  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset "$preset"
  cmake --build --preset "$preset" --target ZzClangTidy
  ctest --preset "$preset" --output-on-failure
done

run_preset linux-clang-asan
for preset in linux-gcc-release linux-static-release \
              linux-gcc-release-lto linux-static-release-lto; do
  run_preset "$preset"
done

for tool in Xvfb awk seq taskset xdpyinfo sha256sum realpath grep; do
  command -v "$tool" >/dev/null || {
    echo "required performance tool is unavailable: $tool" >&2
    exit 69
  }
done
profile=docs/performance/profiles/local-release-xvfb.json
profile_digest=$(sha256sum "$profile" | awk '{print $1}')
[[ "$ZZ_RUNNER_IMAGE_DIGEST" == "sha256:${profile_digest}" ]] || {
  echo "ZZ_RUNNER_IMAGE_DIGEST does not identify $profile" >&2
  exit 64
}
taskset -c 8 true
taskset -c 10 true
export DISPLAY=${ZZ_XVFB_DISPLAY:-:99}
export QT_QPA_PLATFORM=xcb
cmake -E make_directory build/linux-gcc-benchmarks
xvfb_log=build/linux-gcc-benchmarks/xvfb.log
taskset -c 8 Xvfb "$DISPLAY" -screen 0 1920x1080x24 -nolisten tcp \
  >"$xvfb_log" 2>&1 &
xvfb_pid=$!
cleanup_xvfb() {
  kill "$xvfb_pid" 2>/dev/null || true
  wait "$xvfb_pid" 2>/dev/null || true
}
trap cleanup_xvfb EXIT
for _ in $(seq 1 50); do
  xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break
  kill -0 "$xvfb_pid" 2>/dev/null || {
    echo "Xvfb exited before becoming ready; see $xvfb_log" >&2
    exit 1
  }
  sleep 0.1
done
xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 || {
  echo "Xvfb did not become ready; see $xvfb_log" >&2
  exit 1
}

cmake --preset linux-gcc-benchmarks -DZZ_PERFORMANCE_REFERENCE=ON
cmake --build --preset linux-gcc-benchmarks
taskset -c 10 ctest --preset linux-gcc-benchmarks \
  -LE benchmark --output-on-failure -j1
scripts/ci/run-linux-performance-gates.sh

cmake --preset linux-clang-asan-benchmarks
cmake --build --preset linux-clang-asan-benchmarks \
  --target ZzWindowLifecycleBenchmark ZzNavigationPaneBenchmark
ctest --preset linux-clang-asan-benchmarks \
  -R '^benchmark\.(window-lifecycle|navigation-pane)$' --output-on-failure
cleanup_xvfb
trap - EXIT

if [[ -z "${ZZ_UBUNTU2204_BUILD_IMAGE:-}" ]]; then
  echo "ubuntu2204-github-ci remains pending-user-validation; optional image not configured"
  exit 0
fi
if [[ ! "$ZZ_UBUNTU2204_BUILD_IMAGE" =~ ^[^[:space:]@]+@sha256:[0-9a-f]{64}$ ]]; then
  echo "ZZ_UBUNTU2204_BUILD_IMAGE must be an immutable image digest" >&2
  exit 64
fi
command -v docker >/dev/null || {
  echo "docker is required" >&2
  exit 69
}
docker pull "$ZZ_UBUNTU2204_BUILD_IMAGE"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e "ZZ_RUNNER_IMAGE_DIGEST=${ZZ_UBUNTU2204_BUILD_IMAGE##*@}" \
  -v "$source_dir:/workspace" \
  -w /workspace \
  "$ZZ_UBUNTU2204_BUILD_IMAGE" \
  bash scripts/ci/run-ubuntu2204-release-gates.sh
