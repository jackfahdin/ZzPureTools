#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <package-root> <qt-runtime-root>" >&2
  exit 64
fi
for tool in file find ldd readelf realpath sed sort strings; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 69
  }
done

source /etc/os-release
[[ "$ID" == ubuntu && "$VERSION_ID" == 22.04 ]] || {
  echo "runtime check must execute on Ubuntu 22.04" >&2
  exit 1
}

package_root=$(realpath "$1")
qt_root=$(realpath "$2")
[[ -d "$package_root" && -d "$qt_root" ]] || {
  echo "package and Qt runtime roots must be directories" >&2
  exit 1
}

find_exact_runtime() {
  local name=$1
  local -a matches=()
  mapfile -d '' matches < <(find "$package_root" -type f -name "$name" -print0)
  [[ ${#matches[@]} -eq 1 ]] || {
    echo "expected exactly one deployed $name, found ${#matches[@]}" >&2
    exit 1
  }
  realpath "${matches[0]}"
}

deployed_stdcxx=$(find_exact_runtime libstdc++.so.6)
deployed_libgcc=$(find_exact_runtime libgcc_s.so.1)
runtime_dir=$(dirname "$deployed_stdcxx")
[[ "$(dirname "$deployed_libgcc")" == "$runtime_dir" ]] || {
  echo "deployed GNU runtimes must be adjacent" >&2
  exit 1
}
for runtime in "$deployed_stdcxx" "$deployed_libgcc"; do
  [[ "$(file -b "$runtime")" == *ELF* ]] || {
    echo "deployed runtime is not ELF: $runtime" >&2
    exit 1
  }
done

qt_lib_dir=$(realpath "$qt_root/lib")
declare -a qt_libraries=()
for component in Core Gui Widgets Svg Concurrent; do
  candidate="$qt_lib_dir/libQt6${component}.so.6"
  [[ -f "$candidate" ]] || {
    echo "missing Qt runtime: $candidate" >&2
    exit 1
  }
  resolved=$(realpath "$candidate")
  [[ "$(file -b "$resolved")" == *ELF* ]] || {
    echo "Qt runtime is not ELF: $resolved" >&2
    exit 1
  }
  qt_libraries+=("$resolved")
done

libc_file=/lib/x86_64-linux-gnu/libc.so.6
[[ -f "$libc_file" && "$(file -b "$libc_file")" == *ELF* ]] || {
  echo "Ubuntu 22.04 libc is unavailable: $libc_file" >&2
  exit 1
}

available_glibc=$(strings "$libc_file" |
  sed -n 's/.*\(GLIBC_[0-9][0-9.]*\).*/\1/p' | sort -Vu)
available_glibcxx=$(strings "$deployed_stdcxx" |
  sed -n 's/.*\(GLIBCXX_[0-9][0-9.]*\).*/\1/p' | sort -Vu)
[[ -n "$available_glibc" && -n "$available_glibcxx" ]] || {
  echo "available GLIBC or GLIBCXX symbol set is empty" >&2
  exit 1
}
max_available_glibc=$(printf '%s\n' "$available_glibc" | tail -n 1)
max_available_glibcxx=$(printf '%s\n' "$available_glibcxx" | tail -n 1)

declare -A seen_elf=()
declare -a scan_set=()
add_elf() {
  local candidate=$1
  local resolved
  resolved=$(realpath "$candidate")
  [[ -f "$resolved" && "$(file -b "$resolved")" == *ELF* ]] || {
    echo "scan candidate is not an ELF file: $candidate" >&2
    exit 1
  }
  if [[ -z "${seen_elf[$resolved]:-}" ]]; then
    seen_elf[$resolved]=1
    scan_set+=("$resolved")
  fi
}

while IFS= read -r -d '' candidate; do
  if [[ "$(file -b "$candidate")" == *ELF* ]]; then
    add_elf "$candidate"
  fi
done < <(find "$package_root" -type f -print0)
for candidate in "${qt_libraries[@]}"; do
  add_elf "$candidate"
done
[[ ${#scan_set[@]} -gt 0 ]] || {
  echo "ELF scan set is empty" >&2
  exit 1
}

version_not_newer() {
  local required=$1
  local available=$2
  [[ "$(printf '%s\n%s\n' "$required" "$available" |
      sort -V | head -n 1)" == "$required" ]]
}

observed_stdcxx=0
observed_libgcc=0
stdcxx_ldd_marker='libstdc++.so.6 =>'
libgcc_ldd_marker='libgcc_s.so.1 =>'
export LD_LIBRARY_PATH="$runtime_dir:$qt_lib_dir"
for binary in "${scan_set[@]}"; do
  version_info=$(readelf --version-info "$binary") || {
    echo "readelf failed for $binary" >&2
    exit 1
  }
  required_glibc=$(printf '%s\n' "$version_info" |
    sed -n 's/.*\(GLIBC_[0-9][0-9.]*\).*/\1/p' | sort -Vu)
  required_glibcxx=$(printf '%s\n' "$version_info" |
    sed -n 's/.*\(GLIBCXX_[0-9][0-9.]*\).*/\1/p' | sort -Vu)
  if [[ -n "$required_glibc" ]]; then
    max_required_glibc=$(printf '%s\n' "$required_glibc" | tail -n 1)
    version_not_newer "$max_required_glibc" "$max_available_glibc" || {
      echo "$binary requires $max_required_glibc, newer than $max_available_glibc" >&2
      exit 1
    }
  fi
  if [[ -n "$required_glibcxx" ]]; then
    max_required_glibcxx=$(printf '%s\n' "$required_glibcxx" | tail -n 1)
    version_not_newer "$max_required_glibcxx" "$max_available_glibcxx" || {
      echo "$binary requires $max_required_glibcxx, newer than $max_available_glibcxx" >&2
      exit 1
    }
  fi

  ldd_output=$(ldd "$binary") || {
    echo "ldd failed for $binary" >&2
    exit 1
  }
  [[ -n "$ldd_output" ]] || {
    echo "ldd returned empty output for $binary" >&2
    exit 1
  }
  if [[ "$ldd_output" == *"not found"* ]]; then
    echo "unresolved dependency in $binary:" >&2
    echo "$ldd_output" >&2
    exit 1
  fi

  while IFS= read -r line; do
    [[ "$line" == *"$stdcxx_ldd_marker"* ]] || continue
    selected=$(printf '%s\n' "$line" |
      sed -n 's/.*libstdc++\.so\.6 => \([^ ]*\).*/\1/p')
    [[ -n "$selected" ]] || continue
    [[ "$(realpath "$selected")" == "$deployed_stdcxx" ]] || {
      echo "system libstdc++ selected for $binary: $selected" >&2
      exit 1
    }
    observed_stdcxx=$((observed_stdcxx + 1))
  done <<< "$ldd_output"

  while IFS= read -r line; do
    [[ "$line" == *"$libgcc_ldd_marker"* ]] || continue
    selected=$(printf '%s\n' "$line" |
      sed -n 's/.*libgcc_s\.so\.1 => \([^ ]*\).*/\1/p')
    [[ -n "$selected" ]] || continue
    [[ "$(realpath "$selected")" == "$deployed_libgcc" ]] || {
      echo "system libgcc selected for $binary: $selected" >&2
      exit 1
    }
    observed_libgcc=$((observed_libgcc + 1))
  done <<< "$ldd_output"
done

[[ $observed_stdcxx -gt 0 && $observed_libgcc -gt 0 ]] || {
  echo "bundled GNU runtime selection was not observed" >&2
  exit 1
}

echo "Ubuntu 22.04 bundled GNU runtime gate passed for ${#scan_set[@]} ELF files"
