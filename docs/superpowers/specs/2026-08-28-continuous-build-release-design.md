# ZzPureToolsExample 持续构建与跨平台发布设计

## 目标

为 `ZzPureToolsExample` 建立类似 VNote `continuous-build` 的自动发布流程。每次
`master` 分支提交通过同一轮跨平台验证后，GitHub Actions 自动更新固定的
`continuous-build` 预发布页面，用户可以直接下载并运行 Linux、Windows 和 macOS
产物，无需在本机重新编译。

本设计同时精简远端 CI：每个平台或 ABI 只验证一个与实际发布产物一致的
Release/shared/LTO 配置。Debug、static、clang-tidy、sanitizer 和性能参考 preset
继续保留给本地开发和专项门禁，但不再组成日常 GitHub Actions 编译矩阵。

## 范围

### 纳入范围

- Ubuntu 22.04 x86_64 AppImage。
- Windows MSVC 2022 x64 ZIP。
- Windows Qt MinGW x64 ZIP。
- macOS arm64 DMG。
- macOS x86_64 DMG。
- Example 的安装规则、应用身份、图标与桌面元数据。
- 部署后的冒烟启动、依赖检查、许可证检查和 SHA-256 清单。
- 固定 `continuous-build` GitHub Pre-release 的事务式更新。
- CI、打包与持续发布的中文维护文档和静态契约测试。

### 不纳入范围

- SDK 二进制包；SDK 继续以源码形式发布。
- Linux ARM64、deb、rpm、Flatpak 或 Snap。
- Windows 安装器、MSIX 或静态 Qt 应用包。
- macOS universal2 合并包。
- Windows Authenticode、macOS Developer ID 签名和 notarization。
- 正式版本 tag 发布；稳定版本以后使用独立的 `vX.Y.Z` 流程。
- 外层旧项目和新仓库之外的任何文件。

## 已确认决策

| 项目 | 决策 |
|---|---|
| 发布入口 | 固定 tag 和 Release：`continuous-build` |
| 发布性质 | GitHub Pre-release，不设置为 Latest |
| 自动触发 | `master` push 成功后自动更新 |
| 手动触发 | `workflow_dispatch` 可重建指定的当前提交 |
| PR 行为 | 完成构建、测试和打包验证，但不得发布 Release |
| Linux 基线 | Ubuntu 22.04 x86_64 |
| 构建类型 | Release、shared、LTO |
| Linux 格式 | AppImage |
| Windows 格式 | MSVC ZIP 与 Qt MinGW ZIP |
| macOS 格式 | arm64 DMG 与 x86_64 DMG |
| 签名 | continuous build 第一阶段不签名，发布说明明确提示 |
| Qt 初始版本 | Qt 6.8.3，集中定义并可审核升级 |
| 发布条件 | 五个产物必须来自同一提交且全部通过验证 |

## 方案选择

采用单一 workflow 的集中发布方案：平台 job 并行生成产物，唯一的 publish job
下载并校验全部产物后更新 Release。

不采用拆分 CI/CD workflow，因为跨 workflow artifact 的来源提交校验、权限和失败
恢复更复杂。不允许各平台直接并发修改 Release，因为这会形成部分发布、资产覆盖
竞争或同一页面混入不同提交的产物。

## Workflow 架构

`.github/workflows/ci.yml` 保持一个工作流，逻辑结构为：

```text
contracts
   |-- linux-x86_64
   |-- windows-msvc-x86_64
   |-- windows-mingw-x86_64
   |-- macos-arm64
   `-- macos-x86_64
             |
             `-- publish-continuous-build
```

`contracts` 只执行快速的 CMake/脚本/workflow 静态合同，不是第二个 Linux 编译
配置。五个平台 job 均依赖 contracts。publish job 依赖五个平台 job，并且只允许在
非 PR 的 `master` 提交上运行。

