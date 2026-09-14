#!/usr/bin/env bash
set -euo pipefail

readonly excluded_labels='benchmark|screenshot|install|packaging|release'
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
preset=
tests=
docs_only=false

usage() {
  cat <<'USAGE'
usage:
  run-local-development-gate.sh --preset <name> --tests <ctest-regex>
  run-local-development-gate.sh --docs-only

supported presets:
  Linux: linux-gcc-debug
  macOS: macos-clang-release-arm64, macos-clang-release-x86_64

The build mode checks whitespace, builds the complete library/test/example
tree, runs ordinary tests, and requires at least one matching focused test.
The documentation mode checks whitespace and runs the documentation audit.
USAGE
}

fail() {
  echo "本机开发门禁失败：$*" >&2
  exit 2
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "缺少命令：$1"
}

while (($# > 0)); do
  case "$1" in
  --preset)
    (($# >= 2)) || fail "--preset 缺少参数"
    preset=$2
    shift 2
    ;;
  --tests)
    (($# >= 2)) || fail "--tests 缺少参数"
    tests=$2
    shift 2
    ;;
  --docs-only)
    docs_only=true
    shift
    ;;
  --help|-h)
    usage
    exit 0
    ;;
  *)
    fail "未知参数：$1"
    ;;
  esac
done

if [[ $docs_only == true ]]; then
  [[ -z $preset && -z $tests ]] || fail "--docs-only 不能与 --preset 或 --tests 同时使用"
else
  [[ -n $preset && -n $tests ]] || fail "构建模式必须同时提供 --preset 和 --tests"
fi

require_command git
require_command cmake

git -C "$source_dir" diff --check
git -C "$source_dir" diff --cached --check

if [[ $docs_only == true ]]; then
  cmake "-DZZ_SOURCE_DIR=$source_dir" \
    -P "$source_dir/tests/Architecture/ZzDocumentationAudit.cmake"
  echo "本机最小门禁已通过：纯文档模式。"
  echo "待验证：不适用；本模式未修改生产代码。"
  exit 0
fi

require_command ctest
host_system=$(uname -s)
host_arch=$(uname -m)
case "$host_system" in
Linux)
  [[ $preset == linux-gcc-debug ]] || fail "当前 Linux 不支持的 preset：$preset"
  ;;
Darwin)
  case "$preset" in
  macos-clang-release-arm64|macos-clang-release-x86_64) ;;
  *) fail "当前 macOS 不支持的 preset：$preset" ;;
  esac
  ;;
*)
  fail "当前宿主不支持 Bash 本机门禁：$host_system"
  ;;
esac

cd "$source_dir"
cmake --preset "$preset" \
  -DZZ_BUILD_TESTS=ON \
  -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_BUILD_BENCHMARKS=OFF \
  -DZZ_WARNINGS_AS_ERRORS=ON
cmake --build --preset "$preset"
ctest --preset "$preset" --output-on-failure -LE "$excluded_labels"

inventory=$(ctest --preset "$preset" -N -R "$tests" -LE "$excluded_labels")
printf '%s\n' "$inventory"
if ! grep -Eq 'Total Tests: [1-9][0-9]*$' <<<"$inventory"; then
  fail "定向测试正则没有匹配普通测试：$tests"
fi
ctest --preset "$preset" --output-on-failure \
  -R "$tests" -LE "$excluded_labels"

cache_file="$source_dir/build/$preset/CMakeCache.txt"
compiler_path=$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$cache_file" | head -n 1)
qt_cmake_dir=$(sed -n 's/^Qt6_DIR:[^=]*=//p' "$cache_file" | head -n 1)
compiler_metadata=$(find "$source_dir/build/$preset/CMakeFiles" \
  -name CMakeCXXCompiler.cmake -type f -print -quit)
compiler_id=
compiler_version=
if [[ -n $compiler_metadata ]]; then
  compiler_id=$(sed -n 's/^set(CMAKE_CXX_COMPILER_ID "\([^"]*\)")/\1/p' \
    "$compiler_metadata")
  compiler_version=$(sed -n 's/^set(CMAKE_CXX_COMPILER_VERSION "\([^"]*\)")/\1/p' \
    "$compiler_metadata")
fi

echo "本机最小门禁已通过：$host_system/$host_arch。"
echo "CMake：$(cmake --version | sed -n '1p')"
echo "Qt：${qt_cmake_dir:-未从 CMakeCache 解析到 Qt6_DIR}"
echo "编译器：${compiler_id:-未知} ${compiler_version:-未知} (${compiler_path:-未知})"
echo "preset：$preset"
echo "定向测试：$tests"
echo "排除标签：$excluded_labels"
echo "待验证：其他平台 CI、ASan/UBSan、clang-tidy、视觉、性能和真机交互。"
