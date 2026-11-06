# 带动画的页面容器实现计划

> **面向执行者：** 在用户批准开始实现后，使用 executing-plans 逐任务执行。
> 沿用本地 master 线性提交；实现不委派，独立代码审查可按 requesting-code-review 执行。
> 本文同时保留计划与执行记录；具体测试范围见文末及使用文档。

**目标：** 新增独立 `ZzAnimatedStackedWidget`，提供有界页面动效并替换标签示例中的局部实现。

**架构：** QWidget＋受保护的普通 QWidget 宿主承载真实页面；独立覆盖层绘制两张快照，
单一 QVariantAnimation 驱动横向／纵向滑动和渐变，只保留最后一个待切目标。
页面业务、路由和缓存继续由原框架管理。

**技术栈：** C++20、Qt Widgets／Gui／Core／Test、Qt 6.8+、CMake、现有主题／性能报告设施。

**设计依据：** [接口、状态机与验收契约](../specs/2026-11-06-animated-stacked-widget-design.md)。
核对基点 `cd41d79`。首版不替换 `ZzWorkspaceShell` 或 `ZzPageHost`，不包含 Tour／TimerDial。

## 文件与职责

以下新增路径在执行阶段创建；已有路径已在制定计划时核对。

| 操作 | 路径 | 职责 |
| --- | --- | --- |
| 新增 | `ZzFluentUI/widgets/include/ZzFluentUI/ZzAnimatedStackedWidget.h` | 导出接口、枚举、信号与所有权说明 |
| 新增 | `ZzFluentUI/widgets/src/ZzAnimatedStackedWidget.cpp` | 公开委托、Qt 事件入口及析构 |
| 新增 | `ZzFluentUI/widgets/src/private/ZzAnimatedStackedWidgetPrivate.h` | QPointer 页面记录、焦点、队列及状态机 |
| 新增 | `ZzFluentUI/widgets/src/private/ZzAnimatedStackedWidgetPrivate.cpp` | 校验、事务切换、降级与资源回收 |
| 新增 | `ZzFluentUI/widgets/src/private/ZzStackTransitionOverlay.h`、`.cpp` | 仅绘制快照及拦截本区域输入；私有小类无需再拆 Pimpl |
| 新增 | `ZzFluentUI/tests/ZzAnimatedStackedWidgetTest.cpp` | 页面契约、动效、重入、像素与资源回归 |
| 修改 | `ZzFluentUI/CMakeLists.txt` | 注册源文件、MOC 公开头与现有安装导出机制 |
| 修改 | `ZzFluentUI/tests/CMakeLists.txt` | `ZzAnimatedStackedWidgetTest`／`fluent.animated-stack` |
| 修改 | `ZzFluentUI/tests/ZzFluentAccessibilityTest.cpp` | 真实页面树与焦点状态 |
| 修改 | `examples/ZzPureToolsExample/ZzExampleTabBarsPage.cpp` | 使用公共组件，删除 ZzVerticalPageStack |
| 新增 | `examples/ZzPureToolsExample/tests/ZzExampleAnimatedStackTest.cpp` | 真实标签示例接入回归 |
| 修改 | `examples/ZzPureToolsExample/tests/CMakeLists.txt` | `ZzExampleAnimatedStackTest`／`example.animated-stack` |
| 修改 | `tests/InstallConsumer/CMakeLists.txt`、`Gui/main.cpp` | 公开头单独编译及安装后实例化使用 |
| 新增 | `benchmarks/ZzAnimatedStackBenchmark.cpp` | 捕获耗时、绘制耗时、释放及环境指纹采样 |
| 修改 | `benchmarks/CMakeLists.txt` | 独立 `benchmark.animated-stack`；不覆盖原动画场景 |
| 新增 | `docs/development/ANIMATED_STACK_ZH.md` | 使用、信号时序、降级、性能与平台边界 |
| 修改 | `docs/development/FLUENT_UPSTREAM_FOLLOWUP_2026-11-05_ZH.md` | 实际交付状态和证据 |

## 执行约定

- 每个任务先增加有行为意义的失败测试，运行确认原因，再实现并复跑；
  已有契约测试足以复现的问题直接使用现有测试，不为单行配置改动堆叠重复测试。
- 测试使用可控动画时间，不以固定 sleep 判断中间态；异步可见性使用现有 eventually 辅助。
- 测试用的页面和监听器按 Zz 前缀、中文 Doxygen、单一命名空间、最小公开面编写。
- 每个实现任务通过后独立提交，描述用中文简述和详细说明；性能报告在 build 目录生成。
- 用户在本计划后以“开始吧”授权实现；继续使用本地 master 线性提交。
- 执行调整：用手工维护几何／显隐的宿主替换 QStackedLayout，修复真实 Hide 回调重入；
  任务 3／4 的生命周期与输入焦点修复共同提交，资源与可选类型测试在任务 6 补齐。