工作流默认 `contents: read`。只有 publish job 使用 job 级
`contents: write`，通过 GitHub 自动提供的 `GITHUB_TOKEN` 操作 Release，不新增仓库
secret，也不赋予 PR 代码写权限。

同一分支的新构建可以取消仍处于编译阶段的旧构建；进入 Release 更新阶段后不得由
另一轮发布并发写入。发布操作使用独立 concurrency group 串行化。

## 工具链与 Qt 升级

Qt 版本在 workflow 的单一顶层变量中定义。所有平台部署工具必须来自该 job 安装的
同一 Qt SDK：

- Windows 使用对应 SDK 的 `windeployqt`。
- macOS 使用对应 SDK 的 `macdeployqt`。
- Linux Qt 部署插件显式接收同一 `qmake` 路径。

升级 Qt 时不得复用旧缓存生成产物。workflow 缓存键至少包含 OS、架构、Qt 版本、
编译器身份和部署工具版本。

Qt 升级可能同时改变 Qt MinGW 工具 ID、MinGW GCC 版本、macOS 架构支持或 Linux
运行基线。因此 MinGW 工具 ID 与期望 GCC 版本集中放在 Qt 版本旁边，并由
`Assert-QtMinGWKit.ps1` 继续验证。任何不匹配必须在配置或部署阶段失败，不允许自动
改用系统 MinGW、另一套 Qt 或旧缓存。

如果未来 Qt 不再支持 Ubuntu 22.04 或 macOS x86_64，应通过独立设计修改支持矩阵，
不能让 continuous build 静默少发一个产物。

## 平台构建与产物

### Linux x86_64

- Runner：`ubuntu-22.04`。
- 编译器：GCC/G++ 13.1 或更高的 GCC 13 系列。
- Preset：新增或收敛为唯一的 continuous Release/shared/LTO preset。
- 配置必须启用 `ZZ_BUILD_EXAMPLES=ON` 和完整测试。
- AppImage 不捆绑 glibc；捆绑应用实际需要的 Qt、项目动态库、Qt 插件、
  `libstdc++.so.6` 与 `libgcc_s.so.1`。
- 使用现有 ELF/RPATH 和 Ubuntu 22.04 runtime 检查确认没有绝对构建路径、缺失依赖或
  错误加载宿主 GNU runtime。
- 在 Xvfb 中以 AppImage 最终入口运行现有 Example smoke 场景，而不是只运行构建树
  可执行文件。

产物名：

```text
ZzPureToolsExample-continuous-linux-x86_64.AppImage
```

### Windows MSVC x64

- Runner：`windows-2022`。
- Preset：MSVC 2022 Release/shared/LTO，只构建一个链接组合。
- 使用同一 Qt SDK 的 `windeployqt` 部署 Qt DLL、平台插件和编译器运行时。
- 使用 `dumpbin`/PowerShell 检查 DLL 架构、依赖闭包和目录中不存在 MinGW ABI 文件。
- 从部署目录运行现有 offscreen smoke 场景。

产物名：

```text
ZzPureToolsExample-continuous-windows-msvc2022-x86_64.zip
```

### Windows Qt MinGW x64

- Runner：`windows-2022`。
- Preset：Qt 官方 MinGW Release/shared/LTO，只构建一个链接组合。
- Qt SDK、MinGW 与 Ninja 必须通过现有 kit identity 脚本验证。
- 使用同一 SDK 的 `windeployqt` 部署 Qt、MinGW runtime 和插件。
- 使用 `objdump` 检查 PE 架构、依赖闭包和目录中不存在 MSVC ABI 混用。
- 从部署目录运行现有 offscreen smoke 场景。

产物名：

```text
ZzPureToolsExample-continuous-windows-mingw-x86_64.zip
```

### macOS arm64 与 x86_64

