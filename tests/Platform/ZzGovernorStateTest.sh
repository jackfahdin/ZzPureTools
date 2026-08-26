#!/usr/bin/env bash
set -euo pipefail

source_dir=${1:-}
[[ "$source_dir" == /* && -d "$source_dir" ]] || {
  echo "source directory must be an existing absolute path" >&2
  exit 64
}

state_library="$source_dir/scripts/ci/ZzLinuxGovernorState.sh"
[[ -f "$state_library" ]] || {
  echo "governor state library is missing: $state_library" >&2
  exit 1
}

test_root=$(mktemp -d /tmp/zz-governor-state-test.XXXXXX)
cleanup() {
  rm -rf -- "$test_root"
}
trap cleanup EXIT

# shellcheck source=/dev/null
source "$state_library"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

expect_failure() {
  local label=$1
  shift
  if ("$@" >/dev/null 2>&1); then
    fail "$label unexpectedly succeeded"
  fi
}

write_field() {
  local path=$1
  local value=$2
  mkdir -p -- "$(dirname -- "$path")"
  printf '%s\n' "$value" > "$path"
}

create_cpu() {
  local root=$1
  local cpu=$2
  write_field "$root/cpu${cpu}/online" 1
  write_field "$root/cpu${cpu}/cpufreq/scaling_driver" intel_pstate
  write_field "$root/cpu${cpu}/cpufreq/scaling_governor" powersave
  write_field "$root/cpu${cpu}/cpufreq/energy_performance_preference" performance
  write_field "$root/cpu${cpu}/cpufreq/scaling_min_freq" 800000
  write_field "$root/cpu${cpu}/cpufreq/scaling_max_freq" 5400000
}

reset_fixture() {
  local fixture_root="$test_root/sysfs"
  rm -rf -- "$fixture_root"
  mkdir -p -- "$fixture_root"
  create_cpu "$fixture_root" 8
  create_cpu "$fixture_root" 10
}

fixture_root="$test_root/sysfs"
reset_fixture
snapshot=$(zz_governor_snapshot "$fixture_root" 8 10)
jq -e '
  (.cpus | keys) == ["10", "8"] and
  .cpus["8"] == {
    "online": "1",
    "driver": "intel_pstate",
    "governor": "powersave",
    "epp": "performance",
    "minFrequency": "800000",
    "maxFrequency": "5400000"
  } and
  .cpus["10"].governor == "powersave"
' <<<"$snapshot" >/dev/null || fail "snapshot content is incorrect"

zz_governor_apply "$fixture_root" performance 8 10
[[ $(<"$fixture_root/cpu8/cpufreq/scaling_governor") == performance ]] \
  || fail "CPU 8 governor was not applied"
[[ $(<"$fixture_root/cpu10/cpufreq/scaling_governor") == performance ]] \
  || fail "CPU 10 governor was not applied"
[[ $(<"$fixture_root/cpu8/cpufreq/energy_performance_preference") == performance ]] \
  || fail "apply changed EPP"
[[ $(<"$fixture_root/cpu8/cpufreq/scaling_min_freq") == 800000 ]] \
  || fail "apply changed minimum frequency"
[[ $(<"$fixture_root/cpu8/cpufreq/scaling_max_freq") == 5400000 ]] \
  || fail "apply changed maximum frequency"

zz_governor_restore "$fixture_root" "$snapshot" 8 10
zz_governor_verify "$fixture_root" "$snapshot" 8 10

reset_fixture
write_field "$fixture_root/cpu8/online" 0
expect_failure "offline CPU" zz_governor_snapshot "$fixture_root" 8 10

reset_fixture
write_field "$fixture_root/cpu8/cpufreq/scaling_driver" acpi_cpufreq
expect_failure "unsupported driver" zz_governor_snapshot "$fixture_root" 8 10

reset_fixture
rm -- "$fixture_root/cpu8/cpufreq/energy_performance_preference"
expect_failure "missing field" zz_governor_snapshot "$fixture_root" 8 10

reset_fixture
external_field="$test_root/external-governor"
write_field "$external_field" powersave
rm -- "$fixture_root/cpu8/cpufreq/scaling_governor"
ln -s -- "$external_field" "$fixture_root/cpu8/cpufreq/scaling_governor"
expect_failure "symbolic link field" zz_governor_snapshot "$fixture_root" 8 10

reset_fixture
printf 'performance\nbalanced_performance\n' \
  > "$fixture_root/cpu8/cpufreq/energy_performance_preference"
expect_failure "multiline field" zz_governor_snapshot "$fixture_root" 8 10

reset_fixture
expect_failure "relative sysfs root" zz_governor_snapshot relative/sysfs 8 10
expect_failure "empty CPU list" zz_governor_snapshot "$fixture_root"
expect_failure "non-decimal CPU" zz_governor_snapshot "$fixture_root" cpu8

reset_fixture
write_field "$fixture_root/cpu10/cpufreq/scaling_driver" acpi_cpufreq
expect_failure "apply prevalidation" \
  zz_governor_apply "$fixture_root" performance 8 10
[[ $(<"$fixture_root/cpu8/cpufreq/scaling_governor") == powersave ]] \
  || fail "apply modified CPU 8 before validating CPU 10"

reset_fixture
snapshot=$(zz_governor_snapshot "$fixture_root" 8 10)
write_field "$fixture_root/cpu8/cpufreq/energy_performance_preference" balance_performance
expect_failure "EPP restoration verification" \
  zz_governor_verify "$fixture_root" "$snapshot" 8 10

reset_fixture
snapshot=$(zz_governor_snapshot "$fixture_root" 8 10)
write_field "$fixture_root/cpu8/cpufreq/scaling_governor" performance
expect_failure "governor restoration verification" \
  zz_governor_verify "$fixture_root" "$snapshot" 8 10

expect_failure "direct library execution" bash "$state_library"

echo "PASS governor state transaction behavior"
