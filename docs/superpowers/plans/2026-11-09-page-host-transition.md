# 页面动画框架接入实现计划

> **面向执行者：** 使用 executing-plans 在当前会话逐任务实施；实现不委派，独立审查可按
> requesting-code-review 进行。保留本地 master 线性提交，不建分支／worktree、不推送。
> 本文为计划，未勾选项目尚未执行；用户已确认库默认关闭、示例开启渐变。

**目标：** 给 PageHost 增加可选动画，保持同步导航、页面生命周期和工作区公开类型兼容。
**架构：** 从独立组件抽取内部快照播放器；PageHost 管理事务与普通 QWidget 宿主，
NavigationController 同步提交历史，Example 显式开启 Fade；工作区不增加另一层动画。
**技术栈：** C++20、Qt 6.8+ Widgets／Test、CMake、既有主题、性能与 SDK 验证设施。
**设计依据：** [详细契约](../specs/2026-11-09-page-host-transition-design.md)，核对基点 `a6e1c93`。

## 文件边界

| 路径 | 操作及职责 |
| --- | --- |
| `ZzFluentUI/widgets/src/private/ZzPageTransitionPrivate.h`、`.cpp` | 新增内部导出的捕获／播放引擎，不拥有页面 |
| `ZzFluentUI/widgets/src/private/ZzAnimatedStackedWidgetPrivate.h`、`.cpp` | 复用引擎，保留原有最新请求和生命周期策略 |
| `ZzFluentUI/widgets/src/private/ZzStackTransitionOverlay.h`、`.cpp` | 保留两图绘制、预算和输入隔离；按抽取需要收窄访问 |
| `ZzFluentUI/CMakeLists.txt`、`tests/CMakeLists.txt` | 源文件／私有测试注册 |
| `ZzFluentUI/tests/ZzPageTransitionPrivateTest.cpp` | 新增分阶段捕获、源销毁与资源回归 |
| `ZzFluentUI/tests/ZzAnimatedStackedWidgetTest.cpp` | 保持原容器行为，增加复用后信号与资源回归 |
| `ZzPureTools/widgets/include/ZzPureTools/ZzPageHost.h`、`widgets/src/ZzPageHost.cpp` | 新配置／方向重载／状态查询和环境事件入口 |
| `ZzPureTools/widgets/src/private/ZzPageHostPrivate.h`、`.cpp` | 事务、页面集合／LRU、受保护宿主、错误页 |
| `ZzPureTools/widgets/src/private/ZzPageHostTransitionPrivate.h`、`.cpp` | 新增主题、方向、焦点、环境事件与播放器适配 |
| `ZzPureTools/widgets/src/private/ZzNavigationControllerPrivate.h`、`.cpp` | 同步重入保护及 back／forward 方向 |
| `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationController.h` | 补充同步重入错误契约，不改变原方法签名 |
| `ZzPureTools/CMakeLists.txt`、`tests/CMakeLists.txt` | 适配器源文件及 `puretools.page-transition` 注册 |
| `ZzPureTools/tests/ZzPageTransitionTest.cpp` | 新增框架事务／像素／焦点／资源测试 |
| `ZzPureTools/tests/ZzPageLifecycleTest.cpp` | 去除私有 QStackedWidget 断言，保留销毁／缓存失败契约 |
| `ZzPureTools/tests/ZzNavigationControllerTest.cpp` | 错误页、历史与同步重入回归 |
| `ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp`、`ZzWorkspaceNavigationIntegrationTest.cpp` | 外层公开类型／对象身份与单层动画 |
| `examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp` | 在已有窗口装配中开启 Fade |
| `examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp` | 真实导航和前进后退行为 |
| `examples/ZzPureToolsExample/ZzExampleSmokeControllerPrivate.cpp` | 仅在截图流程实际依赖未完成动画时增加有界完成等待 |
| `benchmarks/ZzPageHostTransitionBenchmark.cpp`、`benchmarks/CMakeLists.txt` | 新增生命周期组合性能证据，不替代旧基准 |
| `tests/InstallConsumer/Gui/main.cpp` | PureTools 安装消费新接口及旧工作区类型验证 |
| `docs/development/PAGE_HOST_TRANSITION_ZH.md` | 实施最后新增使用及实际验收边界 |
| `docs/development/ANIMATED_STACK_ZH.md` | 最后更新框架接入状态和两种请求语义 |

