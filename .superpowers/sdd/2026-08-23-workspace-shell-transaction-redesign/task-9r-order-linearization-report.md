# Task 9R order and planner linearization report

验证 HEAD：`90f6e71cc57b3f5e6e5feb0a646e4412cfa8ad8d`。最终重跑日期：2026-08-24，Linux / GCC 15.2 / Clang 20.1.8 / Qt 6.11.1。最终重跑前 `git status --short --branch`、`git diff --check 91040df..HEAD` 均退出 0；工作树干净，未触碰主工作树或 `temp_image/`。

## Commits

- `85ad3ef 修复：统一侧栏主次面板注册顺序`
- `15e8abb 修复：守卫侧栏内容内部所有权`
- `d5a4150 修复：以固定身份校验侧栏所有权`
- `4d30788 性能：线性化活动栏选择同步`
- `1dfd34c 性能：线性化工作区布局规划器`
- `68fc589 修复：守卫活动迁移回滚边界`
- `16a2682 测试：覆盖活动回滚污染面板`
- `d8dddc8 测试：补全活动迁移状态合同`
- `90f6e71 测试：修复规划器别名的 Clang shadow 回归`

`91040df..HEAD` 还包含与这些实现对应的中文设计/计划提交。`git diff --stat` 为 14 files changed、3352 insertions、259 deletions；代码范围是 ActivityBar、WorkspaceShell、LayoutState、ActivityMoveTransaction 及其测试，另含计划列出的 docs。

## Secondary-first RED and GREEN

RED 命令为 GCC Debug 构建后聚焦运行：

```bash
QT_QPA_PLATFORM=offscreen build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest secondaryFirstRegistrationUsesCanonicalSideOrderAndRoundTrips sideRegistrationDoesNotReclaimThirdPartyContentAfterPanelMove sideRegistrationRollsBackOnlyOuterAfterReentrantRegistration
```

退出码 3，`2 passed, 3 failed`。物理栈首项仍为先注册 Secondary，两个同步重入回调未进入。审查补强还分别取得同栈第三方 owner RED 和 frame objectName RED，均为退出码 1、`2 passed, 1 failed`。

GREEN 后每侧物理栈固定 `Primary + Secondary`，同 area 保持输入顺序；Activity model 保持全局注册顺序。Secondary-first/交错注册成功后立即 `saveLayout()` 并 round trip。任务 1 最终聚焦为 `10 passed, 0 failed`，完整函数清单分段为 `41 + 73 + 38 = 152 passed, 0 failed`。本次最终门禁的完整 Shell 为 `151 passed, 0 failed`（测试清单后续调整后的当前总数）。同步第三方接管返回 `InvalidState`，只清本次 registry/model/connection，不强取第三方内容。

## Planner 512/4096 RED and GREEN

- Planner RED：512/4096 中位数 `23,645,591 -> 1,372,266,266 ns`，`58.03x`，退出码 1，违反 `<20x`。
- Planner GREEN：`8,859,781 -> 70,920,724 ns`，`8.00x`，满足 `<20x`；private QtTest 20/20。
- Activity move RED 分层：128/512 forward placeSide `11.91 -> 183.8 ms`，`15.4x`，分别 128/512 次 movePanel、127/511 次 signal。
- 单 moved 实现 GREEN：128/512 完整事务 `2,295,977 -> 8,917,115 ns`，`3.88x`，满足 `<10x`。
- ActivityBar 共享组件 RED/GREEN：`198,830 -> 2,137,214 ns`，`10.75x`；优化后 `172,299 -> 726,080 ns`，`4.21x`。

本次新鲜 Debug 运行再次执行了 `restorePlannerScalesBelowQuadraticGrowth` 和 `activityMoveAuditScalesBelowQuadraticGrowth`，两者均包含在 20/20 与 151/151 通过结果中。

最终扫描：

```bash
rg -n 'contains\(|indexOf\(|find_if' ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
```

退出码 0（存在已分类命中）。LayoutState 命中为 QHash/QSet membership 或单 panel 的 visible/size 定位；Activity 命中为局部集合、固定四 area、单 movedId 定位或 QWidget/PanelStack ownership 查询。未发现循环体内随 panel/group 数量增长的全 `QStringList` 扫描。

## Semantic mutation evidence

- 锚点变异把 before bucket 临时移到 anchor 后：`alternatingOmissionsKeepStableAnchorsAndSizes` 失败，实际首项 `f`、期望 `e`，退出码 1；恢复后 GREEN。
- target-index 变异把唯一 moved 目标索引临时加一：同侧用例期望一条 signal、实际两条，跨侧期望一条、实际零条，退出码 1；恢复后三个 Activity 用例 GREEN。
- ActivityBar 二分精确匹配变异移除 row equality：另一 edge row 被错误接受，语义测试失败；恢复后完整 ActivityBar 22/22。
- Clang shadow 修复的临时兼容检查仅把两处既有 `emplace()` 改为显式 projection 赋值，ASan private 单目标编译链接退出 0；随后恢复，未提交该前序兼容修改。

## GCC Debug/Release/static

所有最终配置都显式传入 Qt/compiler 环境；因先前缓存重建会丢失 RPATH，本轮加入 `-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON`，运行测试时显式使用 Qt 和构建树库目录。

