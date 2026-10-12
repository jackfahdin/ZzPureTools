#!/usr/bin/env bash
# 在性能参考机上捕获指定场景的活动基线（逐字复制 reporter 输出，不改数值）。
#
# 用法：
#   scripts/ci/capture-linux-performance-reference.sh [--force] [场景 ...]
#
# 缺省捕获四个后接入的场景：
#   workspace-components fluent-standard-surfaces backdrop radial-gauge-animation
# 基线已存在时拒绝覆盖；确需重建（例如报告指标集合变化）传 --force。
#
# 捕获前会用 linux-gcc-reference preset 完整跑一轮 benchmark 标签测试，
# 其中包含全部 benchmark.reference-gate.* 绝对门禁；绝对门禁不通过则不复制。
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"
source "$source_dir/scripts/ci/ZzPerformanceEnvironment.sh"
zz_require_default_performance_runtime

require_env() {
  [[ -n "${!1:-}" ]] || {
    echo "missing environment variable: $1" >&2
    exit 64
  }
}

for name in DISPLAY QT_QPA_PLATFORM ZZ_RUNNER_IMAGE_DIGEST ZZ_GPU_IDENTITY; do
  require_env "$name"
done
[[ "$QT_QPA_PLATFORM" == "xcb" ]] || {
  echo "QT_QPA_PLATFORM must be xcb" >&2
  exit 64
}
export ZZ_BENCHMARK_COMMIT=${ZZ_BENCHMARK_COMMIT:-$(git rev-parse --verify HEAD)}

force=0
scenarios=()
for arg in "$@"; do
  if [[ "$arg" == "--force" ]]; then
    force=1
  else
    case "$arg" in
      startup|theme-switch|animation|large-model|window-lifecycle|navigation-pane|\
      workspace-components|fluent-standard-surfaces|backdrop|radial-gauge-animation|\
      idle|example-startup|example-navigation|example-theme-switch|example-large-model|example-idle)
        scenarios+=("$arg") ;;
      *) echo "unknown performance scenario: $arg" >&2; exit 64 ;;
    esac
  fi
done
if [[ ${#scenarios[@]} -eq 0 ]]; then
  scenarios=(workspace-components fluent-standard-surfaces backdrop
             radial-gauge-animation)
fi

preset=linux-gcc-reference
build_dir="$source_dir/build/$preset"
reports_dir="$build_dir/reports"
reference_dir="$source_dir/docs/performance/reference/linux"

for scenario in "${scenarios[@]}"; do
  baseline="$reference_dir/$scenario.json"
  if [[ -e "$baseline" && $force -eq 0 ]]; then
    echo "baseline already exists (use --force to rebuild): $baseline" >&2
    exit 64
  fi
done

cmake --preset "$preset" -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF
cmake --build --preset "$preset" --parallel
# Do not accept reports left by an earlier run, even if a producer is removed
# from CTest by mistake. Only the explicitly selected generated files are removed.
for scenario in "${scenarios[@]}"; do
  cmake -E rm -f "$reports_dir/benchmark.$scenario.json"
done
# The threshold contract requires the baselines we are about to create. Exclude
# only that contract during bootstrap; all producers and absolute gates still run.
taskset -c 10 ctest --preset "$preset" -L benchmark \
  -E '^platform.performance-threshold-contract$' --output-on-failure -j1

scenario_list=$(IFS=';'; echo "${scenarios[*]}")
cmake "-DZZ_REPORT_DIR=$reports_dir" "-DZZ_REFERENCE_DIR=$reference_dir" \
  "-DZZ_SCENARIOS=$scenario_list" "-DZZ_COMMIT=$ZZ_BENCHMARK_COMMIT" \
  "-DZZ_FORCE=$force" -P "$source_dir/scripts/ci/ZzCapturePerformanceReference.cmake"

echo "下一步：提交 reference/linux/ 下新增基线，再运行 scripts/ci/run-linux-gates.sh"