- Runner：分别使用 `macos-15` 和 `macos-15-intel`。
- Preset：各自架构的 Apple Clang Release/shared/LTO，只构建 shared 配置。
- `ZzPureToolsExample` 生成规范 `.app`，使用同一 Qt SDK 的 `macdeployqt` 部署 Qt
  frameworks 与插件，再生成 DMG。
- `lipo` 必须确认应用主程序和一方动态库只包含当前 job 的目标架构。
- 从最终 `.app/Contents/MacOS/` 入口运行 offscreen smoke 场景。
- 第一阶段不签名、不 notarize；Release 正文给出 Gatekeeper 提示，不伪装为正式签名
  产物。

产物名：

```text
ZzPureToolsExample-continuous-macos-arm64.dmg
ZzPureToolsExample-continuous-macos-x86_64.dmg
```

## Example 应用身份与安装边界

Example 增加跨平台安装规则，并统一以下身份：

- 可执行目标：`ZzPureToolsExample`。
- 显示名称：`ZzPureToolsExample`。
- 组织名称：`Jackfahdin`。
- Linux desktop id：`io.github.jackfahdin.ZzPureToolsExample`。
- macOS bundle identifier：`io.github.jackfahdin.ZzPureToolsExample`。

Linux 需要仓库受控的 `.desktop`、AppStream/desktop 元数据和可部署图标。Windows
需要 `.ico`，macOS 需要 `.icns`。初版只使用仓库内 Jackfahdin 已授权的项目资源
生成平台变体，不下载第三方图标；资源源文件与生成规则进入版本控制，便于以后直接
替换品牌图标。

Example 的可选 `temp_image/` 仍然是本机预览输入，不得被 CI 读取、打包、上传或
提交。无该目录时使用当前确定性内嵌预览。

## 二进制分发与许可证

continuous build 是预发布，但仍属于二进制分发，不得绕过许可证审计。

- CI 为 `ZZ_RELEASE_BUILD=ON` 准备 `release-evidence.json` 指定的公开外部证据，
  每个文件必须匹配现有 SHA-256。
- 项目 MIT、QWindowKit、qmsetup/syscmdline 通知、ZzLog、spdlog、fmt、GNU runtime
  和实际部署的 Qt 模块许可证随产物安装。
- Qt 动态部署包记录 Qt 版本、模块、许可证和对应源码获取地址。
- Linux 捆绑 GNU runtime 时同时包含 `COPYING3` 与 `COPYING.RUNTIME`，并继续执行
  实际加载路径检查。
- 打包后的许可证审计按产物格式检查，不以构建树中存在许可证代替。

外部证据只进入 job 临时目录，不提交到仓库，不作为 Release 资产发布。下载地址、
预期摘要与失败行为由脚本和合同测试锁定。

## 打包脚本边界

新增平台脚本只负责编排，不在 YAML 中堆积复杂部署逻辑：

- Linux Bash 脚本：组装 AppDir、调用部署工具、审计和生成 AppImage。
- Windows PowerShell 脚本：接收 MSVC/MinGW 模式、部署、审计和生成 ZIP。
- macOS Bash 脚本：接收目标架构、部署 `.app`、审计和生成 DMG。
- 公共脚本：生成构建清单、SHA-256 和 Release 正文输入。

所有脚本必须使用显式的源目录、构建目录和输出目录；拒绝工作区根目录、未解析变量
和仓库外任意删除。输出目录每次由 CI 创建且只能删除其自身内容。

每个平台 artifact 至少包含：

```text
package
package.sha256
build-info.json
```

`build-info.json` 固定记录 commit、dirty 状态、UTC 构建时间、runner OS/架构、Qt、
编译器、CMake preset、链接方式、LTO 状态和包文件 SHA-256。

## Release 更新事务

publish job 下载五个平台 artifact 后执行以下顺序：