Debug：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-gcc-debug --parallel 2
QT_QPA_PLATFORM=offscreen build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest
QT_QPA_PLATFORM=offscreen build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest -silent
ctest --preset linux-gcc-debug -R '^(puretools.workspace-layout-state-private|puretools.workspace-layout-codec-private|puretools.workspace-shell|architecture.boundaries|architecture.complete-audit|architecture.fluent-visual-token-contract)$' --output-on-failure
```

配置退出 0；增量全默认目标退出 0（`ninja: no work to do`；首次缓存重建时已完整执行 509 步）。private 退出 0，20 passed/0 failed；Shell 退出 0，151 passed/0 failed。定向 CTest 退出 8，5/6；唯一失败是既有 `architecture.complete-audit` 的 `ZzSplitWorkspacePrivate.cpp:711 OrderedPage` 缺少 `Zz` 前缀，本批三项 workspace 测试均通过。

Release 与 static：

```bash
cmake --preset linux-gcc-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-gcc-release --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
ctest --preset linux-gcc-release -R '^(puretools.workspace-layout-state-private|puretools.workspace-shell)$' --output-on-failure
cmake --preset linux-static-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-static-release --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
ctest --preset linux-static-release -R '^(puretools.workspace-layout-state-private|puretools.workspace-shell)$' --output-on-failure
```

Release 配置/构建退出 0（4 个增量步骤），CTest 退出 0、2/2；static 配置/构建退出 0（4 个增量步骤），CTest 退出 0、2/2。

## ASan/UBSan

```bash
cmake --preset linux-clang-asan -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-clang-asan --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
```

配置退出 0；构建退出 1。Clang 20 的本批 16 个 `-Wshadow` 诊断经 `90f6e71` 已清零；只剩前序 private test 第 677、753 行 `request.projection.emplace()` 的 `no matching member function`。因此 2/2 sanitizer CTest 未运行，不能表述为 ASan/UBSan 通过。

已成功链接的 Shell sanitizer 目标另行运行 `ctest --preset linux-clang-asan -R '^puretools.workspace-shell$' --output-on-failure`，退出 8、0/1。在 `keepsActivityMoveResourceBudgetStableAcrossRoundTrips` 第 2863 行，`currentParents != baselineParents` 后 QtTest 尝试 stringify 悬空 `QWidget*`，ASan 报 `SEGV`。该断言来自基线前提交 `8bbd750`，不是 `91040df..HEAD` 新增，分类为前序 sanitizer 阻塞。

## Clang and clang-tidy

```bash
cmake --preset linux-clang-tidy-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DZZ_ENABLE_CLANG_TIDY=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build --preset linux-clang-tidy-release --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy
```

配置退出 0，Cache 确认 `ZZ_ENABLE_CLANG_TIDY=ON` 且 `ZzClangTidy` 目标存在。两测试目标构建退出 1，仅为同两处既有 `emplace()`。`ZzClangTidy` 退出 1，因为其全目标依赖在进入 tidy 脚本前被同两处编译错误阻塞；clang-tidy 未完成，不能记为通过。

## Full CTest classification

```bash
ctest --preset linux-gcc-debug --output-on-failure
```

使用绝对构建树 `LD_LIBRARY_PATH` 和 `QT_QPA_PLATFORM=offscreen`，退出 8；143/148 passed，5 failed，总耗时 187.04 秒。

- 本批相关：`puretools.workspace-shell`、layout-state private、codec private、ActivityBar 均通过。
- 既有 workspace screenshot：DPR 100/125/150/200 四个目标失败；每个内部 2 passed/6 failed，diff 位于 build reports，未更新基线。
- 既有架构命名：`architecture.complete-audit` 因 `OrderedPage` 缺 `Zz` 前缀失败。
- Example 硬编码日志路径：本轮 16 个 example 目标全部通过，未复现该环境失败。
- 新出现其他失败：0。

## Windows/MinGW/macOS static portability

以下三条 `! rg` 分别退出 0、无输出：目标源码未发现嵌套 namespace 简写、Qt Private API/stylesheet、GCC/Clang 专用 attribute/builtin/pragma。

```bash
! rg -n 'namespace [A-Za-z_][A-Za-z0-9_]*::' <three-private-sources>
! rg -n 'Qt[A-Za-z]+/private|Qt[A-Za-z]+Private|setStyleSheet|styleSheet\(' <three-private-sources>
! rg -n '__attribute__|__builtin_|#pragma GCC|#pragma clang' <three-private-sources>
```

Windows/MSVC：未运行。Qt SDK MinGW：未运行。macOS/AppleClang：未运行。上述证据只能说明目标源码未发现这些平台专用扩展，不能表述为三平台通过。

## Independent review

等待控制者分派独立审查。审查范围应为精确 diff `91040df..HEAD`，本执行代理未以自审冒充独立审查。

## Remaining parked findings

- private codec 测试尚未断言 InvalidArgument/InvalidState/Io error-code mapping（Minor）。
- ParentChange 中销毁正在换父内容属于计划明确不支持的边界。
- rollback-failed 路径仍有 Secondary area 被映射为 Primary 的风险。
- Clang 20 private test 第 677、753 行 `optional::emplace()` 前序编译阻塞。
- ASan Shell 基线资源测试第 2863 行悬空 `QWidget*` 比较/格式化崩溃。
- 既有 4 个 workspace screenshot 目标和 `OrderedPage` 架构命名失败。
- 最终独立审查尚未完成；在审查无未解决 Critical/Important 前，不宣称整个计划完全完成。
