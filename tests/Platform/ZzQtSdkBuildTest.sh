#!/usr/bin/env bash
set -euo pipefail
repo=$(realpath "${1:?repository root required}")
script="$repo/scripts/sdk/build-qt-precision-sdk.sh"
[[ -f "$script" ]] || { echo 'FAIL: SDK build entry point is missing' >&2; exit 1; }
scratch=$(mktemp -d -t zz-sdk-contract-XXXXXXXX)
trap 'rm -rf -- "$scratch"' EXIT
reject() {
    local expected=$1
    shift
    if bash "$script" "$@" >"$scratch/output" 2>&1; then
        echo "FAIL: unexpectedly accepted: $*" >&2
        exit 1
    fi
    if ! grep -Fq -- "$expected" "$scratch/output"; then
        cat "$scratch/output" >&2
        echo "FAIL: expected diagnostic: $expected" >&2
        exit 1
    fi
}
bash "$script" --help >"$scratch/help"
reject 'unknown option' --unknown
reject 'requires a value' --prefix
reject 'positive integer' --work-dir "$scratch/work" --prefix "$scratch/sdk" --jobs 0
reject 'outside the repository' --work-dir "$scratch/work" --prefix "$repo/build/sdk"
reject 'must not overlap' --work-dir "$scratch/work" --prefix "$scratch/work/sdk"
reject 'must not overlap' --work-dir "$scratch/work" --prefix "$scratch"
mkdir "$scratch/existing"
printf 'keep this installation\n' >"$scratch/existing/sentinel"
reject 'not empty' --work-dir "$scratch/work" --prefix "$scratch/existing"
[[ $(<"$scratch/existing/sentinel") == 'keep this installation' ]]
printf 'keep this lock\n' >"$scratch/existing/.zz-sdk-lock"
reject 'not empty' --work-dir "$scratch/work" --prefix "$scratch/existing"
[[ $(<"$scratch/existing/.zz-sdk-lock") == 'keep this lock' ]] || { echo 'FAIL: existing lock was truncated'; exit 1; }
mkdir "$scratch/symlink-install"
ln -s "$scratch/existing/sentinel" "$scratch/symlink-install/.zz-sdk-lock"
reject 'symbolic link' --work-dir "$scratch/work" --prefix "$scratch/symlink-install"
[[ $(<"$scratch/existing/sentinel") == 'keep this installation' ]]
# A corrupt archive must never be unpacked or replaced silently, even in preparation mode.
mkdir -p "$scratch/corrupt/downloads"
printf 'not a Qt archive\n' >"$scratch/corrupt/downloads/qtbase-everywhere-src-6.11.1.tar.xz"
reject 'SHA-256 mismatch' --work-dir "$scratch/corrupt" --prefix "$scratch/new sdk" --prepare-only
[[ ! -e "$scratch/corrupt/src/qtbase" ]]
[[ ! -e "$scratch/new sdk/lib" ]]
[[ -z $(find "$scratch/corrupt" -name complete -print -quit) ]]
printf 'SDK argument, isolation and corrupt-cache checks passed.\n'
