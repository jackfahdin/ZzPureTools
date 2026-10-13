#!/usr/bin/env bash
set -euo pipefail
repo=$(realpath "${1:?repository root required}")
script="$repo/scripts/sdk/verify-qt-precision-sdk.sh"
[[ -f "$script" ]] || { echo 'FAIL: SDK verification entry point is missing' >&2; exit 1; }
scratch=$(mktemp -d -t zz-sdk-verify-contract-XXXXXXXX)
trap 'rm -rf -- "$scratch"' EXIT
bash "$script" --help >/dev/null
if bash "$script" --sdk-work-dir "$scratch/missing" --prefix "$scratch/sdk" >"$scratch/log" 2>&1; then
    echo 'FAIL: missing SDK accepted' >&2
    exit 1
fi
grep -Fq 'completed SDK build' "$scratch/log"
if bash "$script" --sdk-work-dir "$scratch/missing" --prefix "$scratch/sdk" --with-multimedia >"$scratch/log" 2>&1; then
    echo 'FAIL: audio verification without framework build accepted' >&2
    exit 1
fi
grep -Fq 'requires --framework-build' "$scratch/log"
printf 'SDK verification prerequisite checks passed.\n'
