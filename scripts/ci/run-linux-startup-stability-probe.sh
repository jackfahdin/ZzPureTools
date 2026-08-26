#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"

fail() {
  local message=$1
  local exit_code=${2:-1}
  echo "startup stability probe error: $message" >&2
  exit "$exit_code"
}

if ((EUID == 0)); then
  fail "must run as the invoking non-root user" 64
fi
if (($# != 2)) || [[ $1 != --phase ]]; then
  fail "usage: $0 --phase control|performance" 64
fi
phase=$2
case "$phase" in
  control|performance) ;;
  *) fail "phase must be control or performance" 64 ;;
esac

for tool in Xvfb awk cmake git grep jq realpath seq sha256sum taskset xdpyinfo; do
  command -v "$tool" >/dev/null 2>&1 \
    || fail "required tool is unavailable: $tool" 69
done

tracked_status=$(git status --porcelain=v1 --untracked-files=no)
[[ -z "$tracked_status" ]] \
  || fail "tracked worktree must be clean before collecting evidence" 65
head_commit=$(git rev-parse --verify 'HEAD^{commit}')
[[ "$head_commit" =~ ^[0-9a-f]{40}$ ]] \
  || fail "HEAD must resolve to a full lowercase commit"

profile="$source_dir/docs/performance/profiles/local-release-xvfb.json"
baseline="$source_dir/docs/performance/reference/linux/startup.json"
thresholds="$source_dir/docs/performance/reference/linux/regression-thresholds.json"
state_library="$source_dir/scripts/ci/ZzLinuxGovernorState.sh"
for input_file in "$profile" "$baseline" "$thresholds" "$state_library"; do
  [[ -f "$input_file" && -r "$input_file" && ! -L "$input_file" ]] \
    || fail "required input must be a readable regular non-symlink file: $input_file" 66
done

jq -e '
  .schemaVersion == 1 and
  .profileId == "local-release-xvfb" and
  .status == "selected" and
  (.execution.xvfbLogicalCpu | type) == "number" and
  (.execution.benchmarkLogicalCpu | type) == "number" and
  (.execution.cpuAffinityRequired == true) and
  (.xvfbArguments | type) == "array" and
  (.xvfbArguments | length) > 0 and
  all(.xvfbArguments[]; type == "string" and length > 0) and
  (.display.rendererIdentity | type) == "string" and
  (.display.rendererIdentity | length) > 0 and
  (.toolchain.qt | type) == "string" and
  (.toolchain.compiler | type) == "string" and
  (.configuration.gxxPath | type) == "string" and
  (.configuration.qtRoot | type) == "string"
' "$profile" >/dev/null || fail "performance profile schema is invalid" 66

xvfb_cpu=$(jq -er '.execution.xvfbLogicalCpu | tostring' "$profile")
benchmark_cpu=$(jq -er '.execution.benchmarkLogicalCpu | tostring' "$profile")
[[ "$xvfb_cpu" =~ ^[0-9]+$ && "$benchmark_cpu" =~ ^[0-9]+$ \
   && "$xvfb_cpu" != "$benchmark_cpu" ]] \
  || fail "profile CPU affinity is invalid" 66
taskset -c "$xvfb_cpu" true \
  || fail "Xvfb CPU affinity is unavailable: $xvfb_cpu" 69
taskset -c "$benchmark_cpu" true \
  || fail "benchmark CPU affinity is unavailable: $benchmark_cpu" 69

mapfile -t xvfb_arguments < <(jq -er '.xvfbArguments[]' "$profile")
renderer_identity=$(jq -er '.display.rendererIdentity' "$profile")
expected_qt=$(jq -er '.toolchain.qt' "$profile")
expected_compiler=$(jq -er '.toolchain.compiler' "$profile")
expected_gxx=$(jq -er '.configuration.gxxPath' "$profile")
expected_qt_root=$(jq -er '.configuration.qtRoot' "$profile")
profile_digest=$(sha256sum "$profile" | awk '{print $1}')
[[ "$profile_digest" =~ ^[0-9a-f]{64}$ ]] \
  || fail "profile digest is invalid"

cache_file="$source_dir/build/linux-gcc-benchmarks/CMakeCache.txt"
benchmark_binary="$source_dir/build/linux-gcc-benchmarks/benchmarks/ZzStartupBenchmark"
[[ -f "$cache_file" && -r "$cache_file" && ! -L "$cache_file" ]] \
  || fail "benchmark cache is missing: $cache_file" 66
[[ -f "$benchmark_binary" && -x "$benchmark_binary" && ! -L "$benchmark_binary" ]] \
  || fail "startup benchmark is missing: $benchmark_binary" 66
