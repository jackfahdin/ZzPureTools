# 框架页面导航动画

`ZzPureTools::ZzPageHost` 已接入可选的快照动画，支持 None、Fade、SlideHorizontal 和
SlideVertical。库默认 None，`ZzPureToolsExample` 在窗口装配中显式开启 Fade。
逻辑导航、历史及页面生命周期始终同步完成，动画仅负责呈现已提交的切换。

## 使用接口

安装消费只需链接 `Zz::PureTools`，不用手动增加 FluentUI 或包含私有播放器头。
所有操作应在宿主所属 GUI 线程执行。下面的配置函数可以直接用于已有宿主：

```cpp
#include <ZzPureTools/ZzPageHost.h>

ZzCore::ZzResult<void> configureTransitions(ZzPureTools::ZzPageHost &host)
{
    using Host = ZzPureTools::ZzPageHost;
    auto result = host.setTransitionMode(Host::ZzTransitionMode::Fade);
    if (!result) return result;
    result = host.setTransitionDuration(-1); // 主题 PageTransition 时长。
    if (!result) return result;
    // 在首次创建前配置即可；页面回收、重建后仍生效。
    return host.setRouteTransitionEnabled(
        ZzPureTools::ZzRouteId(QStringLiteral("video")), false);
}
```

时长范围为 `[-1, 10000]` ms，默认 `-1` 使用主题时长，`0` 直接切换。
无效枚举、时长或路由返回 `InvalidArgument`，不会裁剪输入；跨线程修改返回
`InvalidState`。修改模式、时长或禁用本段涉及的路由会结束动画，显示已提交页面；
无关路由的开关不打断本段。路由开关同时约束作为源页和目标页的捕获资格。

原单参数 `activate(registration)` 保留，默认方向为 Forward。直接指定方向的示例：

```cpp
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>

ZzCore::ZzResult<void> openDetails(ZzPureTools::ZzPageHost &host)
{
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(QStringLiteral("details"));
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    page.factory = [](QWidget *pageParent) {
        return ZzPureTools::ZzPageInstance::create(
            pageParent, new QWidget(pageParent),
            std::make_unique<QObject>(), std::make_unique<QObject>());
    };
    return host.activate(page,
        ZzPureTools::ZzPageHost::ZzTransitionDirection::Forward);
}
```

`ZzNavigationController::navigate()` 和 `goForward()` 使用 Forward，`goBack()` 使用
Backward。横向 Forward 在 LTR 从右进入、RTL 从左进入；纵向 Forward 从下进入，
不随 RTL 镜像。Fade 忽略方向。`isTransitionRunning()` 只查询视效状态，
`currentRouteChanged` 仍表示同步逻辑导航，不表示动画结束。

## 同步提交、生命周期与失败

连续请求 A→B→C→D 时，每次成功返回都已提交对应路由和历史。新请求成功创建目标后，
结束旧视效并重新捕获；没有延后的业务导航队列。同路由重复激活不重启动画。
独立的 [ZzAnimatedStackedWidget](ANIMATED_STACK_ZH.md) 则保留最新待切请求，
两者复用绘制播放器，但请求语义不同。

| 操作或情况 | 结果 |
| --- | --- |
| `PageHost::activate()` 的 factory 失败 | 原路由、缓存、页面及尚在播放的旧段保留 |
| Controller 普通 `navigate()` 的 factory 失败 | 沿用原契约显示框架错误页，结束快照并发送失败通知 |
| Controller `goBack()`／`goForward()` 的 factory 失败 | 原页面和双向历史保留 |
| 同步 factory、显隐、销毁或导航通知回调中重入修改 | 返回 `InvalidState`；不另开错误页或发重复失败通知 |
| 事务内重入 `deactivateCurrent()` | 无操作 |
| 目标在回调中被销毁或移出宿主 | 安全失败或清理失效记录，不播放失效快照 |
| 环境变化中断已经提交的切换 | 结束快照，显示最新已提交页面，不回退路由 |

factory 成功后先捕获源、目标，再按原策略销毁旧实例，最后提交并播放。
WhileActive 和零容量 Recreatable 页面在 `activate()` 返回前完成取消及
View→Presenter→ViewModel 销毁；动画显示的是已捕获图像，不延长业务对象寿命。
Persistent 保留实例，Recreatable 继续使用原有有界 LRU。

需要从导航通知再发起导航时，应投递到下一轮事件循环。例如，以下函数用于已经
注册了 `details` 路由的 controller，避免在 `currentRouteChanged` 回调中同步重入：

