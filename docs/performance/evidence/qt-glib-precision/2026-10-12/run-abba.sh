#!/usr/bin/env bash
# Diagnostic only: run under one Xvfb on CPU 8; benchmark children use CPU 10.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: bash run-abba.sh REPOSITORY PROBE_BUILD NEW_OUTPUT_DIR' >&2; exit 64; }
repo=$(cd "$1" && pwd -P)
probe_build=$(cd "$2" && pwd -P)
output_dir=$3
[[ ! -e "$output_dir" ]] || { echo 'output directory must be new' >&2; exit 64; }
: "${DISPLAY:?run under Xvfb}"
: "${ZZ_BENCHMARK_COMMIT:?set the actual benchmark commit}"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the actual environment identity}"
: "${ZZ_GPU_IDENTITY:?set the actual GPU identity}"
for mode in original candidate; do
    test -f "$probe_build/libglib-dispatch-$mode.so"
done
mkdir -p -- "$output_dir"
output_dir=$(cd "$output_dir" && pwd -P)
unset QT_NO_GLIB QT_XCB_NO_MITSHM LD_PRELOAD ZZ_DIAGNOSTIC_PRECISE_WAIT
export QT_QPA_PLATFORM=xcb ZZ_CMAKE_PRESET=linux-gcc-benchmarks
run=0
for mode in original candidate candidate original; do
    run=$((run + 1))
    for scenario in radial-gauge-animation animation; do
        binary=ZzGaugeAnimationBenchmark
        if [[ $scenario == animation ]]; then binary=ZzAnimationBenchmark; fi
        report="$output_dir/$run-$mode-$scenario.json"
        /usr/bin/time -f 'elapsed=%e user=%U system=%S cpu=%P' -o "$report.time.log" \
          taskset -c 10 env LD_PRELOAD="$probe_build/libglib-dispatch-$mode.so" \
            "$repo/build/linux-gcc-benchmarks/benchmarks/$binary" --report "$report" \
            2>&1 | tee "$report.run.log"
        rg -Fx "diagnostic GLib timer prepare active: $mode" "$report.run.log"
        gate_result=0
        cmake "-DZZ_REPORT=$report" "-DZZ_SCENARIO=$scenario" -DZZ_METRIC=frame-time \
            -DZZ_MAX_P95=16.7 -P "$repo/cmake/ZzVerifyPerformanceReport.cmake" \
            > "$report.gate.log" 2>&1 || gate_result=$?
        printf 'run=%s mode=%s scenario=%s gate=%s\n' "$run" "$mode" "$scenario" "$gate_result"
        jq -c '.metrics["frame-time"]' "$report"
    done
done
