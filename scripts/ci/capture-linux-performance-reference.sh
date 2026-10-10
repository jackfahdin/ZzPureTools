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
    scenarios+=("$arg")
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
taskset -c 10 ctest --preset "$preset" -L benchmark --output-on-failure -j1

for scenario in "${scenarios[@]}"; do
  report="$reports_dir/benchmark.$scenario.json"
  [[ -f "$report" ]] || {
    echo "missing report: $report" >&2
    exit 1
  }
  reported=$(cmake -P /dev/stdin <<<"file(READ \"$report\" j); string(JSON s GET \"\${j}\" scenario); message(\"\${s}\")" 2>&1 | tail -1)
  [[ "$reported" == "$scenario" ]] || {
    echo "report scenario mismatch: $reported != $scenario" >&2
    exit 1
  }
  cmake -E copy "$report" "$reference_dir/$scenario.json"
  echo "captured baseline: $reference_dir/$scenario.json"
done

echo "下一步：提交 reference/linux/ 下新增基线，再运行 scripts/ci/run-linux-gates.sh"
