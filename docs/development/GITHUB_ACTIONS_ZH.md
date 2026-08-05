# GitHub Actions 持续集成手册

## 当前状态

`.github/workflows/ci.yml` 已在 GitHub 托管 runner 上产生真实运行记录。首次运行暴露了 CMake 3.23 策略声明问题，第二次运行暴露了 aqt 可选模块参数问题，第三次运行已经证明全部平台的 Qt 6.8.3 安装成功，并继续暴露 Qt 6.8 严格告警、MSVC UTF-8 和 MinGW 路径校验问题。当前定义已逐项修复这些问题，在同一提交的完整矩阵变绿前仍不得记为“GitHub 托管 CI 通过”，也不能据此修改 `PLATFORM_SUPPORT_ZH.md` 中的原生平台状态。

macOS 托管 job 使用 macOS 15 runner 构建 deployment target 13.3。Qt 6.8 自身可支持 macOS 12，但 ZzLog 公共 API 使用 `std::format_string`，Apple libc++ 的 C++20 format 运行库要求 deployment target 13.3 或更高；不得通过关闭标准库能力探针伪装 macOS 12 兼容。

该工作流只执行配置、编译、示例构建、静态分析、CTest、安装消费、重定位和二进制依赖检查。Linux 还在 offscreen 平台启动并自动关闭四个示例；Windows 和 macOS 只编译示例，不把托管 runner 上的进程启动视为交互验收。工作流不发布包、不创建 tag、不上传可分发二进制，也不启用 `ZZ_RELEASE_BUILD=ON`。正式发布仍要求仓库外合规证据和人工真机清单。

## 触发条件与权限

工作流在以下情况运行：

- 推送到 `master` 或 `main`。
- 任意 pull request。
- GitHub 页面手工执行 `workflow_dispatch`。

顶层权限固定为 `contents: read`，禁止使用 `pull_request_target`。外部 Action 使用 40 位提交固定，不使用可移动的 `@v4`、`@main` 或 `latest` runner 标签。更新 Action 时必须核对上游来源、替换静态契约中的摘要并重新运行本地契约。

## 托管矩阵

| Job | 固定 runner | Qt/工具链 | 自动覆盖 |
|---|---|---|---|
| `contracts` | `ubuntu-24.04` | runner 自带 CMake | Preset、原生脚本和工作流静态契约 |
| `linux` | `ubuntu-24.04` | Qt 6.8.3、GCC 14、Clang 18、offscreen | GCC Debug/Release、shared/static/LTO、clang-tidy shared/static、ASan+UBSan、四示例编译与冒烟 |
| `windows-msvc` | `windows-2022` | Qt 6.8.3 MSVC 2022 x64 | MSVC shared/static、`/analyze`、LTO、四示例编译、dumpbin ABI 检查 |
| `windows-mingw` | `windows-2022` | Qt 6.8.3 MinGW 13.1.0 | 官方 Qt MinGW shared/static、LTO、四示例编译、objdump ABI 检查 |
| `macos arm64` | `macos-15` | Qt 6.8.3、Apple Clang、LLVM 18 tidy | arm64 shared/static、LTO、clang-tidy、四示例编译、`lipo` 精确架构检查 |
| `macos x86_64` | `macos-15-intel` | Qt 6.8.3、Apple Clang、LLVM 18 tidy | x86_64 shared/static、LTO、clang-tidy、四示例编译、`lipo` 精确架构检查 |

Qt 6.8.3 桌面基础套件已经包含 `QtSvg` 和提供 `Qt6::LinguistTools` 的 Qt Tools 包。`aqtinstall 3.3.0` 的可选模块清单不包含 `qtsvg` 或 `qttools`，因此 Action 不得通过 `modules` 参数重复请求它们；CMake 配置阶段仍会用 `find_package` 验证所需组件确实存在。Windows MinGW job 还从同一 Qt SDK 安装 `tools_mingw1310` 和 `tools_ninja`，然后用 `Assert-QtMinGWKit.ps1` 验证 target triple、GCC 精确版本、qmake prefix 和 xspec，禁止混用 MSVC Qt 或系统 MinGW。

## 与本机性能档案的边界

GitHub 托管机器的 CPU、内存、GPU、负载和镜像会变化，不能执行或更新 `local-release-xvfb` 的绝对性能基线。因此托管 CI 不运行 `linux-gcc-benchmarks`、不设置 `ZZ_PERFORMANCE_REFERENCE=ON`，也不修改 `docs/performance/reference/linux/`。

本机 `run-linux-gates.sh` 继续是当前活动性能参考门禁。`ubuntu2204-github-ci` 仍是独立的未来兼容参考档案；普通 `ubuntu-24.04` CI 通过不能替代该档案的 immutable image digest 和性能审核。

## 远端运行后的处理

1. 在 GitHub Actions 页面确认所有逻辑矩阵组都实际启动，没有因账户额度或 runner 可用性跳过。
2. 任一 job 失败时下载对应的 `*-failure-logs` artifact，并保留完整 Actions 日志和运行 URL。
3. 只根据失败平台修改对应代码、Preset、runner 或依赖声明；不得使用 `continue-on-error` 隐藏失败。
4. 修复后重新运行完整 workflow，并把同一提交的全绿结果记录为“GitHub 托管 CI 通过”。只有 runner 的 OS、架构和 ABI 与目标平台行完全一致时，才能进一步提升为“静态验证通过”。Windows Server 2022 不能替代 Windows 10/11，Ubuntu offscreen 也不能替代 KDE/GNOME X11/Wayland 行。
5. Windows、macOS 和 Linux 窗口系统仍须按 `docs/release/` 下的清单真机签署，托管 CI 不得将状态提升为“真机验收通过”。

该工作流不需要仓库 secret。若 Qt 在线归档、GitHub runner 标签或固定 Action 提交失效，应先保存失败证据，再更新依赖；不得临时切到未固定的版本或降低门禁。