1. 拒绝缺少、重复或平台后缀不一致的包。
2. 重新计算所有 SHA-256，并与各 job 清单比较。
3. 验证五份 `build-info.json` 的完整 commit 完全一致且等于 workflow commit。
4. 生成中文 Release 正文，列出五个产物、校验值、工具链和未签名提示。
5. 使用不含提交号的稳定文件名上传本轮全部资产；提交号仅保留在 Release 标题和 build-info 中。
6. 确认 GitHub API 能列出本轮五个包及其校验/构建信息资产。
7. 将 `continuous-build` tag 指向本轮 commit，更新标题、正文和 Pre-release 状态。
8. 最后删除不属于本轮 commit 的旧资产。

上传新资产之前不得删除上一轮可用资产。如果上传或校验失败，publish job 失败并保留
上一轮完整产物；可能产生的本轮临时资产由下一次 publish 开始时按提交号清理。这样
避免先删除 Release 后创建失败造成下载页面空缺。

Release 标题格式：

```text
Continuous Build <short-sha>
```

Release 正文必须明确：这是自动生成、未签名、非稳定版，只建议测试使用；稳定版本
以后通过版本 tag 单独发布。

## 测试与门禁

### 静态合同

- workflow 只包含一个 Linux 编译配置。
- 所有 Action 使用固定 commit，不使用浮动 `@vN` 或 `*-latest` runner。
- PR 路径没有 `contents: write` 和 Release API 调用。
- publish job 精确依赖五个平台产物。
- Qt 版本、MinGW 工具身份和产物命名保持一致。
- 禁止 `continue-on-error` 隐藏失败。

### 构建与测试

- 五个平台均配置 Release/shared/LTO、编译 Example 并运行完整 CTest。
- 部署后的最终入口执行 smoke，而不是只验证源码树目标。
- Linux 检查 ELF/RPATH/Qt 插件/GNU runtime。
- Windows 检查 PE 架构、ABI 和 Qt 插件。
- macOS 检查 Mach-O 架构、framework 路径和 Qt 插件。

### 打包审计

- 包内不存在构建目录、源码绝对路径、测试报告、私有 Qt 头或 `temp_image/`。
- 所有一方库和实际部署依赖可解析。
- 许可证、第三方通知和 build info 完整。
- 压缩包或磁盘映像可以在全新临时目录解包/挂载并运行 smoke。

## 失败处理

- 任一合同、构建、CTest、部署、smoke、依赖或许可证检查失败时，不运行 publish。
- 任一平台没有生成精确一个预期包时，不运行 publish。
- GitHub Release 更新失败时 workflow 标红，不把部分结果描述为发布成功。
- Qt 在线归档、部署工具下载或固定摘要失效时显式失败；不得自动切换浮动版本。
- Windows/macOS 未签名提示是已知发布属性，不用 `continue-on-error` 隐藏实际运行错误。

## 文档更新

实现时同步更新：

- README：增加 Continuous Build 下载入口和五个产物说明。
- `docs/development/BUILDING_ZH.md`：记录本地复现各平台发布包的方法。
- `docs/development/GITHUB_ACTIONS_ZH.md`：记录简化矩阵、权限和发布事务。
- 平台支持文档：区分 CI 部署 smoke、continuous 预发布与真机验收。
- 发布文档：保留正式签名、notarization 和稳定 tag 为后续工作。

## 验收标准

1. PR 可以验证五个平台，但不会创建或修改 GitHub Release。
2. `master` 上同一提交的五个平台全部通过后，`continuous-build` 页面自动更新。
3. Release 页面同时提供一个 Linux AppImage、两个 Windows ZIP 和两个 macOS DMG。
4. 每个包都有 SHA-256 和结构化构建身份，五者 commit 相同。
5. 最终部署入口的 smoke、架构、依赖和许可证检查全部通过。
6. 新一轮失败不会删除上一轮完整可用的 continuous build。
7. GitHub Actions 中 Linux 只构建一个 Release/shared/LTO 配置。
8. Qt 版本升级只需修改集中版本和必要的匹配工具身份，任何平台不兼容都会失败关闭。
