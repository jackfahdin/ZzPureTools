#!/usr/bin/env python3
"""CI 使用的 Qt 版本配置（单一数据源：packaging/ci/qt-versions.json）。"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any

CONFIG_PATH = Path(__file__).with_name("qt-versions.json")


def load_config() -> dict[str, Any]:
    with CONFIG_PATH.open(encoding="utf-8") as handle:
        return json.load(handle)


def parse_qt_version(version: str) -> tuple[int, int, int]:
    match = re.match(r"(\d+)\.(\d+)\.(\d+)", version.strip())
    if not match:
        return (0, 0, 0)
    return (int(match.group(1)), int(match.group(2)), int(match.group(3)))


def default_qt_version() -> str:
    return str(load_config()["default"])


def minimum_qt_version() -> tuple[int, int, int]:
    return parse_qt_version(str(load_config()["minimum"]))


def supported_qt_versions() -> list[str]:
    return [str(v) for v in load_config()["supported"]]


def resolve_qt_version(explicit: str | None = None) -> str:
    version = (explicit or "").strip() or default_qt_version()
    supported = supported_qt_versions()
    if version not in supported:
        print(
            f"错误: Qt 版本 '{version}' 不在支持列表中: {', '.join(supported)}",
            file=sys.stderr,
        )
        raise SystemExit(1)

    if parse_qt_version(version) < minimum_qt_version():
        minimum = load_config()["minimum"]
        print(f"错误: Qt 版本须 >= {minimum}", file=sys.stderr)
        raise SystemExit(1)

    return version


def write_github_output(name: str, value: str) -> None:
    github_output = os.environ.get("GITHUB_OUTPUT")
    if not github_output:
        print(f"{name}={value}")
        return
    with open(github_output, "a", encoding="utf-8") as handle:
        handle.write(f"{name}={value}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="解析 CI 使用的 Qt 版本")
    parser.add_argument(
        "--resolve-default",
        action="store_true",
        help="输出默认 Qt 版本到 GITHUB_OUTPUT（qt_version=...）",
    )
    parser.add_argument(
        "--print-default",
        action="store_true",
        help="仅打印默认 Qt 版本",
    )
    args = parser.parse_args()

    if args.resolve_default:
        version = resolve_qt_version()
        write_github_output("qt_version", version)
        print(f"Release 使用 Qt {version}", file=sys.stderr)
        return 0

    if args.print_default:
        print(default_qt_version())
        return 0

    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