grep -Fx 'ZZ_PERFORMANCE_REFERENCE:BOOL=ON' "$cache_file" >/dev/null \
  || fail "benchmark cache must set ZZ_PERFORMANCE_REFERENCE:BOOL=ON" 66
cache_value() {
  local key=$1
  awk -v key="$key" '
    index($0, key ":") == 1 {
      separator = index($0, "=")
      if (separator > 0) {
        count += 1
        value = substr($0, separator + 1)
      }
    }
    END {
      if (count != 1) {
        exit 1
      }
      print value
    }
  ' "$cache_file"
}
actual_gxx=$(cache_value CMAKE_CXX_COMPILER) \
  || fail "benchmark cache has no unique CXX compiler" 66
[[ "$actual_gxx" == "$expected_gxx" ]] \
  || fail "benchmark cache compiler does not match the profile" 66
actual_qt_dir=$(cache_value Qt6_DIR) \
  || fail "benchmark cache has no unique Qt6 directory" 66
[[ "$actual_qt_dir" == "$expected_qt_root/lib/cmake/Qt6" ]] \
  || fail "benchmark cache Qt root does not match the profile" 66

export ZZ_CMAKE_PRESET=linux-gcc-benchmarks
export ZZ_BENCHMARK_COMMIT="$head_commit"
export ZZ_RUNNER_IMAGE_DIGEST="sha256:$profile_digest"
export ZZ_GPU_IDENTITY="$renderer_identity"
export QT_QPA_PLATFORM=xcb

# shellcheck source=/dev/null
source "$state_library"
sysfs_root=/sys/devices/system/cpu
cpu_state=$(
  zz_governor_snapshot "$sysfs_root" "$xvfb_cpu" "$benchmark_cpu"
) || fail "failed to capture CPU state"

experiment_root="$source_dir/build/linux-gcc-benchmarks/governor-experiment"
phase_dir="$source_dir/build/linux-gcc-benchmarks/governor-experiment/${phase}"
mkdir -p -- "$experiment_root" "$phase_dir"
[[ ! -L "$experiment_root" && ! -L "$phase_dir" ]] \
  || fail "experiment output directories must not be symbolic links" 66
resolved_experiment_root=$(realpath -e -- "$experiment_root")
resolved_phase_dir=$(realpath -e -- "$phase_dir")
[[ "$resolved_phase_dir" == "$resolved_experiment_root/$phase" ]] \
  || fail "phase output escaped the experiment root" 66

rounds_file="$phase_dir/rounds.ndjson"
summary_file="$phase_dir/summary.json"
for output_file in "$rounds_file" "$summary_file"; do
  [[ ! -L "$output_file" && ! -d "$output_file" ]] \
    || fail "refusing unsafe output file: $output_file" 66
done
: > "$rounds_file"
if [[ -e "$summary_file" ]]; then
  rm -- "$summary_file"
fi

