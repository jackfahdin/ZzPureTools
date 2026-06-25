#!/usr/bin/env bash
# 打包 Linux 完整 SDK
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/resolve_project_env.sh"

PLATFORM_LABEL="${1:?PlatformLabel}"
ARCH_LABEL="${2:?ArchLabel}"
CPU="${3:?Cpu}"
BUILD_TYPE="${4:?BuildType}"
QT_VERSION="${5:?QtVersion}"
SHARED="${6:?Shared}"

INSTALL_ROOT="${GITHUB_WORKSPACE}/install"
DIST_DIR="${GITHUB_WORKSPACE}/dist"
SDK_ZIP="${DIST_DIR}/${ZZ_PROJECT_NAME}-sdk.zip"

if [[ ! -d "${INSTALL_ROOT}" ]]; then
  echo "未找到 SDK 安装目录: ${INSTALL_ROOT}" >&2
  exit 1
fi

cp "${GITHUB_WORKSPACE}/LICENSE" "${INSTALL_ROOT}/"
cp "${GITHUB_WORKSPACE}/packaging/sdk/SDK使用说明.txt" "${INSTALL_ROOT}/"
cp -r "${GITHUB_WORKSPACE}/packaging/sdk/examples" "${INSTALL_ROOT}/"

LIB_TYPE="动态库 (SHARED)"
[[ "${SHARED}" != "ON" ]] && LIB_TYPE="静态库 (STATIC)"

cat > "${INSTALL_ROOT}/BUILD_INFO.txt" <<EOF
${ZZ_PROJECT_NAME} SDK 构建信息
====================
版本: ${ZZ_PROJECT_VERSION}
平台: ${PLATFORM_LABEL}
CPU 架构: ${CPU} (${ARCH_LABEL})
构建类型: ${BUILD_TYPE}
库类型: ${LIB_TYPE}
Qt 版本: ${QT_VERSION}
构建时间(UTC): $(date -u '+%Y-%m-%d %H:%M:%S')
GitHub Run: ${GITHUB_RUN_ID:-local}

目录说明:
  include/   公开头文件
  lib/       lib${ZZ_PROJECT_NAME}.so / .a 与 CMake 包配置
  bin/       运行时库(若安装到 bin)
  examples/  最小集成示例

注意: SDK 不包含 Qt，使用者需自行安装同版本 Qt。
EOF

rm -f "${SDK_ZIP}"
mkdir -p "${DIST_DIR}"
(cd "${INSTALL_ROOT}" && zip -r "${SDK_ZIP}" .)
echo "已生成 SDK: ${SDK_ZIP}"
