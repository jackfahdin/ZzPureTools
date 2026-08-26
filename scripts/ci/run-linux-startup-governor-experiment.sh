#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"

fail() {
  local message=$1
  local exit_code=${2:-1}
  echo "governor experiment error: $message" >&2
  exit "$exit_code"
}

if [[ $EUID -ne 0 ]]; then
  fail "must be launched through sudo" 64
fi
if (($# != 0)); then
  fail "this experiment accepts no arguments" 64
fi

for identity_name in SUDO_USER SUDO_UID SUDO_GID; do
  [[ -n ${!identity_name:-} ]] \
    || fail "missing sudo identity: $identity_name" 64
done
[[ "$SUDO_USER" =~ ^[a-z_][a-z0-9_-]*[$]?$ && "$SUDO_USER" != root ]] \
  || fail "SUDO_USER must identify a non-root local account" 64
[[ "$SUDO_UID" =~ ^[0-9]+$ && "$SUDO_GID" =~ ^[0-9]+$ \
   && "$SUDO_UID" -gt 0 && "$SUDO_GID" -gt 0 ]] \
  || fail "sudo UID and GID must be positive decimal values" 64

for tool in awk dirname getent id install jq mktemp powerprofilesctl \
            realpath rm rmdir setpriv stat tee; do
  command -v "$tool" >/dev/null 2>&1 \
    || fail "required root tool is unavailable: $tool" 69
done
actual_uid=$(id -u "$SUDO_USER") \
  || fail "cannot resolve SUDO_USER" 64
actual_gid=$(id -g "$SUDO_USER") \
  || fail "cannot resolve SUDO_USER group" 64
[[ "$actual_uid" == "$SUDO_UID" && "$actual_gid" == "$SUDO_GID" ]] \
  || fail "sudo identity fields do not match the local account" 64
[[ $(stat -c %u "$source_dir") == "$SUDO_UID" ]] \
  || fail "source directory must be owned by the invoking user" 66

passwd_entry=$(getent passwd "$SUDO_USER") \
  || fail "cannot read the invoking user account" 64
IFS=: read -r _ _ _ _ _ invoking_home invoking_shell <<<"$passwd_entry"
[[ "$invoking_home" == /* && -d "$invoking_home" \
   && $(stat -c %u "$invoking_home") == "$SUDO_UID" ]] \
  || fail "invoking user home is invalid" 66
[[ "$invoking_shell" == /* && -x "$invoking_shell" ]] \
  || fail "invoking user shell is invalid" 66

runtime_dir="/run/user/$SUDO_UID"
session_bus="$runtime_dir/bus"
[[ -d "$runtime_dir" && ! -L "$runtime_dir" \
   && $(stat -c %u "$runtime_dir") == "$SUDO_UID" ]] \
  || fail "invoking user runtime directory is unavailable" 66
[[ -S "$session_bus" && $(stat -c %u "$session_bus") == "$SUDO_UID" ]] \
  || fail "invoking user session bus is unavailable" 66

profile="$source_dir/docs/performance/profiles/local-release-xvfb.json"
state_library="$source_dir/scripts/ci/ZzLinuxGovernorState.sh"
probe_script="$source_dir/scripts/ci/run-linux-startup-stability-probe.sh"
benchmark_root="$source_dir/build/linux-gcc-benchmarks"
cache_file="$benchmark_root/CMakeCache.txt"
[[ -d "$benchmark_root" && ! -L "$benchmark_root" \
   && $(stat -c %u "$benchmark_root") == "$SUDO_UID" \
   && $(stat -c %g "$benchmark_root") == "$SUDO_GID" \
   && $(realpath -e -- "$benchmark_root") == "$benchmark_root" ]] \
  || fail "benchmark root must be a real directory owned by the invoking user" 66
for input_file in "$profile" "$state_library" "$probe_script" "$cache_file"; do
  [[ -f "$input_file" && -r "$input_file" && ! -L "$input_file" ]] \
    || fail "required input must be a readable regular non-symlink file: $input_file" 66
done
[[ -x "$probe_script" ]] || fail "ordinary-user probe must be executable" 66

jq -e '
  .schemaVersion == 1 and
  .profileId == "local-release-xvfb" and
  .status == "selected" and
  (.execution.xvfbLogicalCpu | type) == "number" and
  (.execution.benchmarkLogicalCpu | type) == "number" and
  .execution.cpuAffinityRequired == true
' "$profile" >/dev/null || fail "performance profile schema is invalid" 66
xvfb_cpu=$(jq -er '.execution.xvfbLogicalCpu | tostring' "$profile")
benchmark_cpu=$(jq -er '.execution.benchmarkLogicalCpu | tostring' "$profile")
[[ "$xvfb_cpu" =~ ^[0-9]+$ && "$benchmark_cpu" =~ ^[0-9]+$ \
   && "$xvfb_cpu" != "$benchmark_cpu" ]] \
  || fail "profile CPU affinity is invalid" 66
cpus=("$xvfb_cpu" "$benchmark_cpu")

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
cmake_command=$(cache_value CMAKE_COMMAND) \
  || fail "benchmark cache has no unique CMake command" 66
[[ "$cmake_command" == /* && -f "$cmake_command" \
   && -x "$cmake_command" && ! -L "$cmake_command" ]] \
  || fail "cached CMake command is unsafe: $cmake_command" 66
user_path="$(dirname -- "$cmake_command"):/usr/local/bin:/usr/bin:/bin"

run_as_invoking_user() {
  setpriv --reuid "$SUDO_UID" \
    --regid "$SUDO_GID" \
    --init-groups \
    -- /usr/bin/env -i \
    HOME="$invoking_home" \
    USER="$SUDO_USER" \
    LOGNAME="$SUDO_USER" \
    SHELL="$invoking_shell" \
    PATH="$user_path" \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    XDG_RUNTIME_DIR="$runtime_dir" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$session_bus" \
    "$@"
}

power_profile=$(run_as_invoking_user powerprofilesctl get) \
  || fail "cannot read the invoking user's power profile" 69
[[ "$power_profile" =~ ^[a-z][a-z-]*$ ]] \
  || fail "power profile value is invalid: $power_profile" 66

# shellcheck source=/dev/null
source "$state_library"
sysfs_root=/sys/devices/system/cpu
umask 077
transaction_dir=$(mktemp -d /tmp/zz-governor-experiment.XXXXXX)
transaction_log="$transaction_dir/transaction.log"
before_file="$transaction_dir/host-state-before.json"
applied_file="$transaction_dir/host-state-applied.json"
restored_file="$transaction_dir/host-state-restored.json"
verdict_file="$transaction_dir/transaction-verdict.json"
: > "$transaction_log"

log_message() {
  printf '%s\n' "$*" | tee -a "$transaction_log"
}

clear_phase_evidence() {
  local phase_name=$1
  [[ "$phase_name" == control || "$phase_name" == performance ]] \
    || fail "unsupported phase evidence name: $phase_name" 64

  local experiment_root="$benchmark_root/governor-experiment"
  local phase_dir="$experiment_root/$phase_name"
  if [[ -L "$experiment_root" || -e "$experiment_root" && ! -d "$experiment_root" ]]; then
    fail "experiment evidence root is unsafe: $experiment_root" 66
  fi
  [[ ! -L "$experiment_root" ]] \
    || fail "experiment evidence root must not be a symbolic link" 66
  if [[ -e "$experiment_root" ]]; then
    [[ $(realpath -e -- "$experiment_root") == "$experiment_root" \
       && $(stat -c %u "$experiment_root") == "$SUDO_UID" \
       && $(stat -c %g "$experiment_root") == "$SUDO_GID" ]] \
      || fail "experiment evidence root ownership or path is unsafe" 66
  else
    return 0
  fi

  if [[ -L "$phase_dir" || -e "$phase_dir" && ! -d "$phase_dir" ]]; then
    fail "phase evidence directory is unsafe: $phase_dir" 66
  fi
  [[ ! -L "$phase_dir" ]] \
    || fail "phase evidence directory must not be a symbolic link: $phase_dir" 66
  [[ -e "$phase_dir" ]] || return 0
  [[ $(realpath -e -- "$phase_dir") == "$experiment_root/$phase_name" \
     && $(stat -c %u "$phase_dir") == "$SUDO_UID" \
     && $(stat -c %g "$phase_dir") == "$SUDO_GID" ]] \
    || fail "phase evidence directory ownership or path is unsafe: $phase_dir" 66

  local round round_label suffix target
  for round in {1..10}; do
    printf -v round_label '%02d' "$round"
    for suffix in json compare.log xvfb.log; do
      target="$phase_dir/round-${round_label}.${suffix}"
      if [[ -L "$target" || -d "$target" ]]; then
        fail "refusing unsafe phase evidence path: $target" 66
      fi
      if [[ -e "$target" ]]; then
        [[ -f "$target" ]] \
          || fail "refusing non-regular phase evidence path: $target" 66
        rm -- "$target"
      fi
    done

    target="$phase_dir/.display-${round_label}"
    if [[ -L "$target" || -d "$target" ]]; then
      fail "refusing unsafe phase evidence path: $target" 66
    fi
    if [[ -e "$target" ]]; then
      [[ -f "$target" ]] \
        || fail "refusing non-regular phase evidence path: $target" 66
      rm -- "$target"
    fi
  done

  for target in "$phase_dir/rounds.ndjson" "$phase_dir/summary.json"; do
    if [[ -L "$target" || -d "$target" ]]; then
      fail "refusing unsafe phase evidence path: $target" 66
    fi
    if [[ -e "$target" ]]; then
      [[ -f "$target" ]] \
        || fail "refusing non-regular phase evidence path: $target" 66
      rm -- "$target"
    fi
  done
}

state_captured=0
finish_started=0
before_cpu_state=

print_manual_restore() {
  local cpu original_governor governor_path
  local quoted_governor quoted_path
  for cpu in "${cpus[@]}"; do
    original_governor=$(
      _zz_governor_snapshot_value "$before_cpu_state" "$cpu" governor
    ) || continue
    governor_path="$sysfs_root/cpu${cpu}/cpufreq/scaling_governor"
    printf -v quoted_governor '%q' "$original_governor"
    printf -v quoted_path '%q' "$governor_path"
    log_message \
      "manual restore: printf '%s\\n' $quoted_governor > $quoted_path"
  done
}

install_evidence_file() {
  local source_file=$1
  local target_file=$2
  [[ -f "$source_file" && ! -L "$source_file" ]] || return 0
  [[ ! -L "$target_file" && ! -d "$target_file" ]] || return 1
  install -o "$SUDO_UID" -g "$SUDO_GID" -m 0644 \
    "$source_file" "$target_file"
}

copy_transaction_evidence() {
  local evidence_root=
  evidence_root="$benchmark_root/governor-experiment"
  if [[ -e "$evidence_root" ]]; then
    [[ -d "$evidence_root" && ! -L "$evidence_root" \
       && $(stat -c %u "$evidence_root") == "$SUDO_UID" \
       && $(stat -c %g "$evidence_root") == "$SUDO_GID" ]] \
      || return 1
  else
    install -d -o "$SUDO_UID" -g "$SUDO_GID" -m 0755 "$evidence_root" \
      || return 1
  fi

  install_evidence_file \
    "$before_file" "$evidence_root/host-state-before.json" || return
  install_evidence_file \
    "$applied_file" "$evidence_root/host-state-applied.json" || return
  install_evidence_file \
    "$restored_file" "$evidence_root/host-state-restored.json" || return
  install_evidence_file \
    "$verdict_file" "$evidence_root/transaction-verdict.json" || return
  install_evidence_file \
    "$transaction_log" "$evidence_root/transaction.log"
}

cleanup_transaction_dir() {
  local file
  for file in \
      "$before_file" "$applied_file" "$restored_file" \
      "$verdict_file" "$transaction_log"; do
    if [[ -f "$file" && ! -L "$file" ]]; then
      rm -- "$file"
    fi
  done
  rmdir -- "$transaction_dir" 2>/dev/null || true
}

finish() {
  local original_exit=$?
  if ((finish_started != 0)); then
    exit "$original_exit"
  fi
  finish_started=1
  trap - EXIT INT TERM HUP
  set +e

  local final_exit=$original_exit
  local preserve_transaction=0
  local restore_output restore_status verify_status profile_status
  local restored_cpu_state= restored_power_profile=
  if ((state_captured != 0)); then
    log_message "restore: applying original governor values"
    restore_output=$(
      zz_governor_restore \
        "$sysfs_root" "$before_cpu_state" "${cpus[@]}" 2>&1
    )
    restore_status=$?
    if [[ -n "$restore_output" ]]; then
      log_message "$restore_output"
    fi

    zz_governor_verify \
      "$sysfs_root" "$before_cpu_state" "${cpus[@]}" \
      >>"$transaction_log" 2>&1
    verify_status=$?
    restored_cpu_state=$(
      zz_governor_snapshot "$sysfs_root" "${cpus[@]}" 2>>"$transaction_log"
    )
    if [[ -z "$restored_cpu_state" ]]; then
      verify_status=1
    fi
    restored_power_profile=$(
      run_as_invoking_user powerprofilesctl get 2>>"$transaction_log"
    )
    profile_status=$?
    if ((profile_status != 0)) \
       || [[ "$restored_power_profile" != "$power_profile" ]]; then
      profile_status=1
    fi

    if [[ -n "$restored_cpu_state" ]]; then
      jq --arg power_profile "$restored_power_profile" \
        '. + {powerProfile: $power_profile}' \
        <<<"$restored_cpu_state" > "$restored_file"
    fi

    if ((restore_status != 0 || verify_status != 0 || profile_status != 0)); then
      final_exit=70
      preserve_transaction=1
      log_message \
        "restore: FAILED restore=$restore_status verify=$verify_status power-profile=$profile_status"
      print_manual_restore
      log_message "root recovery evidence retained at $transaction_dir"
    else
      jq -e -n \
        --slurpfile before "$before_file" \
        --slurpfile restored "$restored_file" '
          $before[0].cpus == $restored[0].cpus and
          $before[0].powerProfile == $restored[0].powerProfile
        ' >/dev/null
      if (($? != 0)); then
        final_exit=70
        preserve_transaction=1
        log_message "restore: FAILED normalized before/restored mismatch"
        print_manual_restore
        log_message "root recovery evidence retained at $transaction_dir"
      else
        log_message "restore: VERIFIED all CPU fields and power profile"
      fi
    fi
  fi

  copy_transaction_evidence
  if (($? != 0)); then
    final_exit=70
    preserve_transaction=1
    log_message "evidence: FAILED to install transaction evidence"
    log_message "root transaction evidence retained at $transaction_dir"
  elif ((preserve_transaction == 0)); then
    cleanup_transaction_dir
  fi
  exit "$final_exit"
}

trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

log_message "transaction: source=$source_dir user=$SUDO_USER uid=$SUDO_UID"
before_cpu_state=$(
  zz_governor_snapshot "$sysfs_root" "${cpus[@]}"
) || fail "failed to capture original CPU state"
jq --arg power_profile "$power_profile" \
  '. + {powerProfile: $power_profile}' \
  <<<"$before_cpu_state" > "$before_file"
state_captured=1
log_message "transaction: original host state captured"

run_phase() {
  [[ $# -eq 2 && $1 == --phase ]] \
    || fail "internal phase invocation is invalid"
  local phase_name=$2
  clear_phase_evidence "$phase_name"
  log_message "phase: starting $phase_name"
  run_as_invoking_user "$probe_script" "$@" 2>&1 \
    | tee -a "$transaction_log"
  log_message "phase: completed $phase_name"
}

run_phase --phase control

log_message "apply: requesting performance governor for CPUs ${cpus[*]}"
apply_output=$(
  zz_governor_apply "$sysfs_root" performance "${cpus[@]}" 2>&1
) || {
  log_message "$apply_output"
  fail "failed to apply performance governor"
}
[[ -z "$apply_output" ]] || log_message "$apply_output"

applied_cpu_state=$(
  zz_governor_snapshot "$sysfs_root" "${cpus[@]}"
) || fail "failed to capture applied CPU state"
applied_power_profile=$(run_as_invoking_user powerprofilesctl get) \
  || fail "failed to read applied power profile"
jq --arg power_profile "$applied_power_profile" \
  '. + {powerProfile: $power_profile}' \
  <<<"$applied_cpu_state" > "$applied_file"
jq -e -n \
  --slurpfile before "$before_file" \
  --slurpfile applied "$applied_file" '
    all($applied[0].cpus[]; .governor == "performance") and
    $before[0].powerProfile == $applied[0].powerProfile and
    ($before[0].cpus | with_entries(.value |= del(.governor))) ==
      ($applied[0].cpus | with_entries(.value |= del(.governor)))
  ' >/dev/null || fail "applied state changed fields outside governor"
log_message "apply: VERIFIED only governor changed"

run_phase --phase performance

evidence_root="$benchmark_root/governor-experiment"
control_summary="$evidence_root/control/summary.json"
performance_summary="$evidence_root/performance/summary.json"
for summary in "$control_summary" "$performance_summary"; do
  [[ -f "$summary" && -r "$summary" && ! -L "$summary" ]] \
    || fail "phase summary is absent or unsafe: $summary"
  jq -e '(.rounds | length) == 10 and
         (.passedRounds + .failedRounds) == 10' \
    "$summary" >/dev/null || fail "phase summary is incomplete: $summary"
done
control_passed=$(jq -er '.passedRounds' "$control_summary")
performance_passed=$(jq -er '.passedRounds' "$performance_summary")
if ((performance_passed < 10)); then
  verdict=GOVERNOR_INSUFFICIENT
elif ((control_passed < 10)); then
  verdict=GOVERNOR_CANDIDATE
else
  verdict=INCONCLUSIVE
fi

jq -n \
  --arg verdict "$verdict" \
  --argjson control_passed "$control_passed" \
  --argjson performance_passed "$performance_passed" '
    {
      schemaVersion: 1,
      verdict: $verdict,
      control: {passedRounds: $control_passed, totalRounds: 10},
      performance: {passedRounds: $performance_passed, totalRounds: 10}
    }
  ' > "$verdict_file"
log_message \
  "verdict: $verdict control=$control_passed/10 performance=$performance_passed/10"
