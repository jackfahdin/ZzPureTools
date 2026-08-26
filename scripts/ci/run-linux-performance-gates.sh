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

for name in DISPLAY QT_QPA_PLATFORM ZZ_BENCHMARK_COMMIT ZZ_RUNNER_IMAGE_DIGEST \
            ZZ_GPU_IDENTITY; do
  require_env "$name"
done
[[ "$QT_QPA_PLATFORM" == "xcb" ]] || {
  echo "QT_QPA_PLATFORM must be xcb" >&2
  exit 64
}
cache_file="$source_dir/build/linux-gcc-benchmarks/CMakeCache.txt"
[[ -f "$cache_file" ]] || {
  echo "benchmark cache is missing: $cache_file" >&2
  exit 64
}
grep -Fx 'ZZ_PERFORMANCE_REFERENCE:BOOL=ON' "$cache_file" >/dev/null || {
  echo "benchmark cache must set ZZ_PERFORMANCE_REFERENCE:BOOL=ON" >&2
  exit 64
}

performance_scenarios=(
  startup
  theme-switch
  animation
  large-model
  window-lifecycle
  navigation-pane
  idle
  example-startup
  example-navigation
  example-theme-switch
  example-large-model
  example-idle
)
reports_dir="$source_dir/build/linux-gcc-benchmarks/reports"
reports_dir=$(cd "$reports_dir" && pwd -P)

remove_current_report() {
  local report=$1
  local resolved_report
  resolved_report=$(realpath -m -- "$report")
  [[ "$resolved_report" == "$reports_dir"/benchmark.*.json ]] || {
    echo "refusing to remove report outside $reports_dir: $resolved_report" >&2
    exit 64
  }
  if [[ -e "$resolved_report" || -L "$resolved_report" ]]; then
    rm -- "$resolved_report"
  fi
}

log_command() {
  local command_log=$1
  shift
  printf 'command=' >> "$command_log"
  printf '%q ' "$@" >> "$command_log"
  printf '\n' >> "$command_log"
}

for round in 1 2 3; do
  round_dir="$source_dir/build/linux-gcc-benchmarks/release-rounds/round-${round}"
  cmake -E make_directory "$round_dir"
  command_log="$round_dir/commands.log"
  : > "$command_log"
  printf 'round=%s\ncommit=%s\n' "$round" "$ZZ_BENCHMARK_COMMIT" >> "$command_log"

  for scenario in "${performance_scenarios[@]}"; do
    remove_current_report "$reports_dir/benchmark.${scenario}.json"
  done

  ctest_command=(
    taskset -c 10 ctest --preset linux-gcc-benchmarks
    -L benchmark --output-on-failure -j1
  )
  log_command "$command_log" "${ctest_command[@]}"
  "${ctest_command[@]}"

  for scenario in "${performance_scenarios[@]}"; do
    current="$reports_dir/benchmark.${scenario}.json"
    round_report="$round_dir/benchmark.${scenario}.json"
    test -f "$current"
    printf 'report.current=%s\nreport.saved=%s\n' \
      "$current" "$round_report" >> "$command_log"
    cmake -E copy "$current" "$round_report"

    compare_command=(
      cmake
      "-DZZ_BASELINE=$source_dir/docs/performance/reference/linux/${scenario}.json"
      "-DZZ_CURRENT=$round_report"
      "-DZZ_THRESHOLDS=$source_dir/docs/performance/reference/linux/regression-thresholds.json"
      "-DZZ_ABSOLUTE_GATES_VERIFIED=TRUE"
      -P "$source_dir/cmake/ZzComparePerformanceReport.cmake"
    )
    log_command "$command_log" "${compare_command[@]}"
    "${compare_command[@]}"
  done
done

echo "PASS three-round Linux performance gates"
