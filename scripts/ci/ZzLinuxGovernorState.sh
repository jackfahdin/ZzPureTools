#!/usr/bin/env bash

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
  echo "ZzLinuxGovernorState.sh must be sourced" >&2
  exit 64
fi

_zz_governor_fail() {
  echo "governor state error: $*" >&2
  return 1
}

_zz_governor_require_jq() {
  command -v jq >/dev/null 2>&1 \
    || _zz_governor_fail "jq is required"
}

_zz_governor_validate_root() {
  local sysfs_root=$1
  [[ "$sysfs_root" == /* && -d "$sysfs_root" && ! -L "$sysfs_root" ]] \
    || _zz_governor_fail \
      "sysfs root must be an existing absolute non-symlink directory: $sysfs_root"
}

_zz_governor_validate_cpus() {
  (($# > 0)) || {
    _zz_governor_fail "at least one CPU is required"
    return
  }

  local cpu
  local -A seen=()
  for cpu in "$@"; do
    [[ "$cpu" =~ ^[0-9]+$ ]] \
      || {
        _zz_governor_fail "CPU must be decimal: $cpu"
        return
      }
    [[ -z ${seen[$cpu]+set} ]] \
      || {
        _zz_governor_fail "duplicate CPU: $cpu"
        return
      }
    seen[$cpu]=1
  done
}

_zz_governor_field_path() {
  local sysfs_root=$1
  local cpu=$2
  local field=$3
  case "$field" in
    online)
      printf '%s/cpu%s/online\n' "$sysfs_root" "$cpu"
      ;;
    driver)
      printf '%s/cpu%s/cpufreq/scaling_driver\n' "$sysfs_root" "$cpu"
      ;;
    governor)
      printf '%s/cpu%s/cpufreq/scaling_governor\n' "$sysfs_root" "$cpu"
      ;;
    epp)
      printf '%s/cpu%s/cpufreq/energy_performance_preference\n' \
        "$sysfs_root" "$cpu"
      ;;
    minFrequency)
      printf '%s/cpu%s/cpufreq/scaling_min_freq\n' "$sysfs_root" "$cpu"
      ;;
    maxFrequency)
      printf '%s/cpu%s/cpufreq/scaling_max_freq\n' "$sysfs_root" "$cpu"
      ;;
    *)
      _zz_governor_fail "unknown field: $field"
      ;;
  esac
}

_zz_governor_read_field() {
  local sysfs_root=$1
  local cpu=$2
  local field=$3
  local path
  path=$(_zz_governor_field_path "$sysfs_root" "$cpu" "$field") \
    || return
  [[ -f "$path" && -r "$path" && ! -L "$path" ]] \
    || {
      _zz_governor_fail "field must be a readable regular non-symlink file: $path"
      return
    }

  local -a lines=()
  mapfile -t lines < "$path" \
    || {
      _zz_governor_fail "failed to read field: $path"
      return
    }
  [[ ${#lines[@]} -eq 1 && -n ${lines[0]} && ${lines[0]} != *$'\r'* ]] \
    || {
      _zz_governor_fail "field must contain exactly one non-empty line: $path"
      return
    }
  printf '%s\n' "${lines[0]}"
}

_zz_governor_validate_token() {
  local label=$1
  local value=$2
  [[ "$value" =~ ^[a-z0-9_-]+$ ]] \
    || _zz_governor_fail "$label has an invalid value: $value"
}

_zz_governor_validate_frequency() {
  local label=$1
  local value=$2
  [[ "$value" =~ ^[0-9]+$ ]] \
    || _zz_governor_fail "$label must be decimal: $value"
}

_zz_governor_validate_current_cpu() {
  local sysfs_root=$1
  local cpu=$2
  local online driver governor epp min_frequency max_frequency
  online=$(_zz_governor_read_field "$sysfs_root" "$cpu" online) || return
  driver=$(_zz_governor_read_field "$sysfs_root" "$cpu" driver) || return
  governor=$(_zz_governor_read_field "$sysfs_root" "$cpu" governor) || return
  epp=$(_zz_governor_read_field "$sysfs_root" "$cpu" epp) || return
  min_frequency=$(
    _zz_governor_read_field "$sysfs_root" "$cpu" minFrequency
  ) || return
  max_frequency=$(
    _zz_governor_read_field "$sysfs_root" "$cpu" maxFrequency
  ) || return

  [[ "$online" == 1 ]] \
    || {
      _zz_governor_fail "CPU $cpu is offline"
      return
    }
  [[ "$driver" == intel_pstate ]] \
    || {
      _zz_governor_fail "CPU $cpu must use intel_pstate: $driver"
      return
    }
  _zz_governor_validate_token "CPU $cpu governor" "$governor" || return
  _zz_governor_validate_token "CPU $cpu EPP" "$epp" || return
  _zz_governor_validate_frequency \
    "CPU $cpu minimum frequency" "$min_frequency" || return
  _zz_governor_validate_frequency \
    "CPU $cpu maximum frequency" "$max_frequency"
}

_zz_governor_snapshot_value() {
  local snapshot_json=$1
  local cpu=$2
  local field=$3
  jq -er --arg cpu "$cpu" --arg field "$field" '
    .cpus[$cpu][$field]
    | select(type == "string" and length > 0)
  ' <<<"$snapshot_json"
}

_zz_governor_validate_snapshot_cpu() {
  local snapshot_json=$1
  local cpu=$2
  local online driver governor epp min_frequency max_frequency
  online=$(_zz_governor_snapshot_value "$snapshot_json" "$cpu" online) \
    || {
      _zz_governor_fail "snapshot is missing CPU $cpu online"
      return
    }
  driver=$(_zz_governor_snapshot_value "$snapshot_json" "$cpu" driver) \
    || {
      _zz_governor_fail "snapshot is missing CPU $cpu driver"
      return
    }
  governor=$(
    _zz_governor_snapshot_value "$snapshot_json" "$cpu" governor
  ) || {
    _zz_governor_fail "snapshot is missing CPU $cpu governor"
    return
  }
  epp=$(_zz_governor_snapshot_value "$snapshot_json" "$cpu" epp) \
    || {
      _zz_governor_fail "snapshot is missing CPU $cpu EPP"
      return
    }
  min_frequency=$(
    _zz_governor_snapshot_value "$snapshot_json" "$cpu" minFrequency
  ) || {
    _zz_governor_fail "snapshot is missing CPU $cpu minimum frequency"
    return
  }
  max_frequency=$(
    _zz_governor_snapshot_value "$snapshot_json" "$cpu" maxFrequency
  ) || {
    _zz_governor_fail "snapshot is missing CPU $cpu maximum frequency"
    return
  }

  [[ "$online" == 1 && "$driver" == intel_pstate ]] \
    || {
      _zz_governor_fail "snapshot CPU $cpu identity is invalid"
      return
    }
  _zz_governor_validate_token \
    "snapshot CPU $cpu governor" "$governor" || return
  _zz_governor_validate_token "snapshot CPU $cpu EPP" "$epp" || return
  _zz_governor_validate_frequency \
    "snapshot CPU $cpu minimum frequency" "$min_frequency" || return
  _zz_governor_validate_frequency \
    "snapshot CPU $cpu maximum frequency" "$max_frequency"
}

_zz_governor_write_governor() {
  local sysfs_root=$1
  local cpu=$2
  local governor=$3
  local path
  path=$(_zz_governor_field_path "$sysfs_root" "$cpu" governor) || return
  [[ -f "$path" && -w "$path" && ! -L "$path" ]] \
    || {
      _zz_governor_fail "governor field must be writable and non-symlink: $path"
      return
    }
  printf '%s\n' "$governor" > "$path" \
    || {
      _zz_governor_fail "failed to write governor: $path"
      return
    }
  local actual
  actual=$(_zz_governor_read_field "$sysfs_root" "$cpu" governor) || return
  [[ "$actual" == "$governor" ]] \
    || _zz_governor_fail \
      "CPU $cpu governor readback mismatch: expected=$governor actual=$actual"
}

zz_governor_snapshot() {
  local sysfs_root=${1:-}
  (($# > 0)) && shift
  _zz_governor_validate_root "$sysfs_root" || return
  _zz_governor_validate_cpus "$@" || return
  _zz_governor_require_jq || return

  local snapshot
  snapshot=$(jq -n '{cpus: {}}') || return
  local cpu online driver governor epp min_frequency max_frequency
  for cpu in "$@"; do
    _zz_governor_validate_current_cpu "$sysfs_root" "$cpu" || return
    online=$(_zz_governor_read_field "$sysfs_root" "$cpu" online) || return
    driver=$(_zz_governor_read_field "$sysfs_root" "$cpu" driver) || return
    governor=$(_zz_governor_read_field "$sysfs_root" "$cpu" governor) || return
    epp=$(_zz_governor_read_field "$sysfs_root" "$cpu" epp) || return
    min_frequency=$(
      _zz_governor_read_field "$sysfs_root" "$cpu" minFrequency
    ) || return
    max_frequency=$(
      _zz_governor_read_field "$sysfs_root" "$cpu" maxFrequency
    ) || return
    snapshot=$(jq -c \
      --arg cpu "$cpu" \
      --arg online "$online" \
      --arg driver "$driver" \
      --arg governor "$governor" \
      --arg epp "$epp" \
      --arg min_frequency "$min_frequency" \
      --arg max_frequency "$max_frequency" '
        .cpus[$cpu] = {
          online: $online,
          driver: $driver,
          governor: $governor,
          epp: $epp,
          minFrequency: $min_frequency,
          maxFrequency: $max_frequency
        }
      ' <<<"$snapshot") || return
  done
  jq -S . <<<"$snapshot"
}

zz_governor_apply() {
  local sysfs_root=${1:-}
  local governor=${2:-}
  (($# >= 2)) && shift 2
  _zz_governor_validate_root "$sysfs_root" || return
  _zz_governor_validate_token governor "$governor" || return
  _zz_governor_validate_cpus "$@" || return

  local cpu
  for cpu in "$@"; do
    _zz_governor_validate_current_cpu "$sysfs_root" "$cpu" || return
  done
  for cpu in "$@"; do
    _zz_governor_write_governor "$sysfs_root" "$cpu" "$governor" || return
  done
}

zz_governor_restore() {
  local sysfs_root=${1:-}
  local snapshot_json=${2:-}
  (($# >= 2)) && shift 2
  _zz_governor_validate_root "$sysfs_root" || return
  _zz_governor_validate_cpus "$@" || return
  _zz_governor_require_jq || return

  local cpu original_governor
  for cpu in "$@"; do
    _zz_governor_validate_snapshot_cpu "$snapshot_json" "$cpu" || return
    _zz_governor_read_field "$sysfs_root" "$cpu" governor >/dev/null || return
  done
  for cpu in "$@"; do
    original_governor=$(
      _zz_governor_snapshot_value "$snapshot_json" "$cpu" governor
    ) || return
    _zz_governor_write_governor \
      "$sysfs_root" "$cpu" "$original_governor" || return
  done
}

zz_governor_verify() {
  local sysfs_root=${1:-}
  local snapshot_json=${2:-}
  (($# >= 2)) && shift 2
  _zz_governor_validate_root "$sysfs_root" || return
  _zz_governor_validate_cpus "$@" || return
  _zz_governor_require_jq || return

  local cpu field expected actual
  for cpu in "$@"; do
    _zz_governor_validate_snapshot_cpu "$snapshot_json" "$cpu" || return
    for field in online driver governor epp minFrequency maxFrequency; do
      expected=$(
        _zz_governor_snapshot_value "$snapshot_json" "$cpu" "$field"
      ) || return
      actual=$(_zz_governor_read_field "$sysfs_root" "$cpu" "$field") \
        || return
      [[ "$actual" == "$expected" ]] \
        || {
          _zz_governor_fail \
            "CPU $cpu $field mismatch: expected=$expected actual=$actual"
          return
        }
    done
  done
}
