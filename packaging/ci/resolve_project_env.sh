# 导出 packaging/ci/project.json 中的项目名等变量，供打包脚本使用
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 部分环境（如 Windows Git Bash）的 python3 启动器无法正常工作，先验证可用性
if command -v python3 >/dev/null 2>&1 && python3 --version >/dev/null 2>&1; then
  PYTHON_CMD="python3"
elif command -v python >/dev/null 2>&1 && python --version >/dev/null 2>&1; then
  PYTHON_CMD="python"
else
  echo "错误: 未找到可用的 Python 解释器" >&2
  exit 1
fi

export PYTHONUTF8=1
ZZ_PROJECT_NAME="$("$PYTHON_CMD" "${SCRIPT_DIR}/project_config.py" --print name)"
ZZ_EXAMPLE_NAME="$("$PYTHON_CMD" "${SCRIPT_DIR}/project_config.py" --print example)"
ZZ_PROJECT_VERSION="$("$PYTHON_CMD" "${SCRIPT_DIR}/project_config.py" --print version)"
export ZZ_PROJECT_NAME ZZ_EXAMPLE_NAME ZZ_PROJECT_VERSION