不改 `ZzPageRegistration` 字段、`ZzPageInstance` 销毁顺序、AppCore 依赖或工作区路由结构。

## 任务 1：抽取私有快照播放器

- [ ] 新增 `fluent.page-transition-private` 测试目标；私有接口导出到 FluentUI 动态库，
  头仍位于 src/private 且不安装。测试链接真实 FluentUI；不能把同一导出实现编译进
  PureTools 以绕过链接问题。源文件注册进 `zz_fluent_ui_sources`。
- [ ] 定义内部接口（私有类按仓库规范使用 Pimpl；Mode 是内部枚举，不成为新公开 API）：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzPageTransitionPrivate final {
public:
    enum class Mode { None, SlideHorizontal, SlideVertical, Fade };
    struct Settings {
        QRect contentRect;
        qreal dpr = 1;
        QColor background;
        Mode mode = Mode::None;
        bool reverse = false;
        int duration = 0;
    };
    using Token = quint64;
    explicit ZzPageTransitionPrivate(QWidget *owner);
    ~ZzPageTransitionPrivate();
    void setFinishedCallback(std::function<void()> callback);
    Token beginCapture(const Settings &settings);
    bool captureSource(Token token, QWidget *view);
    bool captureTarget(Token token, QWidget *view);
    bool play(Token token);
    void cancel();
    bool running() const noexcept;
    qsizetype retainedBytes() const noexcept;
private:
    class ZzPrivate;
    std::unique_ptr<ZzPrivate> d;
};
```

- [ ] 测试创建 200×100 红／蓝页，beginCapture、两次捕获后 delete 红页，再 play。
  暂停单一 QVariantAnimation 到 duration/2，验证 OutCubic 中间像素仍包含源页红色；
  推到结束，断言 retainedBytes()==0、完成回调一次。另测 cancel 令旧 token 无法 play、
  超预算不分配、目标 render 触发 cancel 后两图归零、owner 销毁不调用悬空回调。
- [ ] 先运行新目标确认缺失捕获／token 行为失败，再抽取原捕获、预算与 Overlay 动画代码。
  抽取前后独立容器继续负责自己的 source destroyed 中断、pending 和信号配对。
- [ ] 改造 `ZzAnimatedStackedWidgetPrivate`，将图像和动画唯一交给播放器；pending drain 前
  释放失败 capture。完成回调在动画资源清空后进入原 finish 逻辑，取消本段时不得重复通知。
- [ ] 执行并确认通过后提交 `refactor(fluent): 抽取可分阶段捕获的页面播放器`：

```bash
cmake --build build/linux-gcc-debug --target ZzPageTransitionPrivateTest ZzAnimatedStackedWidgetTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^fluent\.(page-transition-private|animated-stack)$' --output-on-failure
```

预期：新测试通过；原 81 行全部保留通过，单动画、双图和原信号时序未变。
Qt 6.8.3 同命令使用 `build/spectrum-qt683-release`；其可选 Video 模块缺失导致原行为计数不同。

## 任务 2：默认关闭的 API 与 PageHost 宿主事务

- [ ] 按设计第 3 节在 PageHost 添加枚举和方法，原 `activate(registration)` 符号保留。
  初始实现设置／查询配置、None 路径，增加 ZzPageTransitionTest 及 `puretools.page-transition`。
- [ ] 测试默认 None／-1、无效 enum／时长／route、跨线程、禁用路由在重建后保持。
  已有 `zzCountingRegistration` 可在新测试文件复制为本地具名 fixture，保存 factory 调用次数、
  QPointer View、唯一父对象、取消与销毁序列；不让测试持有会失效的 QWidget 裸观察指针。

```cpp
ZzPureTools::ZzPageHost host;
using Host = ZzPureTools::ZzPageHost;
QCOMPARE(host.transitionMode(), Host::ZzTransitionMode::None);
QCOMPARE(host.transitionDuration(), -1);
QVERIFY(host.setTransitionMode(Host::ZzTransitionMode::Fade));
QVERIFY(host.setTransitionDuration(200));
QVERIFY(!host.setTransitionDuration(-2));
QCOMPARE(host.transitionDuration(), 200);
QVERIFY(!host.setRouteTransitionEnabled(ZzPureTools::ZzRouteId{}, false));
```

- [ ] 修改生命周期测试的私有类型依赖：通过 factory 记录 pageParent；比较 children 数、
  View 可见性、当前路由及 liveViews，不以 `findChild<QStackedWidget *>()` 作为功能契约。
  在改产品实现前运行这些测试，确认仅更换观察方式时仍通过。
- [ ] 加入 Hide／Show 回调删除目标或调用 activate、factory 重入、宿主同步销毁的失败回归。
  事务内 activate／setRecreatableCapacity／showFrameworkError 及新配置操作返回 InvalidState，
  重入 deactivateCurrent 无操作。先确认原实现的失败行为，然后替换内部 QStackedWidget
  为稳定 QWidget 宿主及受保护的显隐／尺寸同步。
- [ ] 新目标创建先局部持有，factory 成功后才修改 pages／LRU；所有可重入调用后重新找路由，
  不保留 map iterator；移走／删除页面及时清理失效记录。析构先停止和断开展示回调。
  Constructor 在 body 赋值 d_ptr，避免 child event 访问未初始化私有状态。
- [ ] 运行生命周期、新目标和公开 API 测试，提交 `refactor(puretools): 保护页面宿主的同步激活事务`：

```bash
cmake --build build/linux-gcc-debug --target ZzPageLifecycleTest ZzPageTransitionTest ZzWorkspacePublicApiTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^puretools\.(page-lifecycle|page-transition|workspace-public-api)$' --output-on-failure
```

预期：None 路径仍满足 factory 失败回滚和 LRU 容量，原工作区返回类型未变。

## 任务 3：WhileActive 截图与播放接入

- [ ] 新增 ZzPageHostTransitionPrivate，私有依赖上一步播放器；主题通过既有 FluentStyle／
  ZzWidgetTheme 获取，非 Fluent 样式走回退；将模式映射封装在适配器 cpp。
  适配器以 ZZ_PURE_TOOLS_EXPORT 内部导出，采用 Pimpl，头不安装；PageHost 给该适配器及
  ZzNavigationControllerPrivate 增加 friend，供私有事务访问。
- [ ] 参数化 Persistent／WhileActive／Recreatable，容量 0／1／3，None／三种动画、前后方向、
  RTL 和 contentsMargins。针对 WhileActive 编写以下真实行为断言并先运行失败：

```text
activate(A: WhileActive)，A 中持有 View、Presenter、ViewModel 的 QPointer。
显示宿主后 activate(B: Persistent, Forward)。
返回时 currentRoute == B；A 的三个 QPointer 全空，取消回调一次；动画仍 running。
暂停到中间帧：能观察 A 的红色图像，B 的蓝色图像，真实 B 暂不可见。
结束时仅 B 可见，图像字节零，销毁顺序仍为 View → Presenter → ViewModel。
```

- [ ] 按设计第 5 节实现先完成 factory、结束旧视效、捕获源／隐藏目标、销毁旧页、提交目标、
  安装快照的顺序。capture 资格失败只降级视效，不使合法路由激活失败。
  目标被外部回调销毁返回 InvalidState，不再播放；环境中断只取消 token 并直接显示目标。
- [ ] 测试连续 A→B→C→D，每次 activate 返回立即 currentRoute 正确、每个 WhileActive
  离开时已销毁；一次最多一段两图，旧图释放早于新图分配。测试同路由重复请求不重启动画。
- [ ] 测试开播前外部源删除、目标删除、父对象变化、目标捕获中 resize／reducedMotion；
  回调主动删除已有页不要求复活原页，但不能留下失效注册或释放后的指针。
- [ ] 两版 Qt 新目标、独立容器、生命周期通过后提交 `feat(puretools): 为页面宿主接入有界切换动画`。

## 任务 4：导航历史、失败页与重入

- [ ] 在 ZzNavigationControllerTest 增加开启动画后的失败矩阵；原错误语义先在 None 下运行确认。
  已有 failedPageCreationShowsFrameworkErrorPage、failedForwardKeepsCurrentRouteAndHistory
  等测试继续保留，并把动画模式作为覆盖维度。
- [ ] navigate／goForward 使用新 Forward 重载，goBack 使用 Backward。添加控制器事务守卫
  和 PageHost 私有忙状态预检，覆盖 factory、展示回调以及导航信号发射期；
  忙状态直接返回 InvalidState，不走 framework error 与 navigationFailed 分支。
  守卫对对象销毁安全，signals 后用 QPointer 判断是否能继续 historyStateChanged。
- [ ] 在 currentRouteChanged 回调中调用 goBack，断言嵌套返回 InvalidState；外层完成后再
  goBack 成功。回调直接删除 controller／host 时验证无后续访问及重复通知。

```text
navigate(A) 成功；navigate(B) 成功；navigate(C) 成功（B 的动画尚未结束）。
每次返回 host.currentRoute == controller.currentRoute。
goBack => B；再次 goBack => A；goForward => B。
设 C factory 失败：goForward 失败但 B 与双向历史不变。
直接 navigate(C) 失败：显示 C 的框架错误页，结束快照；沿用原历史恢复语义。
```

- [ ] 同一窗口出现多个 PageHost 时各自独立；从另一个窗口返回的导航事件不能改变本窗口
  历史或释放其快照。运行 navigation、page-lifecycle 与 page-transition：

```bash
cmake --build build/linux-gcc-debug --target ZzNavigationControllerTest ZzPageTransitionTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^puretools\.(navigation|page-lifecycle|page-transition)$' --output-on-failure
```

- [ ] 完成独立生命周期／API 审查，修复阻塞问题后提交 `fix(puretools): 保持动画导航的历史与错误页语义`。

## 任务 5：环境、焦点、输入与工作区／示例

- [ ] 新增环境参数化测试：resize、contentsMargins、hide、disable、deactivate、RTL、DPR、
  palette、style、font、language、减少动态效果、模式／时长与涉及路由禁用。
  断言 currentRoute 不回退、running false、仅已提交页可见、快照为零。
- [ ] 在真实顶层窗口中测试：输入原在 A 时过渡暂聚焦宿主、结束恢复 B；
  显隐回调主动移动外部焦点则不抢回；None／捕获失败没有残留临时焦点。
  隐藏真实页不接收 overlay 区域点击／滚轮／Tab；保留原 disabled 状态。
  QAccessible 检查过渡中无两套可见页树，结束目标名称／子项恢复。
- [ ] 可选模块检测沿用真实 OpenGL／Quick／Video 类型用例，增加按路由禁用及重新创建。
  页面含正在播放的嵌套 ZzAnimatedStackedWidget 时宿主本段直接切换；静止嵌套允许截图。
- [ ] 工作区回归断言 `stackWidget()` 的静态类型与实例身份：

```cpp
static_assert(std::is_same_v<
    decltype(std::declval<ZzPureTools::ZzWorkspaceShell &>().stackWidget()),
    QStackedWidget *>);