run_round() (
  local round=$1
  local round_label
  printf -v round_label '%02d' "$round"
  local round_report="$phase_dir/round-${round_label}.json"
  local compare_log="$phase_dir/round-${round_label}.compare.log"
  local xvfb_log="$phase_dir/round-${round_label}.xvfb.log"
  local display_file="$phase_dir/.display-${round_label}"

  local output_file
  for output_file in \
      "$round_report" "$compare_log" "$xvfb_log" "$display_file"; do
    [[ ! -L "$output_file" && ! -d "$output_file" ]] \
      || fail "refusing unsafe round output: $output_file" 66
    if [[ -e "$output_file" ]]; then
      rm -- "$output_file"
    fi
  done

  local xvfb_pid=
  cleanup_xvfb() {
    if [[ -n "$xvfb_pid" ]]; then
      kill "$xvfb_pid" 2>/dev/null || true
      wait "$xvfb_pid" 2>/dev/null || true
    fi
  }
  trap cleanup_xvfb EXIT

  taskset -c "$xvfb_cpu" Xvfb -displayfd 3 "${xvfb_arguments[@]}" \
    3>"$display_file" >"$xvfb_log" 2>&1 &
  xvfb_pid=$!

  local display_number=
  local attempt
  for attempt in $(seq 1 50); do
    if [[ -s "$display_file" ]]; then
      IFS= read -r display_number < "$display_file" || true
      if [[ "$display_number" =~ ^[0-9]+$ ]] \
         && xdpyinfo -display ":$display_number" >/dev/null 2>&1; then
        break
      fi
    fi
    kill -0 "$xvfb_pid" 2>/dev/null \
      || fail "round $round Xvfb exited before becoming ready; see $xvfb_log"
    sleep 0.1
  done
  [[ "$display_number" =~ ^[0-9]+$ ]] \
    && xdpyinfo -display ":$display_number" >/dev/null 2>&1 \
    || fail "round $round Xvfb did not become ready; see $xvfb_log"
  export DISPLAY=":$display_number"

  taskset -c "$benchmark_cpu" "$benchmark_binary" --report "$round_report" \
    || fail "round $round startup benchmark failed"
  [[ -f "$round_report" && -s "$round_report" && ! -L "$round_report" ]] \
    || fail "round $round reporter is absent or unsafe"

  jq -e \
    --arg commit "$head_commit" \
    --arg preset "$ZZ_CMAKE_PRESET" \
    --arg digest "$ZZ_RUNNER_IMAGE_DIGEST" \
    --arg renderer "$renderer_identity" \
    --arg qt "$expected_qt" \
    --arg compiler "$expected_compiler" '
      .schemaVersion == 1 and
      .scenario == "startup" and
      .build.commit == $commit and
      .build.preset == $preset and
      .environment.runnerImageDigest == $digest and
      .environment.gpu == $renderer and
      .environment.qtVersion == $qt and
      .environment.compiler == $compiler and
      .environment.windowSystem == "xcb" and
      (.metrics["external-total"].p50 | type) == "number" and
      (.metrics["external-total"].p95 | type) == "number" and
      (.metrics["external-total"].max | type) == "number" and
      (.metrics["first-paint"].p50 | type) == "number" and
      (.metrics["first-paint"].p95 | type) == "number" and
      (.metrics["first-paint"].max | type) == "number"
    ' "$round_report" >/dev/null \
    || fail "round $round reporter identity or metrics are invalid"

  set +e
  cmake \
    "-DZZ_BASELINE=$baseline" \
    "-DZZ_CURRENT=$round_report" \
    "-DZZ_THRESHOLDS=$thresholds" \
    -P "$source_dir/cmake/ZzComparePerformanceReport.cmake" \
    >"$compare_log" 2>&1
  local comparison_exit_code=$?
  set -e
  if ((comparison_exit_code != 0)); then
    if grep -F 'INVALID ' "$compare_log" >/dev/null; then
      cat "$compare_log" >&2
      fail "round $round comparison was INVALID"
    fi
    if ! grep -F 'FAIL startup:' "$compare_log" >/dev/null; then
      cat "$compare_log" >&2
      fail "round $round comparison failed without a gate verdict"
    fi
  fi

  jq -cn \
    --argjson round "$round" \
    --arg report "$round_report" \
    --argjson comparison_exit_code "$comparison_exit_code" \
    --slurpfile reporter "$round_report" '
      {
        round: $round,
        report: $report,
        comparisonExitCode: $comparison_exit_code,
        externalTotal: $reporter[0].metrics["external-total"]
          | {p50, p95, max},
        firstPaint: $reporter[0].metrics["first-paint"]
          | {p50, p95, max}
      }
    ' >> "$rounds_file"
)

for round in $(seq 1 10); do
  echo "startup stability probe: phase=$phase round=$round/10"
  run_round "$round"
done

zz_governor_verify \
  "$sysfs_root" "$cpu_state" "$xvfb_cpu" "$benchmark_cpu" \
  || fail "CPU state changed while running the ordinary-user probe"
rounds_json=$(jq -s '.' "$rounds_file")
passed_rounds=$(jq '[.[] | select(.comparisonExitCode == 0)] | length' \
  <<<"$rounds_json")
failed_rounds=$(jq '[.[] | select(.comparisonExitCode != 0)] | length' \
  <<<"$rounds_json")

jq -n \
  --arg phase "$phase" \
  --arg head "$head_commit" \
  --arg profile "$profile" \
  --arg profile_digest "sha256:$profile_digest" \
  --argjson cpu_state "$cpu_state" \
  --argjson rounds "$rounds_json" \
  --argjson passed_rounds "$passed_rounds" \
  --argjson failed_rounds "$failed_rounds" '
    {
      schemaVersion: 1,
      phase: $phase,
      head: $head,
      profile: {path: $profile, digest: $profile_digest},
      cpuState: $cpu_state,
      rounds: $rounds,
      passedRounds: $passed_rounds,
      failedRounds: $failed_rounds
    }
  ' > "$summary_file"

jq -e \
  --arg phase "$phase" \
  --arg head "$head_commit" '
    .schemaVersion == 1 and
    .phase == $phase and
    .head == $head and
    (.rounds | length) == 10 and
    (.passedRounds + .failedRounds) == 10 and
    (.cpuState.cpus | type) == "object"
  ' "$summary_file" >/dev/null \
  || fail "summary validation failed"

echo "PASS startup stability probe phase=$phase passed=$passed_rounds failed=$failed_rounds"