- 部分后续测试所需行为已在任务 2 实现，新增时直接通过；没有人为破坏实现制造失败。
  捕获中断覆盖目标、显隐回调抢焦点、无关页面 opt-out 中断均保留真实 RED／GREEN 证据。
- 示例专项通过同一个生产 buildTabBars 装配入口验证；该无状态私有成员改为 static，
  避免测试额外链接无关 Showcase 页面，完整应用另有集成和截图回归。

## 任务 1：可安装的页面容器与所有权

**文件：** 新增控件四文件及 `ZzAnimatedStackedWidgetTest.cpp`；修改两个 FluentUI CMake 文件。

- [x] 为 `ZzAnimatedStackedWidgetTest` 注册 Qt Test 目标和 `fluent.animated-stack`，
  沿用 `ZzExpanderTest` 的警告、sanitizer、AUTOMOC、offscreen 配置。
  将新的公开头加入 `zz_fluent_ui_moc_headers`，实现源加入模块源文件清单。
- [x] 添加 `ownsAndReturnsPages`、`rejectsInvalidPages`、`removesCurrentDeterministically`、
  `observesDestroyedOrReparentedPages` 和 `indexChangesPreserveIdentity`。
  核心断言示例：

```cpp
ZzFluentUI::ZzAnimatedStackedWidget stack;
auto *first = new QWidget;
QPointer<QWidget> guard(first);
QCOMPARE(stack.addWidget(first), 0);
QCOMPARE(stack.currentWidget(), first);
QCOMPARE(stack.addWidget(first), 0);
QCOMPARE(stack.count(), 1);
QCOMPARE(stack.takeWidget(first), first);
QVERIFY(first->parentWidget() == nullptr);
QCOMPARE(stack.currentIndex(), -1);
delete first;
QVERIFY(guard.isNull());
```

- [x] 构建并记录新增类型不存在的失败；创建最小接口后再确认非法接管、删除和索引行为
  的断言失败，避免只以编译失败证明全部所有权场景。
- [x] 实现设计第 3 节接口的容器基础：页面父子关系、受保护的内部内容宿主、
  QPointer 记录、索引查询、take、destroyed／ParentChange 清理；实际宿主调整见上方说明。
  此任务切页直接生效，未启用绘制覆盖层。
- [x] 显隐、setParent 和信号前后加对象存活／代次检查；构造期事件不得访问尚未赋值的 d_ptr。
  析构先断连接和过滤器，再销毁私有状态，最后让 QWidget 清理子树。
- [x] 两版 Qt 运行新目标与公开头检查，通过后提交：`feat(fluent): 增加独立页面容器与所有权契约`。

```bash
cmake --build build/spectrum-qt683-release --target ZzAnimatedStackedWidgetTest --parallel 2
ctest --test-dir build/spectrum-qt683-release -R '^fluent\.animated-stack$' --output-on-failure
```

预期：新增目标存在，所有权和无动画状态断言全部通过；同命令替换为 `build/linux-gcc-debug`。

## 任务 2：单动画快照绘制

**文件：** 控件四文件、私有 Overlay 两文件、新测试及 FluentUI 模块 CMake。

- [x] 添加纯色页测试 `rendersSlideAndFade_data`，覆盖四模式、前后方向、RTL、
  contentsMargins 与 DPR；加入 `commitsLogicalPageBeforeVisualCompletion`。
  显示容器后才切换，先验证原实现缺少 running 和中间态，从而得到真实失败。
- [x] 用暂停的 QVariantAnimation 驱动中点，断言空间位置和颜色，不只断言动画对象存在：

```cpp
using Stack = ZzFluentUI::ZzAnimatedStackedWidget;
stack.setTransitionMode(Stack::ZzTransitionMode::SlideHorizontal);
stack.setDuration(200);
QVERIFY(stack.setCurrentIndex(1));
QCOMPARE(stack.currentIndex(), 1);
QVERIFY(stack.isTransitionRunning());
auto *animation = stack.findChild<QVariantAnimation *>();
QVERIFY(animation);
animation->pause();
animation->setCurrentTime(100);
// 红／蓝纯色页：按 OutCubic 在 t=0.5 的进度 0.875 计算分界，
// 读取 DPR 对应像素，分别验证分界两侧，允许边界 1 个物理像素误差。
animation->setCurrentTime(animation->duration());
QVERIFY(!stack.isTransitionRunning());
```