```cpp
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <ZzPureTools/ZzNavigationController.h>

void connectDeferredNavigation(ZzPureTools::ZzNavigationController *controller)
{
    QObject::connect(controller,
        &ZzPureTools::ZzNavigationController::currentRouteChanged,
        controller, [controller](const ZzPureTools::ZzRouteId &route) {
            if (route != ZzPureTools::ZzRouteId(QStringLiteral("redirect"))) return;
            QTimer::singleShot(0, controller, [controller] {
                const auto result = controller->navigate(
                    ZzPureTools::ZzRouteId(QStringLiteral("details")));
                if (!result) qWarning() << result.error().technicalMessage();
            });
        });
}
```

## 焦点、输入、降级和资源

播放时隐藏真实页面，由覆盖层隔离本区域输入。源页原本持有焦点时，临时聚焦宿主，
结束后恢复目标页记忆焦点或可聚焦子项；回调主动移到外部的焦点不会被抢回。
PageHost 保留原有 `Qt::NoFocus` 默认策略，不增加默认 Tab 停靠点；业务控件原来的
enabled 状态保留。过渡中不暴露两套可见的无障碍页面树，结束后恢复真实目标树。

None、零时长、减少动态效果、隐藏／禁用宿主、无有效内容区域、超出预算、路由禁用时
直接切换。OpenGL／Quick／Video／原生窗口子树保守降级；其他不适合截图的第三方
渲染器应使用路由开关。嵌套 `ZzAnimatedStackedWidget` 正在播放时，外层本次直接切换；
静止嵌套容器允许截图。尺寸、内容边距、DPR、主题、字体、语言、RTL、显隐、禁用及
窗口失活等变化会清理旧快照。

每个宿主固定一个 QVariantAnimation，最多两张 ARGB32 快照，总计不超过 64 MiB；
每帧只绘制快照，不重新 render 业务页。取消、结束或销毁后快照字节归零。
不同窗口的宿主、历史和图像独立。预算不包含业务资源、Qt 后端缓存或进程 RSS。
`ZzWorkspaceShell::stackWidget()` 仍返回同一外层 `QStackedWidget *`，工作区不增加
第二层导航动画。内部 QWidget 宿主及播放器均不属于公共 SDK 接口。

## 2026-11-09 验收证据

产品实现核对到 `753f808`，SDK 消费验证为 `9ab9b51`，基准修复和正式采样为
`f23ff8d32039d760c4bd9c581678a8512c2063b7`。测试行数包含初始化／清理。

| 验证范围 | 结果与本地证据 |
| --- | --- |
| PageHost | 两版 Qt 的 X11 全部 92 行通过；生命周期×容量×模式、方向／RTL、环境、重入、销毁、输入焦点、资源均覆盖；`build/page-host-x11-final.log`、`build/page-host-x11-qt683-final.log` |
| 像素专项 | Qt 6.11.1／6.8.3，DPR 1／1.25／1.5／2 各 14 行通过；`build/page-host-linux-gcc-debug-dpr-*.log`、`build/page-host-qt683-final-dpr*.log` |
| Qt 6.11.1 Debug | 独立容器、共享播放器、导航和生命周期通过；工作区／无障碍／Fluent 与 Workspace 四档截图 86 项通过；`build/page-host-regression.log` |
| Qt 6.8.3 Release | 最终集合 98 项分批通过；初次 94 项通过、4 项因未构建而未运行，补建后 4 项及完整架构检查通过；`build/page-host-qt683-final-test.log`、`build/page-host-qt683-completion-test.log` |
| Example | 两版 Qt 中英文全路由、workspace smoke 和四档截图通过；中文／英文真实渐变全路由约 11～12 秒，集成测试总 timeout 为 30 秒；`build/page-host-default-focus-green.log`、`build/page-host-example-final-visual.log`、Qt 6.8 最终日志 |
| ASan＋UBSan | 共享播放器、独立容器、PageHost、导航、生命周期五组通过；最后焦点与导航边界复验通过；`build/page-host-asan-final-test.log`、`build/page-host-asan-boundaries-test.log` |
| 资源压力 | 1,000 次页面轮换、100 次快速导航及历史容量、多宿主独立图像、空闲零字节通过 |
| Qt 6.8 共享／静态 SDK | 各自 install.consumer、public-headers、package-relocation 三项通过，含 Development、完整安装、Starter 与迁移；`build/page-host-sdk-shared.log`、`build/page-host-sdk-static.log` |
| CI 配置 | preset、gate scripts、GitHub Actions、release packaging、Linux AppImage、Windows、macOS、continuous release 八项静态契约通过 |

独立审查覆盖同步提交、销毁与重入、SDK 公共依赖、基准计时和原始报告。
Linux 的自动化证据不替代真实 GPU、视频、读屏器、跨显示器 DPI 或原生输入验收。
Windows MSVC／MinGW 和 macOS 尚未实机验收，仅完成相关静态契约检查。
没有修改截图基线、遮罩、像素门限或性能门禁，也没有修改第三方源码。

