# QWindowKit 上游同步计划

> 在当前会话由主代理按 executing-plans 实施，独立审查按 requesting-code-review 进行。
> 保留本地 master 线性提交，不推送，不访问 docs/research。用户已授权同步第三方源码。

**状态：** 2026-11-09 已同步并完成下述本机验证；本地提交，不推送。

**目标：** 将 QWindowKit 从 `5626b3ae5f22508d77361495f589abedb39c2ee4` 同步到
2026-11-09 查询的 main HEAD `2072958b04820e9e7629f6c40068dee0ce0e9915`，保持框架窗口功能。
**架构：** 上游移除 qmsetup/stdcorelib，使用自带 CMake 工具；保持静态私有 QWK 后端、
现有 ZzWindowKit API 和 SDK 封装。迁移 Windows 长路径保护，移除过时依赖安装要求。
**技术栈：** CMake、C++20、Qt 6.8.3／6.11.1、GCC、Clang sanitizer、Linux X11。

## 1. 固定上游与核对来源

- [x] 查询 main HEAD、比较旧提交至新提交的完整差异；Widgets 运行时代码未改变。
- [x] 下载固定 codeload 归档，核对 Git 文件与归档字节；检查子模块和许可变化。
- [x] 同步 `ZzThirdParty/qwindowkit` 的受版本控制文件，移除上游已删除的 qmsetup 子树。

## 2. 保留构建兼容性

- [x] 为 `cmake/SyncHeaders.cmake` 新增框架侧契约测试，覆盖普通／私有头、style 排除、
  陈旧头清理和 Windows 长路径。先确认长路径用例失败，再迁移原 `b80a5d2` 的保护：
  Windows 拼接路径长度 ≥240 时使用规范化绝对源路径，其他路径继续使用相对引用。
- [x] 更新 `cmake/ZzInstallPackage.cmake`、`cmake/ZzVerifyInstalledLicenses.cmake`、
  `scripts/package/StageRuntimeLicenses.cmake` 和打包契约，去除已删除依赖的许可证复制；
  保留实际 QWindowKit 许可证、历史具名审核与禁止构建工具进入产品包的检查。
- [x] 更新 `cmake/ZzReleaseChecks.cmake` 与 `tests/Release/ZzReleaseChecksFixture.cmake`：
  显式支持派生构建工具已移除状态，必须验证源码缺席、移除提交和历史记录摘要；
  继续验证旧状态，新增残留源码、伪造移除提交和历史摘要篡改的失败测试。
- [x] `scripts/package/PrepareReleaseEvidence.cmake` 从 manifest 读取固定上游提交，
  移除状态不再下载已不参与构建的 Qt 5.15.2 源码证据。

## 3. 更新来源记录并构建

- [x] 更新 `docs/third-party/qwindowkit-vendor.json`、`release-evidence.json` 和
  `reviews/qwindowkit-source-verification.json`，登记新归档、迁移补丁、无子模块状态；
  不修改历史人工签署记录。
- [x] 构建 Qt 6.11 Debug 全部目标、Qt 6.8 Release 窗口／框架／示例目标及静态 SDK。
  使用 `cmake --build <既有目录> --parallel 2`，同一目录构建与测试不并发。

## 4. 验证功能与分发

- [x] Qt 6.11 执行全套 CTest（独立 SDK/迁移另跑），覆盖窗口、导航、多窗口、截图、
  Example、架构、发布和 CI 静态契约；任何缺失目标先补建，不将 Not Run 视为通过。
- [x] Qt 6.8 定向执行 `windowkit.*`、`puretools.*`、`example.*` 及公开头／架构检查。
- [x] 两版 Qt 在 Xvfb/xcb 下运行 `ZzWindowKitLifecycleTest` 和窗口示例烟测。
- [x] 共享／静态分别执行 `install.consumer`、`platform.package-relocation`。
- [x] Clang ASan＋UBSan 执行 windowkit、页面导航和多窗口相关回归。
- [x] 重新准备固定摘要发布证据，配置正式发布并验证许可证安装。
- [x] 独立审查上游同步差异、唯一补丁、构建边界及实测证据。

## 5. 文档与提交

- [x] 最后更新来源说明、THIRD_PARTY_NOTICES、RELEASE_BLOCKERS 和本计划状态。
  明确 Windows/macOS 仅静态契约或模拟生成检查，未原生实机验收。
- [x] 执行架构／文档审计、Markdown 相对链接检查和 `git diff --check`。
- [x] 本地提交代码及证据清单，文档最后提交；构建产物不提交，确认工作区干净。

## 完成记录

- 同步代码与构建适配：`e6bff10`；上游固定在 `2072958b04820e9e7629f6c40068dee0ce0e9915`。
- 额外修复：`d18980e`。全量回归发现既有 PureTools 架构扫描将 PageHost 的 Q_ENUM
  当作未注释方法；新增正反 fixture，准确排除该元对象宏，仍拒绝后续未注释方法。
- 上游归档 157 文件与 checkout 逐字节一致，vendor 仅 Windows 长路径补丁不同。
  旧 qmsetup 中的本地 Git 元数据保存在忽略的 build 目录，未包含于新 vendor 或提交。
- 源码树外的头生成契约也已通过；Windows 分支在 Linux 上模拟，未宣称原生 Windows 编译。

所有本轮日志位于 `build/qwindowkit-sync-20261109/`：

| 验证 | 实际结果 | 日志 |
| --- | --- | --- |
| Qt 6.11.1 Debug | 全量构建通过；279 项首次 278 通过，剩余 Q_ENUM 扫描误报修复后通过；新正反契约也通过 | `qt611-build.log`、`qt611-tests.log`、`enum-contract-green.log` |
| Qt 6.8.3 Release | 全量构建及 147 项定向回归通过，覆盖窗口、页面、导航、多窗口、工作区、示例和截图 | `qt683-build.log`、`qt683-tests.log` |
| Clang 20.1.8 ASan＋UBSan | 窗口、页面导航和多窗口 21 项通过 | `asan-build.log`、`asan-tests.log` |
| X11／Xvfb | 两版 Qt 生命周期各 5 行通过，窗口示例均正常启动并退出 | `x11-qt611.log`、`x11-qt683.log`、`demo-x11-qt611.log`、`demo-x11-qt683.log` |
| Qt 6.8 共享 SDK | 安装消费及包迁移 2/2 通过 | `sdk-shared.log` |
| Qt 6.8 静态 SDK | 安装消费及包迁移 2/2 通过 | `sdk-static.log` |
| 正式发布 | 新空证据目录下载成功，配置摘要门禁、Release 构建及许可证安装审计通过 | `prepare-evidence.log`、`release-configure.log`、`release-build.log`、`release-install.log` |
| 来源及发布检查 | 新旧证据格式、残留源码／伪造提交／历史摘要篡改负例、打包、架构共 9 项通过 | `final-contracts.log` |
| Windows 长路径 | 原始上游生成器先失败，迁移兼容保护后通过；源码树外目录通过 | `headers-red.log`、`headers-green.log`、`headers-outside-tree.log` |

独立审查未发现阻塞项。历史审核和批准文件原字节未改，源码／证据摘要保持一致；
截图基线、像素门限、性能门限均未调整。Windows／macOS 的构建脚本与打包契约通过，
未执行原生编译及窗口交互验收，Linux 自动化结果不替代这些平台的实机结果。
最终来源、补丁和分发边界见[来源核对记录](../../third-party/PROVENANCE_AUDIT_ZH.md)。