- [x] Overlay 增加受控帧状态 setter，输入两张 QImage、进度、模式、方向和内容矩形。
  `paintEvent` 只绘制，外层在 valueChanged 中更新进度／update。
  滑动偏移用 `extent * progress`；Fade 先画旧图，再以 progress 画新图。
- [x] 实现准备旧图、逻辑提交、准备目标图、隐藏真实宿主、显示覆盖层的事务。
  每个 render 前后检查页面 QPointer 和代次；不调用 processEvents 强行刷新布局。
- [x] 读取主题 PageTransition、reducedMotion 和非 Fluent 回退；复用一个 QVariantAnimation，
  保留 `transitionStarted`／`transitionFinished` 的严格配对。
- [x] 两版 Qt 的新测试全部通过；提交：`feat(fluent): 实现单动画页面滑动与渐变`。

## 任务 3：快速请求、删除与信号重入

**文件：** 私有状态机、公开委托和新测试。

- [x] 增加 `coalescesLatestDestination`、`clearsPendingForCurrentTarget`、
  `ignoresInvalidRequestWithoutDroppingPending`、`survivesPendingPageRemoval`、
  `callbacksCanDeleteOrRetarget_data`、`renderCanRequestAnotherPage`。
  每一类在修复前运行并记录失败；崩溃用 `-nocrashhandler`，避免产生巨大 core 日志。
- [x] 使用页面 QPointer＋方向保存唯一 pending，不保存可漂移索引。
  回调中导航先进入待切槽；finished 回调产生的新请求覆盖旧 pending。
  准备／收敛期间依靠 operationGeneration 阻止旧流程在重入后继续提交。
- [x] 固定以下行为作为断言：

```text
A -> B started；请求 C；请求 D
此时 current == B，动画对象数量 == 1
结束第一段：只开始 B -> D；C 从未发 currentChanged
结束第二段：current == D，图像释放，队列为空
```

- [x] 分别在 currentChanged、started、finished 和页面 render 中执行：删除容器、
  删除当前页、删除待切页、take、外部 setParent、再次请求当前页／其他页。
  断言无额外信号、无遗留覆盖层、选择有效存活页面。
- [x] Qt 6.8／6.11 通过，并在已配置的 ASan／UBSan 构建运行该测试；
  提交：`fix(fluent): 完善页面切换队列与生命周期边界`。

## 任务 4：中断、输入、焦点及快照降级

**文件：** 控件事件入口、Overlay、新测试和 `ZzFluentAccessibilityTest.cpp`。

- [x] 参数化新增 `settlesOnEnvironmentChange_data`：resize、hide、disable、deactivate、
  palette／style／font／language、RTL、DPR、reducedMotion、模式和时长变化。
  断言直接收敛到最后有效目标，running 为 false，图像全部释放。
- [x] 新增 `blocksInputToHiddenPages` 和 `restoresFocusWithoutStealing`：
  动画覆盖区域点击／滚轮／Tab 不操作真实页；外部导航仍可使用；外部焦点不被抢回。
  保留页面自身 disabled 状态，不用强行 disable／enable 实现隔离。
- [x] 无障碍断言：运行期间没有两套可见页面节点，结束后目标页名称与可聚焦子项可访问。
  单独验证 None 和 reducedMotion 不产生临时覆盖层焦点路径。
- [x] 增加 `skipsSnapshotForIneligiblePages_data`：显式禁用、原生子窗口、空区域、隐藏、
  零时长；可选 Qt 模块可用时验证真实 OpenGL／Quick／Video 类型。
  基础 Widgets 构建不能因为检测这些类型而新增链接依赖。
- [x] 增加溢出／64 MiB 超预算的纯资源计算测试，先判断预算再构造图像；
  大尺寸测试不实际分配超预算图像。捕获过程中失败必须释放第一张图像。
- [x] 运行两版 Qt offscreen 与 X11/Xvfb，四档 DPR 执行像素专项；
  提交：`fix(fluent): 完善页面动画降级与输入焦点恢复`。

```bash
QT_QPA_PLATFORM=xcb xvfb-run -a build/linux-gcc-debug/ZzFluentUI/tests/ZzAnimatedStackedWidgetTest -nocrashhandler
for scale in 1 1.25 1.5 2; do
  QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR="$scale" \
    QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough \
    build/linux-gcc-debug/ZzFluentUI/tests/ZzAnimatedStackedWidgetTest \
    rendersSlideAndFade -nocrashhandler || break
done
```

## 任务 5：替换示例中的局部实现

