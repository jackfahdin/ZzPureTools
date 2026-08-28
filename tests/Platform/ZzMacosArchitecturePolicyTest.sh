#!/usr/bin/env bash
set -euo pipefail

source_dir=${1:?source directory is required}
policy="$source_dir/scripts/package/ZzMacosArchitecturePolicy.sh"
[[ -f $policy && ! -L $policy ]] || {
  echo "macOS architecture policy is unavailable: $policy" >&2
  exit 1
}
# shellcheck source=/dev/null
source "$policy"

zz_macos_arch_list_contains "arm64" arm64
zz_macos_arch_list_contains "x86_64 arm64" arm64
zz_macos_arch_list_contains "x86_64 arm64" x86_64
! zz_macos_arch_list_contains "x86_64" arm64
! zz_macos_arch_list_contains "arm64" x86_64
! zz_macos_arch_list_contains "x86_64arm64" arm64

zz_macos_arch_list_is_exact "arm64" arm64
zz_macos_arch_list_is_exact "x86_64" x86_64
! zz_macos_arch_list_is_exact "x86_64 arm64" arm64
! zz_macos_arch_list_is_exact "x86_64 arm64" x86_64
! zz_macos_arch_list_is_exact "" arm64

echo "PASS macOS architecture policy"
