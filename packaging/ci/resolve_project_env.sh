# 导出 packaging/ci/project.json 中的项目名等变量，供打包脚本使用
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZZ_PROJECT_NAME="$(python3 "${SCRIPT_DIR}/project_config.py" --print name)"
ZZ_EXAMPLE_NAME="$(python3 "${SCRIPT_DIR}/project_config.py" --print example)"
export ZZ_PROJECT_NAME ZZ_EXAMPLE_NAME