**文件：** `ZzExampleTabBarsPage.cpp`、新增 `ZzExampleAnimatedStackTest.cpp`、示例测试 CMake。

- [x] 用现有 Showcase 的 buildTabBars 装配入口创建 TabBars 页面，查找 `zzExampleNavigationPages`，
  新测试先断言其类型为 `ZzAnimatedStackedWidget`，在旧实现上应失败。
- [x] 验证五个标签对应五个稳定页面、SlideVertical、生效索引、快速点击只保留最后目标；
  原 `zzExampleNavigationBody0` 至 `4` 名称与可见文字保持一致。
- [x] 删除局部 ZzVerticalPageStack，改用公共组件；保留布局和页面颜色，
  将 `switchTo(index)` 替换为 `setCurrentIndex(index)`，删去仅旧动画使用的 include。
- [x] 用真实示例测试验证语言、RTL、关闭／重开页面和减少动态效果。
  构建 Example 并运行已有 workspace／integration／screenshot 测试；最终态出现变化时
  先分析布局与样式原因，不自动刷新基线。
- [x] 通过后提交：`refactor(example): 使用公共页面动画容器`。

```bash
cmake --build build/linux-gcc-debug --target ZzPureToolsExample ZzExampleAnimatedStackTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^example\.(animated-stack|workspace-smoke|puretools-integration(-english)?|puretools-screenshot-.*)$' \
  --output-on-failure --parallel 2
```

## 任务 6：资源与性能证据

**文件：** 新测试、`benchmarks/ZzAnimatedStackBenchmark.cpp`、benchmark CMake。

- [x] 新增 `keepsResourcesBounded`：固定页面数量运行 1,000 次切换和 100 次排队请求，
  验证至多一个动画、两个图像、一个 pending；稳定结束后图像存储为零，
  对象数回到稳定值。统计仅包括组件自身对象，不包括业务页面自有动画。
  连续推进动画时间时真实页面的额外绘制计数不能逐帧增长。
- [x] 新 benchmark 复用 `ZzPerformanceReporter`、`ZzBenchmarkMetadata`，
  注册独立 `benchmark.animated-stack`，保留原 `benchmark.animation`。
  记录 `snapshot_prepare_ms`、`overlay_paint_ms`、`peak_snapshot_bytes` 与 `idle_snapshot_bytes`。
  测试可通过私有头访问资源统计，不向公开控件增加测试专用接口。
- [x] 固定 1280×720 两页：每页同样的 12 个 QLabel／QLineEdit 表单项和一个 QPushButton；
  预热 10 次、采样 100 次，在 DPR 1、1.5、2 各运行三轮。
  捕获耗时从切页调用前计到快照安装完成，必须包含两张图的分配和 render。
  绘制耗时由基准内 QApplication 子类的 notify 包围覆盖层 Paint 事件分发测量，
  不能以帧数除总时长替代，也不向产品 paintEvent 增加基准日志。
- [x] 资源约束作为确定性失败条件；耗时对照设计中的 p95 目标 16.7／8 ms。
  超出目标先定位与优化，保留原始报告；未满足时不得标记性能验收完成。
  首版不把新报告直接写入历史相对基线或改变全局性能门限。
- [x] 提交基准代码与资源回归：`test(fluent): 增加页面动画资源与性能验证`。

参考运行（性能构建需要已配置 `ZZ_BUILD_BENCHMARKS=ON`）：

```bash
cmake --build build/linux-gcc-benchmarks --target ZzAnimatedStackBenchmark --parallel 2
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 xvfb-run -a \
  build/linux-gcc-benchmarks/benchmarks/ZzAnimatedStackBenchmark \
  --report build/animated-stack-dpr150-round1.json
```

其余缩放和轮次使用独立报告名，报告必须记录真实 DPR、Qt kit、编译模式和环境指纹。

## 任务 7：SDK、跨平台与 CI 交付检查

**文件：** `tests/InstallConsumer/CMakeLists.txt`、`tests/InstallConsumer/Gui/main.cpp`。

- [x] 将 `Zz::FluentUI|ZzFluentUI/ZzAnimatedStackedWidget.h` 加入安装头单独编译清单。
  消费者包含新头、实例化容器、添加两个页面，在 None 模式下验证切换与 take；
  这应先暴露头未导出／实现未链接的问题，再补齐打包配置。
- [x] 运行 Qt 6.8 共享与静态 Development／完整安装消费者，
  `install.consumer`、`platform.package-relocation`、`architecture.public-headers` 全通过。
  使用现有安装测试路径，不因新增组件无故新增嵌套构建步骤。
