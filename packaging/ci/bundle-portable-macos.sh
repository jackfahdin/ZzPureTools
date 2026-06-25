#!/usr/bin/env bash
# 打包 macOS 可运行测试包
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/resolve_project_env.sh"

BUILD_TYPE="${1:?BuildType}"
PLATFORM_LABEL="${2:?PlatformLabel}"
ARCH_LABEL="${3:?ArchLabel}"
QT_VERSION="${4:?QtVersion}"

QT_ROOT="${QT_ROOT_DIR:?}"
EXAMPLE_DIR="${GITHUB_WORKSPACE}/build/example"
DIST_DIR="${GITHUB_WORKSPACE}/dist"
BIN="${EXAMPLE_DIR}/${ZZ_EXAMPLE_NAME}"
PORTABLE_ZIP="${DIST_DIR}/${ZZ_PROJECT_NAME}-portable.zip"

if [[ ! -x "${BIN}" ]]; then
  echo "未找到示例程序: ${BIN}" >&2
  exit 1
fi

find build -name "lib${ZZ_PROJECT_NAME}.dylib" -exec cp -v {} "${EXAMPLE_DIR}/" \;

MACDEPLOYQT="${QT_ROOT}/bin/macdeployqt"
if [[ ! -x "${MACDEPLOYQT}" ]]; then
  echo "未找到 macdeployqt: ${MACDEPLOYQT}" >&2
  exit 1
fi

DEPLOY_FLAG=--release
[[ "${BUILD_TYPE}" == "Debug" ]] && DEPLOY_FLAG=--debug
"${MACDEPLOYQT}" "${BIN}" "${DEPLOY_FLAG}" -verbose=2

cat > "${EXAMPLE_DIR}/使用说明.txt" <<EOF
${ZZ_PROJECT_NAME} 可运行示例包 (macOS)
=============================
在终端执行: ./${ZZ_EXAMPLE_NAME}

平台: ${PLATFORM_LABEL}
架构: ${ARCH_LABEL}
构建类型: ${BUILD_TYPE}
Qt 版本: ${QT_VERSION}
EOF

mkdir -p "${DIST_DIR}"
rm -f "${PORTABLE_ZIP}"
(cd "${EXAMPLE_DIR}" && zip -r "${PORTABLE_ZIP}" .)
echo "已生成: ${PORTABLE_ZIP}"
