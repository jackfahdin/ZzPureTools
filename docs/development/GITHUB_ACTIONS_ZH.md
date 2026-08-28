# GitHub Actions 持续构建与发布手册

## 当前定义

`.github/workflows/ci.yml` 将 `ZzPureToolsExample` 的验证和可分发包合并为同一条
continuous build 流程。固定下载页为
<https://github.com/jackfahdin/ZzPureTools/releases/tag/continuous-build>。该页面是滚动
更新的 `Pre-release`，不是稳定版本；首次五平台同提交运行全部成功前，不登记远端
通过结论，也不预填运行地址。

工作流在推送到 `master`、pull request 和手工 `workflow_dispatch` 时运行。PR 执行
配置合同、原生构建、CTest、部署审计和包内 smoke，但不上传 Release artifact，也不
进入发布 job。`master` 和手工运行只有在目标 ref 为 `master` 时才允许发布。

## 权限与依赖固定

工作流顶层权限为 `contents: read`。只有 `publish-continuous-build` job 使用
`contents: write`，其余 job 没有仓库写权限；工作流不使用 `pull_request_target`，也
不需要仓库 secret。所有外部 Action 使用 40 位提交，下载工具使用固定 release URL
和硬编码 `SHA-256`，不得改为可移动的 `@v4`、`@main`、`latest` 或 continuous 下载。

Qt 采用集中升级规则：五个平台都读取 workflow 顶层的 `QT_VERSION`，当前是
`6.8.3`。升级 Qt 时只修改这一处版本入口，然后同步审查可用 runner/架构、Qt MinGW
工具链、截图跨 minor 阈值和许可证内容，并重新运行完整五平台工作流。不得分别修改
job 形成不同 Qt 版本，也不得只凭一个平台通过就更新 Release。

## 发布矩阵

| Job | 原生 runner | Preset | 产物与自动验证 |
|---|---|---|---|
| `contracts` | Ubuntu 22.04 | 无编译 preset | Preset、打包脚本、workflow 和发布事务合同 |
| `linux` | Ubuntu 22.04 x86_64 | `linux-continuous-release` | shared/LTO AppImage、ELF 审计、Xvfb smoke |
| `windows-msvc` | Windows Server 2022 x86_64 | `windows-msvc2022-continuous` | MSVC ZIP、PE/runtime 审计、offscreen smoke |
| `windows-mingw` | Windows Server 2022 x86_64 | `windows-mingw-continuous` | Qt MinGW ZIP、PE/runtime 审计、offscreen smoke |
| `macos` / `arm64` | macOS 15 arm64 | `macos-continuous-arm64` | 单架构 DMG、挂载后 smoke、Mach-O 审计 |
| `macos` / `x86_64` | macOS 15 Intel | `macos-continuous-x86_64` | 单架构 DMG、挂载后 smoke、Mach-O 审计 |
| `publish-continuous-build` | Ubuntu 22.04 | 无编译 preset | 聚合五组 artifact 并事务式更新固定 Release |

远端 Linux 只运行实际发布的 `linux-continuous-release`。Debug、static、Clang、
clang-tidy、ASan/UBSan 和性能门禁仍是本机专项 preset，不再宣称由日常 GitHub Linux
job 执行。GitHub 托管机器也不更新 `local-release-xvfb` 性能参考报告。

## Artifact 与 Release 事务

五个平台 job 各自产生一个独立 artifact 目录，其中恰好有：

1. 平台包：AppImage、ZIP 或 DMG。
2. `<package>.sha256`：包的 SHA-256 摘要。
3. `build-info.json`：commit、runner、架构、Qt、编译器、preset、链接方式和 LTO 身份。

发布 job 使用固定提交的 `actions/download-artifact` 下载五组文件。调用 GitHub API 前，
`VerifyArtifactSet.cmake` 必须确认五个平台齐全、commit 完全一致、文件名与架构匹配且
摘要可重新计算。发布时五份 `build-info.json` 重命名为
`<package>.build-info.json`，因此 Release 最终精确包含 5 个包和 15 个互不重名资产。

固定 `continuous-build` tag 的更新顺序如下：

1. 校验本地五平台集合，缺包或摘要错误时不访问 GitHub。
2. 首次运行创建未签名的 `Pre-release`；已有 Release 则先保留旧资产。
3. 上传并从 API 读取全部 15 个新资产，确认每个新资产只出现一次。
4. 更新说明和目标提交，再验证固定 tag 已指向本次 commit。
5. 只有上述步骤全部成功后才删除不属于本提交的旧资产。

因此，上传、API 可见性或 tag 更新失败时保留上一轮完整 Release。并发组
`continuous-build-release` 使用 `cancel-in-progress: false`，避免进入写入阶段的运行被
下一次推送中断。再次运行同一提交时可复用已经完整上传的 15 个资产。

## 安全与验收边界

所有 continuous build 都是未签名开发预览。Windows 和 macOS 可能显示系统安全提示；
项目不得引导用户关闭整机安全机制。下载者应核对相邻 SHA-256 和
`<package>.build-info.json` 中的提交，再决定是否运行。

CI smoke 不等于真机验收。offscreen、Xvfb、DMG 挂载或部署目录启动成功，只能说明该
原生 runner 上的包可以加载和退出，不能证明真实显示器上的鼠标命中、IME、DPI、窗口
材质、系统菜单、拖放和桌面集成。平台状态只能按
`docs/development/PLATFORM_SUPPORT_ZH.md` 和对应人工清单提升。

## 远端失败处理

1. 确认 `contracts`、五个平台通道和 `publish-continuous-build` 都实际启动，没有因额度或 runner 可用性跳过。
2. 任一平台失败时下载其 `*-failure-logs`，保存完整日志和 workflow URL；不得使用 `continue-on-error` 隐藏失败。
3. 只修改失败平台对应的代码、preset、打包脚本或固定依赖，并重新运行完整工作流。
4. 发布 job 失败时先核对旧 Release 仍完整，再检查本次五组 artifact、API 可见性和 tag 更新步骤。
5. 同一 commit 的全部 job 通过后，核对 Release 为 `prerelease=true`、`latest=false`、目标 commit 正确且只有 15 个新资产。
6. 下载五个包并重新计算 SHA-256；完成后把 workflow URL、commit 和五平台结论追加到本文件。

本机只能验证 YAML、脚本和事务合同，不能用 Linux 静态合同替代 Windows 或 macOS 原生
runner 的结果。