- [x] 两套 Qt 定向回归包括 `fluent.animated-stack`、`fluent.accessibility`、
  `fluent.screenshot-*`、`puretools.page-lifecycle` 和工作区公开 API／Shell 测试。
- [x] 完整执行下列 CI 配置步骤，再运行架构与文档检查；不能只跑组件测试：

```bash
set -e
cmake -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" -P tests/Platform/PresetMatrixContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzGateScriptContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzGitHubActionsContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" \
  -DZZ_TEST_ROOT="$PWD/build/contracts/release-packaging-support" \
  -P tests/Platform/ZzReleasePackagingSupportContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzLinuxAppImagePackagingContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzWindowsPackagingContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzMacosPackagingContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzContinuousReleaseContract.cmake
ctest --test-dir build/spectrum-qt683-release \
  -R '^(fluent\.(animated-stack|accessibility|screenshot-.*)|puretools\.(page-lifecycle|workspace-shell|workspace-public-api)|architecture\.(complete-audit|public-headers|documentation-audit)|platform\.gate-script-contract)$' \
  --output-on-failure --parallel 2
```

- [x] Windows MSVC／MinGW 与 macOS 静态检查后记录真实构建／交互结果；
  没有相应环境就标记“未实机验收”，不能以 Linux 结果替代。
- [x] 独立审查接口、重入安全、性能报告、导出与截图变更范围，修复阻塞项；
  提交：`test(sdk): 验证页面动画容器的安装消费`。

## 任务 8：使用文档与最终验收

**文件：** 新建 `docs/development/ANIMATED_STACK_ZH.md`，更新上游对照清单和本计划勾选状态。

- [x] 文档提供添加页、设置模式／时长、切换、take、视频页禁用动画的完整调用例，
  说明 currentChanged 与动画结束的区别、最新请求合并、所有权和外部焦点规则。
- [x] 写入两版 Qt、各 DPR、X11、sanitizer、SDK、性能三轮和 CI 配置检查的实际结果；
  记录没有执行的原生平台验证，不使用“全部通过”代替明确测试范围。
- [x] 更新上游清单为“独立组件已完成，框架自动接入另行设计”，
  列出设计第 8 节的公开类型、WhileActive 和双重动画三项前置条件。
- [x] 运行文档审计、相对链接检查与 `git diff --check`；确认不存在误提交的报告或基线。
- [x] 最后提交文档：`docs(fluent): 记录页面动画容器使用与验收边界`。

## 完成定义

首版完成必须同时满足：公共组件可安装使用、示例不再维护重复动画、
快速切换与生命周期测试通过、输入／可访问性恢复正确、确定性资源门禁通过、
参考性能目标有实测证据、当前截图最终态与配置契约通过，且无独立审查阻塞项。
本计划本身通过文档检查不等于以上功能已经通过验证。

## 执行结果（2026-11-06）

用户批准后完成首版。提交依次为 `717f158`（所有权）、`10d2673`（快照动画）、
`3db2980`（重入与焦点）、`791fc93`（示例）、`e0dbcb0`（资源／性能测试）、
`fcd4d49`（SDK）、`3e3dc76`（报告 schema）、`ad542b6`（捕获降级焦点）。
实现均为本地 master 的线性提交，没有修改第三方或截图基线，没有推送。

- Qt 6.8.3／Qt 6.11.1 组件最终 80／81 行通过；两版 X11 与四档 DPR 通过，
  Qt 6.11 ASan＋UBSan 81 行及无障碍目标通过。
- 两版 Qt 的示例集成、语言、工作区和截图通过；既有 page-lifecycle、workspace-shell、
  workspace-public-api、Fluent 截图与公开头／架构检查通过。
- Qt 6.8 共享／静态 Development、完整安装、Starter 与 package-relocation 通过。
  包迁移初次因旧构建缓存缺少 ZZ_QT_PREFIX 停止，补齐后两种链接方式均通过；
  Qt 6.8 四个原本未构建的定向目标已补建并通过。
- 9 轮性能报告独立复审通过：最慢准备 p95 8.619 ms、Paint p95 1.244 ms，
  DPR 2 峰值 28.13 MiB，空闲零保留；报告明确记录源码 `3e3dc76` 和本机环境。
- 8 项 CI 配置契约通过。Windows／macOS 仅完成相关配置契约，未实机验收；
  不将 Linux 的类型降级用例表述为真实 GPU／视频或原生读屏器验证。
- 独立复审无剩余阻塞项；文档审计、相对链接及 diff 空白检查通过。

完整调用示例、逐轮数据和后续框架接入前置条件见
[页面动画容器使用与验收记录](../../development/ANIMATED_STACK_ZH.md)。
