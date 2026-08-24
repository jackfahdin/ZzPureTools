# Task 9R order and planner linearization report

报告 HEAD：`9535a8ce1ffaea5e1a9d5872a295b6bc91194a25`。早先 order/planner 性能门禁重跑日期为 2026-08-24，Linux / GCC 15.2 / Clang 20.1.8 / Qt 6.11.1；其 `91040df..bee5e5b` 审查证据和值保持不变。2026-08-25 的最终分支复审与控制者验证以 `8515520..9535a8c` 为静态 diff 范围；工作树干净，未触碰主工作树或 `temp_image/`。

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
- `756c392 修复：拒绝侧栏同步所有权污染`
- `3e6c21d 修复：保留框架内第三方侧栏内容`
- `a421cc1 修复：以整帧托管保护侧栏内容所有权`
- `2844f7f 测试：覆盖托管框架排队回挂窗口`
- `4309507 测试：完整冲刷托管终结事件`
- `bee5e5b 修复：原子终结侧栏托管框架`
- `ff06fdf 修复：审计活动行移除后的侧栏重入`
- `9535a8c 修复：重审失败回滚后的侧栏同步`

`91040df..bee5e5b` 还包含与这些实现对应的中文设计/计划提交。`git diff --stat` 为 17 files changed、4224 insertions、268 deletions；代码范围是 ActivityBar、WorkspaceShell、LayoutState、ActivityMoveTransaction、PanelStack 及其测试，另含计划列出的 docs。

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

初始独立审查范围为 `91040df..8deed7a`，结论为 0 Critical、2 Important、1 Minor。两个 Important 是 `addWidget()` 同步边界后从 observed parent 误学习固定 owner，以及 `panelMoved` 第三方接管 content 后失败 rollback 遗留 PanelStack record/frame ghost。修复链为 `756c392`、`3e6c21d`、`a421cc1`、`2844f7f`、`4309507`、`bee5e5b`；完整四轮根因、RED/GREEN 和逐轮复审证据见被忽略的 `final-review-owner-fix-report.md`，本报告不重复其细节。

四轮定向独立复审已完成。第 4 轮原始 DeferredDelete TOCTOU 已标记为 ADDRESSED，ChildRemoved-only 清理覆盖亦已补齐。最终独立审查范围为 `91040df..bee5e5b`，没有未解决的 Critical、Important 或 Minor。

实现者最终验证（不替代独立审查）为：ownership/rollback/take 聚焦 15/15、`fluent.panel-stack`/`fluent.side-pane` 2/2、完整 `ZzWorkspaceShellTest` 157/157、`puretools.workspace-shell` 1/1；静态扫描及 `git show --check` clean。早先性能数值与 GCC Debug/Release/static 门禁证据保持如下，未因后续 owner/rollback 修复而重写。

## Final branch review closure

第三波 `ff06fdf` 关闭了 `rowsRemoved` 精确重入缺口；其定向复审发现新的 Important：registry 在 edge visibility、current、active 同步前已被擦除。第四波 `9535a8c` 以逻辑待删候选、每轮一个 setter、每个 setter 返回后的全局重审及最终无 signal 的 erase 收口；`collapsedChanged` 和 `currentSourceIndexChanged` 两条真实 RED 均转 GREEN。

原终审者对 `9535a8c` 独立复审：erase-after-sync 为 ADDRESSED；collapse/current/active setter 均发生在 erase 前，任一 setter 变异后会进入下一轮完整审计；24 轮耗尽时 fail-closed 且不 erase。复审没有新 Critical、Important 或 Minor，允许关闭全部 Final Important。`active` 没有单独的动态重入用例是范围外观察，生产路径与同类 setter 静态等价；这不改变审查结论。没有第五波代码修复。

### 控制者最终验证（2026-08-25）

- `linux-gcc-debug` 配置退出 0，全默认目标增量构建退出 0；完整 Debug Shell 为 169/169，25.406 s；Debug Shell/state/codec CTest 为 3/3，26.02 s。
- GCC Release 配置/构建退出 0，Shell/state CTest 为 2/2，21.47 s；static Release 配置/构建退出 0，Shell/state CTest 为 2/2，21.39 s。
- 全量 Debug CTest 为 127/132，172.63 s；仅既有 workspace screenshot DPR 100/125/150/200 四项和既有 `OrderedPage` 缺 `Zz` 前缀的架构审计失败。当前配置默认未构建 examples，故分母为 132，不沿用早先 148。
- Clang 20.1.8 Release 配置退出 0；单独 `ZzWorkspaceShellTest` 的生产与测试构建退出 0、运行 169/169，21.319 s。联合 Shell/state/codec 构建仍在 `ZzWorkspaceLayoutStatePrivateTest.cpp:678,754` 的两处既有 `optional::emplace()` 停止，不归因本分支。
- 静态扫描确认 changed public include diff 为空，schema 1/2 与 Qt Dock state version 未改；namespace shorthand、Qt private header、stylesheet、compiler extension 均无匹配；`git diff --check 8515520..HEAD` clean。

## Remaining parked findings

- private codec 测试尚未断言 InvalidArgument/InvalidState/Io error-code mapping（Minor）。
- ParentChange 中销毁正在换父内容属于计划明确不支持的边界。
- rollback-failed 路径仍有 Secondary area 被映射为 Primary 的风险。
- Clang 20 private test 第 677、753 行 `optional::emplace()` 前序编译阻塞。
- ASan Shell 基线资源测试第 2863 行悬空 `QWidget*` 比较/格式化崩溃。
- 既有 4 个 workspace screenshot 目标和 `OrderedPage` 架构命名失败。
- 初始独立审查的 Activity 性能门禁 Minor 仍延后：计时后没有逐次断言 `current`/`active`，不能表述为全部历史 Minor 已修。
- Windows/MSVC、Qt SDK MinGW、macOS/AppleClang、ASan、clang-tidy 均未运行；只有源码静态可移植性检查，不宣称这些平台或工具已通过。
