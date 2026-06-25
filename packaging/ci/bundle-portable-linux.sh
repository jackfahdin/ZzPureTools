#!/usr/bin/env bash
# 打包 Linux 可运行测试包
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

find build -name "lib${ZZ_PROJECT_NAME}.so" -exec cp -v {} "${EXAMPLE_DIR}/" \;

if [[ -x "${QT_ROOT}/bin/linuxdeployqt" ]]; then
  DEPLOY_FLAG=--release
  [[ "${BUILD_TYPE}" == "Debug" ]] && DEPLOY_FLAG=--debug
  "${QT_ROOT}/bin/linuxdeployqt" "${BIN}" "${DEPLOY_FLAG}" -bundle-non-qt-libs -verbose=2
else
  echo "未找到 linuxdeployqt，改用手动拷贝 Qt 库"
  mkdir -p "${EXAMPLE_DIR}/plugins/platforms"
  cp -v "${QT_ROOT}/lib"/libQt6Widgets.so* "${EXAMPLE_DIR}/" || true
  cp -v "${QT_ROOT}/lib"/libQt6Gui.so* "${EXAMPLE_DIR}/" || true
  cp -v "${QT_ROOT}/lib"/libQt6Core.so* "${EXAMPLE_DIR}/" || true
  cp -v "${QT_ROOT}/plugins/platforms/libqxcb.so" "${EXAMPLE_DIR}/plugins/platforms/"
fi

cat > "${EXAMPLE_DIR}/使用说明.txt" <<EOF
${ZZ_PROJECT_NAME} 可运行示例包 (Linux)
=============================
在终端执行: ./${ZZ_EXAMPLE_NAME}

平台: ${PLATFORM_LABEL}
架构: ${ARCH_LABEL}
构建类型: ${BUILD_TYPE}
Qt 版本: ${QT_VERSION}

若缺库可执行:
  export LD_LIBRARY_PATH=\$PWD:\$LD_LIBRARY_PATH
EOF

mkdir -p "${DIST_DIR}"
rm -f "${PORTABLE_ZIP}"
(cd "${EXAMPLE_DIR}" && zip -r "${PORTABLE_ZIP}" .)
echo "已生成: ${PORTABLE_ZIP}"