## 性能采样与复现

两种生命周期策略均使用固定 1280×720 表单，每页 12 行 QLabel／QLineEdit 和一个
QPushButton。每轮预热 10 次、正式 100 次切换，每次 10 个实际 Overlay Paint。
`activate_total_ms` 包含 factory、销毁和完整同步调用；`snapshot_prepare_ms` 由私有
捕获观察入口记录两图分配、布局和 render，排除业务 factory；`overlay_paint_ms` 由
QApplication::notify 包围实际 Paint 分发，表示 CPU 绘制时间，不是合成帧率。

本机 i7-14700、约 30.44 GiB 内存、Ubuntu 26.04.1、GCC 15.2、Qt 官方 6.11.1 GCC kit；
Release、共享库、LTO，Xvfb 1920×1080×24、Mesa llvmpipe 26.0.8。
Xvfb 固定 CPU 8，基准固定 CPU 10；所有构建停止后，在上述干净提交上顺序运行。

| 策略 | DPR | 准备 p95，三轮（ms） | Paint p95，三轮（ms） | 两图峰值（MiB） | 空闲字节 |
| --- | --- | --- | --- | --- | --- |
| Persistent | 1 | 4.318／4.324／4.592 | 0.397／0.391／0.422 | 7.03125 | 0 |
| Persistent | 1.5 | 2.101／2.064／2.336 | 4.847／4.805／4.844 | 15.8203125 | 0 |
| Persistent | 2 | 4.573／4.646／4.598 | 2.930／2.908／2.972 | 28.125 | 0 |
| WhileActive | 1 | 5.405／5.367／5.358 | 0.437／0.397／0.388 | 7.03125 | 0 |
| WhileActive | 1.5 | 4.819／4.602／4.629 | 4.873／4.846／4.844 | 15.8203125 | 0 |
| WhileActive | 2 | 8.902／8.858／8.869 | 2.850／2.895／2.878 | 28.125 | 0 |

18 轮均满足准备 p95 ≤16.7 ms、Paint p95 ≤8 ms。每份报告有 100 个准备样本和
1,000 个 Paint 样本；WhileActive 每次返回均验证旧 View 已销毁，包含预热最终为
110 次，Persistent 为 0。业务 factory 的任意工作负载不受此固定表单性能保证。

原始报告为 `build/page-host-{persistent,while-active}-dpr{1,1.5,2}-round{1,2,3}.json`。
独立环境档案 `build/page-host-transition-environment.json` 的 SHA-256 为
`467392ab22bbcd4f0570768b9b0c7f0329d0a41e0bfcb5d7be90929ab08eba14`；报告内十个
环境字段、源码提交与真实 Qt kit 一致，未借用仓库 patched Qt 参考环境摘要。
这些文件均为本机构建产物，不作为历史相对基线提交。首次采样发现基准使用无上下文
destroyed 观察连接而被展示对象断连移除；改为宿主 context 后重新采集全部 18 份。
`build/page-host-pre-observer-fix/` 和 diagnostic 报告仅保留排错记录，不作为上述证据。

重新采样时，先构建，再设置当前干净提交、真实环境摘要及 GPU 信息：

```bash
cmake --build build/linux-gcc-benchmarks --target ZzPageHostTransitionBenchmark --parallel 2
export ZZ_BENCHMARK_COMMIT="$(git rev-parse HEAD)"
export ZZ_CMAKE_PRESET=linux-gcc-benchmarks
export ZZ_RUNNER_IMAGE_DIGEST="sha256:$(sha256sum build/page-host-transition-environment.json | cut -d ' ' -f 1)"
export ZZ_GPU_IDENTITY='Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.8 Xvfb 1920x1080x24'
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough \
  taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' taskset -c 10 \
  build/linux-gcc-benchmarks/benchmarks/ZzPageHostTransitionBenchmark \
  --policy while-active --report build/page-host-while-active-new-run.json
```

上例环境信息只适用于本机档案，换环境需重新记录。两个独立 CTest 入口为
`benchmark.page-host-transition-persistent`、`benchmark.page-host-transition-while-active`，
使用相同元数据环境；本次两个入口均通过，记录在 `build/page-host-performance-runs.log`。
耗时与资源门限直接影响进程退出码，不以平均值替代 p95，也不替换既有性能场景。

设计与任务完成记录见[框架接入设计](../superpowers/specs/2026-11-09-page-host-transition-design.md)
和[实现计划](../superpowers/plans/2026-11-09-page-host-transition.md)。