```

  在现有 WorkspaceNavigationIntegration fixture 记录外层堆栈 QPointer，
  导航、迁移宿主、关闭／重开窗口后核对所属窗口身份；外层不创建页面动画。
- [ ] Example 的 ZzExampleWindowShellPrivate::initialize 在校验 window／host 后显式开启：

```cpp
auto configured = window->pageHost()->setTransitionMode(
    ZzPureTools::ZzPageHost::ZzTransitionMode::Fade);
if (!configured) return configured;
```

  在 workspace-smoke 中覆盖正常导航、Back／Forward、快速导航、中文／英文、RTL、
  关闭重开和 reducedMotion。截图已有异步等待处增加 `!host->isTransitionRunning()` 条件；
  不固定 sleep，不把动画全局关掉以绕过真实接入测试。
- [ ] 两版 Qt offscreen、新目标 X11 和四档像素专项通过后提交 `feat(example): 启用框架页面导航渐变`：

```bash
cmake --build build/linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest ZzPageTransitionTest ZzWorkspaceNavigationIntegrationTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^(puretools\.(page-transition|workspace-navigation-integration)|example\.(workspace-smoke|puretools-integration(-english)?|puretools-screenshot-.*))$' --output-on-failure --parallel 2
QT_QPA_PLATFORM=xcb xvfb-run -a build/linux-gcc-debug/ZzPureTools/tests/ZzPageTransitionTest -nocrashhandler
```

新测试的 `rendersFrames` 数据函数覆盖模式／方向／RTL；专项按
`QT_SCALE_FACTOR=1/1.25/1.5/2` 分别执行，`QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough`。

## 任务 6：资源、性能与 sanitizer

- [ ] 固定四页轮换 1,000 次，每次立即核对当前 route、可回收页面数和销毁顺序，
  100 次快速导航核对最后目标及历史容量。统计组件唯一动画、两图峰值与 idle 零字节；
  真实页面 paint 计数不能随逐帧时间推进增长。统计不包含业务页自有动画。
- [ ] 新增 ZzPageHostTransitionBenchmark，按下述两个策略注册独立 CTest 入口。
  每页固定 1280×720、12 行 QLabel／QLineEdit 和一个 QPushButton；参数
  `--policy persistent`／`--policy while-active` 分别采样，预热 10 次、正式 100 次，
  DPR 1／1.5／2 各三轮，共 18 份报告。CTest 注册两个 policy 数据入口，名称分别为
  `benchmark.page-host-transition-persistent` 和 `benchmark.page-host-transition-while-active`。
  QWidget factory 构造时间计入 `activate_total_ms`；内部 capture 区间包含图像分配与两次
  render，报告 `snapshot_prepare_ms`；应用 notify 包围 Overlay Paint 得到 `overlay_paint_ms`。
  私有播放器提供 `setCaptureObserver(std::function<void(bool entering)>)`，在双图捕获边界
  配对调用；默认空回调，不计时、不分配日志。基准回调使用 QElapsedTimer，
  失败／取消也结束观测，observer 重入需按原 token 失效规则处理。
  该入口不安装、不进入 PageHost 公开头；无业务 factory 执行在此捕获区间中。
  `ZzPageHostTransitionPrivate` 提供内部静态桥
  `observeCapture(ZzPageHost *, std::function<void(bool)>)` 及
  `snapshotBytes(const ZzPageHost *)`；经 friend 找到宿主适配器后委托播放器。
  基准包含该私有头、链接真实 PureTools／FluentUI，不重复编译播放器实现。
- [ ] 复用 ZzPerformanceReporter 固定环境 schema，不添加第 11 个环境字段。
  用场景名 `page-host-transition-persistent`／`page-host-transition-while-active` 区分策略，
  记录峰值／空闲快照和销毁计数；
  分别对两种策略检查准备 p95 ≤16.7 ms、Paint p95 ≤8 ms，资源违规直接失败。
  构建停止后顺序采样，保存实际 Qt kit、提交、DPR 与环境摘要，不覆盖旧参考报告。
- [ ] 运行 ASan＋UBSan：

```bash
cmake --build build/linux-clang-asan --target ZzPageTransitionTest ZzPageLifecycleTest ZzNavigationControllerTest ZzAnimatedStackedWidgetTest --parallel 2
ctest --test-dir build/linux-clang-asan -R '^(puretools\.(page-transition|page-lifecycle|navigation)|fluent\.animated-stack)$' --output-on-failure
cmake --build build/linux-gcc-benchmarks --target ZzPageHostTransitionBenchmark --parallel 2
```

- [ ] 审查完整 prepare／destroy／play 顺序及 18 份原始报告，达标后提交
  `test(puretools): 验证框架页面动画的生命周期与资源预算`。不以动画平均帧率代替 CPU Paint 证据。

## 任务 7：SDK、CI 和最终文档

- [ ] InstallConsumer/Gui 使用 PageHost None 与 Fade 配置、新方向重载及路由禁用；
  静态断言 WorkspaceShell 旧返回类型。外部只链接 PureTools 的消费者不能被迫手动链接
  FluentUI 或包含新私有头；共享和静态链接都必须验证内部播放器符号可解析。
- [ ] 构建所有即将运行的测试目标，避免把 Not Run 当成通过。两版 Qt 执行独立容器、
  PageHost／导航、工作区、无障碍、四档 Fluent／Example 截图、公开头与完整架构检查。
- [ ] Qt 6.8 共享／静态运行现有 install.consumer 与 package-relocation，覆盖 Development、
  完整安装、Starter 与迁移。先检查 CMakeCache 的 ZZ_QT_PREFIX 为对应真实 kit，
  不新增嵌套构建路径；使用既有两个并行编译限制。

```bash
ctest --test-dir build/spectrum-qt683-release -R '^(install.consumer|platform.package-relocation|architecture.public-headers)$' --output-on-failure
ctest --test-dir build/spectrum-static-sdk -R '^(install.consumer|platform.package-relocation|architecture.public-headers)$' --output-on-failure
```

- [ ] 完整运行 CI 配置八项：

```bash
set -e
cmake -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" -P tests/Platform/PresetMatrixContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzGateScriptContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzGitHubActionsContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -DZZ_TEST_ROOT="$PWD/build/contracts/release-packaging-support" -P tests/Platform/ZzReleasePackagingSupportContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzLinuxAppImagePackagingContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzWindowsPackagingContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzMacosPackagingContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzContinuousReleaseContract.cmake
```

- [ ] 独立代码审查确认公开依赖、同步语义、WhileActive 销毁、重入、资源和报告，修复阻塞项。
  提交 `test(sdk): 验证框架页面动画的安装与兼容性`。
- [ ] 最后创建 PAGE_HOST_TRANSITION_ZH.md，提供默认关闭／显式 Fade、时长、路由禁用、
  直接 activate 与 Controller 错误页差异、连续请求立即提交及同步重入拒绝的完整示例。
  更新 ANIMATED_STACK_ZH.md 的框架接入进展，记录每种 kit／平台的真实执行范围。
  Windows／macOS 无原生环境标注未实机验收，不能从 Linux 结果推导。
- [ ] 更新本计划勾选状态，运行 architecture.complete-audit、documentation-audit、
  改动 Markdown 相对链接检查及 `git diff --check`；确认没有提交 build 报告或参考 PNG。
  最后提交 `docs(puretools): 记录框架页面动画的使用和验收边界`。

## 完成定义

默认应用行为兼容、Example 明确开启、同步路由与历史正确、WhileActive 不延寿、
错误页语义不变、资源门禁与性能目标有实测证据、SDK 和既有回归通过且独立审查无阻塞。
本计划的文档检查通过不代表产品能力已经完成；本次只交付计划，不执行以上实现步骤。
