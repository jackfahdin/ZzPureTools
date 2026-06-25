#!/usr/bin/env python3
"""根据 workflow 手动输入生成 CI 构建矩阵。

项目仅支持 Qt 6.10+。矩阵条目与 Qt 官方预编译包一致：
  Windows x64  : MSVC 2022、MinGW 13.1
  Windows arm64: MSVC 2022（windows-11-arm Runner）
  Linux x64    : GCC x86_64
  Linux arm64  : GCC aarch64
  macOS        : Clang x86_64 / arm64
"""

from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

# 与 packaging/ci/qt-versions.json 保持同步
sys.path.insert(0, str(Path(__file__).resolve().parent))
from qt_versions import minimum_qt_version, parse_qt_version, resolve_qt_version

MIN_QT = minimum_qt_version()


def _bool_env(name: str, default: bool = False) -> bool:
    value = os.environ.get(name, str(default)).strip().lower()
    return value in {"1", "true", "yes", "on"}


def _read_choice(name: str, default: str) -> str:
    return (os.environ.get(name, default) or default).strip().lower()


def _expand_choice(choice: str, universe: tuple[str, ...]) -> set[str]:
    if choice == "none":
        return set()
    if choice == "all":
        return set(universe)
    if choice in universe:
        return {choice}
    print(f"警告: 未知选项 '{choice}'，有效值: none, all, {', '.join(universe)}", file=sys.stderr)
    return set()


def _parse_qt_version(version: str) -> tuple[int, int, int]:
    return parse_qt_version(version)


@dataclass(frozen=True)
class Variant:
    platform: str
    toolchain: str
    arch_key: str
    arch_label: str
    cpu: str
    os: str
    qt_host: str
    qt_arch: str
    display_name: str
    needs_msvc_env: bool = False
    osx_arch: str = ""
    qt_tools: str = ""

    @property
    def platform_id(self) -> str:
        return f"{self.platform}-{self.toolchain}-{self.arch_label}"

    def to_matrix_entry(self) -> dict[str, Any]:
        entry: dict[str, Any] = {
            "os": self.os,
            "platform": self.platform,
            "platform_id": self.platform_id,
            "display_name": self.display_name,
            "toolchain": self.toolchain,
            "qt_host": self.qt_host,
            "qt_arch": self.qt_arch,
            "arch_label": self.arch_label,
            "cpu": self.cpu,
            "needs_msvc_env": self.needs_msvc_env,
        }
        if self.osx_arch:
            entry["osx_arch"] = self.osx_arch
        if self.qt_tools:
            entry["qt_tools"] = self.qt_tools
        return entry


WINDOWS_TOOLCHAINS = ("msvc", "mingw")
WINDOWS_ARCHS = ("x64", "arm64")
LINUX_ARCHS = ("x64", "arm64")
MACOS_ARCHS = ("x64", "arm64")

WINDOWS_VARIANTS: list[Variant] = [
    Variant(
        platform="windows",
        toolchain="msvc",
        arch_key="x64",
        arch_label="x64",
        cpu="x86_64",
        os="windows-latest",
        qt_host="windows",
        qt_arch="win64_msvc2022_64",
        display_name="Windows MSVC x64",
        needs_msvc_env=True,
    ),
    Variant(
        platform="windows",
        toolchain="mingw",
        arch_key="x64",
        arch_label="x64",
        cpu="x86_64",
        os="windows-latest",
        qt_host="windows",
        qt_arch="win64_mingw",
        qt_tools="tools_mingw1310,qt.tools.win64_mingw1310",
        display_name="Windows MinGW x64",
    ),
    Variant(
        platform="windows",
        toolchain="msvc",
        arch_key="arm64",
        arch_label="arm64",
        cpu="aarch64",
        os="windows-11-arm",
        qt_host="windows_arm64",
        qt_arch="win64_msvc2022_arm64",
        display_name="Windows MSVC arm64",
        needs_msvc_env=True,
    ),
]

LINUX_VARIANTS: list[Variant] = [
    Variant(
        platform="linux",
        toolchain="gcc",
        arch_key="x64",
        arch_label="x64",
        cpu="x86_64",
        os="ubuntu-latest",
        qt_host="linux",
        qt_arch="linux_gcc_64",
        display_name="Linux GCC x64",
    ),
    Variant(
        platform="linux",
        toolchain="gcc",
        arch_key="arm64",
        arch_label="arm64",
        cpu="aarch64",
        os="ubuntu-24.04-arm",
        qt_host="linux_arm64",
        qt_arch="linux_gcc_arm64",
        display_name="Linux GCC arm64",
    ),
]

MACOS_VARIANTS: list[Variant] = [
    Variant(
        platform="macos",
        toolchain="clang",
        arch_key="x64",
        arch_label="x64",
        cpu="x86_64",
        os="macos-13",
        qt_host="mac",
        qt_arch="clang_64",
        osx_arch="x86_64",
        display_name="macOS Clang x64 (Intel)",
    ),
    Variant(
        platform="macos",
        toolchain="clang",
        arch_key="arm64",
        arch_label="arm64",
        cpu="aarch64",
        os="macos-latest",
        qt_host="mac",
        qt_arch="clang_64",
        osx_arch="arm64",
        display_name="macOS Clang arm64 (Apple Silicon)",
    ),
]


def build_matrix() -> list[dict[str, Any]]:
    include: list[dict[str, Any]] = []

    build_windows = _bool_env("BUILD_WINDOWS", True)
    build_linux = _bool_env("BUILD_LINUX", True)
    build_macos = _bool_env("BUILD_MACOS", True)

    win_archs = _expand_choice(_read_choice("WINDOWS_ARCH", "x64"), WINDOWS_ARCHS) if build_windows else set()
    win_toolchains = (
        _expand_choice(_read_choice("WINDOWS_TOOLCHAIN", "msvc"), WINDOWS_TOOLCHAINS) if build_windows else set()
    )
    linux_archs = _expand_choice(_read_choice("LINUX_ARCH", "x64"), LINUX_ARCHS) if build_linux else set()
    macos_archs = _expand_choice(_read_choice("MACOS_ARCH", "all"), MACOS_ARCHS) if build_macos else set()

    for variant in WINDOWS_VARIANTS:
        if variant.arch_key in win_archs and variant.toolchain in win_toolchains:
            include.append(variant.to_matrix_entry())

    for variant in LINUX_VARIANTS:
        if variant.arch_key in linux_archs:
            include.append(variant.to_matrix_entry())

    for variant in MACOS_VARIANTS:
        if variant.arch_key in macos_archs:
            include.append(variant.to_matrix_entry())

    return include


def main() -> int:
    try:
        qt_version_str = resolve_qt_version(os.environ.get("QT_VERSION"))
    except SystemExit as exc:
        return int(exc.code) if exc.code is not None else 1

    qt_version = _parse_qt_version(qt_version_str)
    if qt_version < MIN_QT:
        print(
            f"错误: Qt 版本须 >= {MIN_QT[0]}.{MIN_QT[1]}.{MIN_QT[2]}，当前为 "
            f"{qt_version[0]}.{qt_version[1]}.{qt_version[2]}",
            file=sys.stderr,
        )
        return 1

    matrix = build_matrix()
    if not matrix:
        print("错误: 构建矩阵为空，请至少启用一个平台/工具链/架构组合。", file=sys.stderr)
        return 1

    payload = json.dumps({"include": matrix}, ensure_ascii=False)
    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as handle:
            handle.write(f"matrix={payload}\n")
    else:
        print(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
