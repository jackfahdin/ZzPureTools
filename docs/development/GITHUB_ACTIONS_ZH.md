# GitHub Actions 持续集成手册

## 当前状态

`.github/workflows/ci.yml` 已建立，但在仓库首次推送到 GitHub 并产生真实运行记录前，状态只能写为“待首次运行”。工作流通过本地语法和静态契约不等于 GitHub 托管 runner 已经通过，也不能据此修改 `PLATFORM_SUPPORT_ZH.md` 中的原生平台状态。

该工作流只执行配置、编译、静态分析、CTest、安装消费、重定位和二进制依赖检查。它不发布包、不创建 tag、不上传可分发二进制，也不启用 `ZZ_RELEASE_BUILD=ON`。正式发布仍要求仓库外合规证据和人工真机清单。

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
| `linux` | `ubuntu-24.04` | Qt 6.8.3、GCC 14、Clang 18、offscreen | GCC Debug/Release、shared/static/LTO、clang-tidy shared/static、ASan+UBSan |
| `windows-msvc` | `windows-2022` | Qt 6.8.3 MSVC 2022 x64 | MSVC shared/static、`/analyze`、LTO、dumpbin ABI 检查 |
| `windows-mingw` | `windows-2022` | Qt 6.8.3 MinGW 13.1.0 | 官方 Qt MinGW shared/static、LTO、objdump ABI 检查 |
| `macos arm64` | `macos-15` | Qt 6.8.3、Apple Clang、LLVM 18 tidy | arm64 shared/static、LTO、clang-tidy、`lipo` 精确架构检查 |
| `macos x86_64` | `macos-15-intel` | Qt 6.8.3、Apple Clang、LLVM 18 tidy | x86_64 shared/static、LTO、clang-tidy、`lipo` 精确架构检查 |

Qt 模块显式包含 `qtsvg` 和提供 `Qt6::LinguistTools` 的 `qttools`。Windows MinGW job 还从同一 Qt SDK 安装 `tools_mingw1310` 和 `tools_ninja`，然后用 `Assert-QtMinGWKit.ps1` 验证 target triple、GCC 精确版本、qmake prefix 和 xspec，禁止混用 MSVC Qt 或系统 MinGW。

## 与本机性能档案的边界

GitHub 托管机器的 CPU、内存、GPU、负载和镜像会变化，不能执行或更新 `local-release-xvfb` 的绝对性能基线。因此托管 CI 不运行 `linux-gcc-benchmarks`、不设置 `ZZ_PERFORMANCE_REFERENCE=ON`，也不修改 `docs/performance/reference/linux/`。

本机 `run-linux-gates.sh` 继续是当前活动性能参考门禁。`ubuntu2204-github-ci` 仍是独立的未来兼容参考档案；普通 `ubuntu-24.04` CI 通过不能替代该档案的 immutable image digest 和性能审核。

## 首次上传后的处理

1. 在 GitHub Actions 页面确认所有逻辑矩阵组都实际启动，没有因账户额度或 runner 可用性跳过。
2. 任一 job 失败时下载对应的 `*-failure-logs` artifact，并保留完整 Actions 日志和运行 URL。
3. 只根据失败平台修改对应代码、Preset、runner 或依赖声明；不得使用 `continue-on-error` 隐藏失败。
4. 修复后重新运行完整 workflow，并把同一提交的全绿结果记录为“GitHub 托管 CI 通过”。只有 runner 的 OS、架构和 ABI 与目标平台行完全一致时，才能进一步提升为“静态验证通过”。Windows Server 2022 不能替代 Windows 10/11，Ubuntu offscreen 也不能替代 KDE/GNOME X11/Wayland 行。
5. Windows、macOS 和 Linux 窗口系统仍须按 `docs/release/` 下的清单真机签署，托管 CI 不得将状态提升为“真机验收通过”。

首次运行不需要仓库 secret。若 Qt 在线归档、GitHub runner 标签或固定 Action 提交失效，应先保存失败证据，再更新依赖；不得临时切到未固定的版本或降低门禁。
