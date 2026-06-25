#!/usr/bin/env python3
"""读取 packaging/ci/project.json（CI 项目名配置）。"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any

CONFIG_PATH = Path(__file__).resolve().with_name("project.json")


def load_config() -> dict[str, Any]:
    with CONFIG_PATH.open(encoding="utf-8") as handle:
        return json.load(handle)


def get_value(key: str) -> str:
    config = load_config()
    if key not in config:
        print(f"错误: project.json 缺少字段 '{key}'", file=sys.stderr)
        raise SystemExit(1)
    return str(config[key])


def write_github_output(name: str, value: str) -> None:
    github_output = os.environ.get("GITHUB_OUTPUT")
    if not github_output:
        print(f"{name}={value}")
        return
    with open(github_output, "a", encoding="utf-8") as handle:
        handle.write(f"{name}={value}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="读取 project.json")
    parser.add_argument("--print", dest="key", metavar="KEY", help="打印指定字段")
    parser.add_argument(
        "--github-output",
        action="store_true",
        help="将 name、example、version 写入 GITHUB_OUTPUT",
    )
    args = parser.parse_args()

    if args.github_output:
        write_github_output("project_name", get_value("name"))
        write_github_output("example_name", get_value("example"))
        write_github_output("project_version", get_value("version"))
        return 0

    if args.key:
        print(get_value(args.key))
        return 0

    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
