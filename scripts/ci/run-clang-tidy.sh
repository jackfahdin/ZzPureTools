#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
    echo "usage: $0 <source-dir> <build-dir> <run-clang-tidy> <clang-tidy>" >&2
    exit 64
fi

source_dir=$(cd "$1" && pwd -P)
build_dir=$(cd "$2" && pwd -P)
runner=$3
tidy=$4
source_dir_regex=$(printf '%s\n' "$source_dir" \
    | sed 's/[][\\.^$*+?(){}|]/\\&/g')
source_regex="^${source_dir_regex}/((ZzCore|ZzWindowKit|ZzFluentUI|ZzPureTools)/.*|(tests|benchmarks)/.*|ZzThirdParty/ZzLog/src/.*)\\.(cc|cpp|cxx|mm)$"
header_regex="^${source_dir_regex}/((ZzCore|ZzWindowKit|ZzFluentUI|ZzPureTools)/(include|src|foundation|widgets|appcore)|(tests|benchmarks)/.*|ZzThirdParty/ZzLog/(include|src))/.*"

exec "$runner" \
    -quiet \
    -p "$build_dir" \
    -clang-tidy-binary "$tidy" \
    -header-filter "$header_regex" \
    -warnings-as-errors '*' \
    "$source_regex"
