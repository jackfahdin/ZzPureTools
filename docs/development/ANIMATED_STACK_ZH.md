# 页面动画容器

`ZzFluentUI::ZzAnimatedStackedWidget` 是独立的 QWidget 页面容器，支持直接切换、
横向滑动、纵向滑动和渐变。标签栏示例已使用它；页面路由、factory、缓存及
Presenter／ViewModel 生命周期仍由应用或原框架管理。

## 安装与使用

安装后链接 `Zz::FluentUI`，包含 `ZzFluentUI/ZzAnimatedStackedWidget.h`。
所有操作在 GUI 线程执行。下面的函数可放入已有 Qt Widgets 应用：

```cpp
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

ZzFluentUI::ZzAnimatedStackedWidget *createPages(QWidget *parent)
{
    using Stack = ZzFluentUI::ZzAnimatedStackedWidget;
    auto *pages = new Stack(parent);
    pages->setTransitionMode(Stack::ZzTransitionMode::SlideVertical);
    pages->setDuration(-1); // PageTransition 主题时长；0 为直接切换。

    auto *overview = new QWidget;
    auto *layout = new QVBoxLayout(overview);
    layout->addWidget(new QLineEdit(QStringLiteral("Overview")));
    pages->addWidget(overview);

    auto *details = new QWidget(pages);
    pages->addWidget(details);
    pages->setCurrentWidget(details, Stack::ZzTransitionDirection::Forward);

    // 实际应用可在此页放置视频或其他不适合快照的渲染器。
    auto *videoPage = new QWidget;
    pages->addWidget(videoPage);
    pages->setPageAnimationEnabled(videoPage, false);

    // take 不销毁页面；调用方取得无父对象、隐藏的页面并负责后续所有权。
    QWidget *returned = pages->takeWidget(videoPage);
    delete returned;
    return pages;
}
```

容器只接管无父对象或直接以自身为父对象的页面；拒绝其他父对象、跨线程、
自身和祖先。重复添加返回原索引。移除当前页时优先选择同一索引的后继页，
没有后继则选择前驱；外部销毁或重新设置父对象也会移除注册记录。
页面由容器销毁，除非先 `takeWidget()` 或移交给其他父对象。

## 通知与连续请求

`currentChanged(index)` 表示逻辑当前页已提交，快照动画可能仍在播放。
插入／移除导致当前页索引改变时也会通知。`transitionStarted()` 表示本段快照
安装完成，`transitionFinished()` 表示真实页面恢复且快照已释放；直接切换不发
动效信号。存活容器的每个已开始段有一次结束通知，销毁容器会停止动画。

例如 A→B 进行中依次请求 C、D，只播放 A→B，然后播放 B→D；C 不会成为逻辑当前页。
再次请求当前目标 B 会清空待切目标，无效请求不覆盖已有有效目标。
结束信号回调中的新请求优先于之前排队的目标。待切目标保存页面身份，索引变化不会误切页。
源页或当前目标被删除、取回或移走时终止本段、清空待切目标并选择存活页面。

Horizontal 的 Forward 在 LTR 从右进入、RTL 从左进入；Vertical 的 Forward 从下进入。
Automatic 根据源／目标的逻辑顺序决定方向，Fade 忽略方向。默认使用横向滑动，
时长 `-1` 表示主题 `PageTransition`，显式时长限制为 `[-1, 10000]` ms。

## 中断、焦点和降级

尺寸、内容边距、DPR、主题、字体、语言、布局方向、显隐、禁用和窗口失活会停止
旧快照并直接收敛到最后有效请求。改变模式／时长或禁用当前涉及页的动画同样收敛；
禁用无关页不会打断当前段。

动画中真实页面宿主隐藏，覆盖层接收本区域的鼠标、触摸与滚轮输入，不改变业务页
的 enabled 状态。仅原页面拥有焦点时才临时聚焦容器；结束后尝试恢复目标页记忆焦点，
没有则选择首个可聚焦子项。用户或页面显隐回调主动移到外部的焦点不会被抢回。
快照不复制可访问页面树，结束后恢复真实页的可访问名称及子项。

None、零时长、减少动态效果、隐藏／空区域、显式页面禁用、预算不足时直接切换。
通过类型继承与原生窗口属性识别 OpenGL／Quick／Video／窗口容器子树并保守降级，
不为产品库新增这些 Qt 模块的强制依赖。其他第三方渲染器应显式关闭页面动画。

## 实际结构与资源约束

私有实现采用受保护的普通 QWidget 宿主，逐页维护几何和显隐；覆盖层是宿主的兄弟。
原设计中的 QStackedLayout 在旧页 Hide 回调同步切页／删除目标时会继续使用旧局部目标，
因此改用 `QPointer` 存活检查、修改深度和请求代次维护事务。回归覆盖这些真实 Qt 回调。

组件固定拥有一个 QVariantAnimation，最多两张 ARGB32 图像及一个待切 QPointer。
捕获前检查物理尺寸及两图总计 64 MiB 上限；降级路径在消费待切请求之前释放临时图像。
每帧只合成固定快照，不重新 render 真实页、不新建动画。停止后图像占用为零。
此预算不包含业务页面资源、Qt 后端缓存或整个进程 RSS。

## 2026-11-06 验收证据

功能代码核对到 `ad542b6`，性能采样基点为 `3e3dc76`。本机 Linux，Qt 6.8.3 Release 和 Qt 6.11.1 Debug；
测试行数含初始化／清理，区别来自可用的 Qt 可选模块。

| 验证 | 实际结果 |
| --- | --- |
| 组件测试 | Qt 6.8：80 行；Qt 6.11：81 行，通过 |
| X11／Xvfb | 两版 Qt 分别 80／81 行通过，含焦点、删除与回调重入 |
| 中间帧像素 | 两版 Qt 在 DPR 1／1.25／1.5／2 各 14 行通过 |
| ASan＋UBSan | Qt 6.11，组件 81 行通过；无障碍目标通过 |
| 快照限制 | 1,000 次切换、100 次排队请求、整数／预算边界及结束后零保留通过 |
| 可选类型 | Qt 6.8 OpenGL／Quick，Qt 6.11 OpenGL／Quick／Video 降级通过 |
| 示例 | 两版 Qt 的接入、中英文集成、工作区和四档截图各 8 个目标分批通过 |
| 既有组件与框架 | 两版 Qt 的无障碍、四档 Fluent 截图、page-lifecycle、workspace-shell、workspace-public-api、公开头及完整架构审计通过 |
| SDK | Qt 6.8 共享与静态的 Development／完整安装、单头编译、Starter 消费与包迁移均通过 |
| CI 配置 | preset、gate、Actions、发布打包支持、Linux／Windows／macOS 打包和连续发布 8 项契约通过 |

Windows MSVC／MinGW 和 macOS 未实机验收。Linux 的类型降级检查也不代替真实 GPU、
视频播放、读屏器、跨显示器 DPI 与原生平台输入验收。没有修改截图基线、遮罩或性能门限。

包迁移首次运行发现两个旧构建目录的 `ZZ_QT_PREFIX` 为空；补齐真实 Qt 6.8.3 SDK 路径后，
共享／静态迁移检查均通过。sanitizer 与性能目录中旧 qmsetup 安装缺失私有构建辅助文件，
由原构建目录重新安装依赖后恢复构建；未修改第三方源码。
Qt 6.8 定向回归首次有四个目标未构建，补建后全部通过；21 个定向目标的分批记录见
`build/animated-stack-final-regression-683.log` 与 `build/animated-stack-final-targets683.log`。
Qt 6.11 记录为 `build/animated-stack-regression-611.log`、
`build/animated-stack-example-regression.log`，最终组件和 sanitizer 记录为
`build/animated-stack-{linux-gcc-debug,linux-clang-asan}-focus-final.log`。

### 性能采样

参考场景固定 1280×720，两页各 12 行 QLabel／QLineEdit 与一个 QPushButton；
每轮预热 10 次、采样 100 次切换，每次采集 10 个进度的 Overlay Paint。
完整切页调用包含两张图像分配与 QWidget render，绘制耗时由基准 QApplication 的
`notify()` 包围实际 Paint 分发。它是 CPU 绘制时间，不等同于系统合成帧率。

本机 i7-14700、约 30.44 GiB 内存、Ubuntu 26.04.1、GCC 15.2、Qt 官方 6.11.1 GCC kit，
Release、共享库、LTO；Xvfb 1920×1080×24、Mesa llvmpipe 26.0.8。
Xvfb 固定 CPU 8，基准固定 CPU 10，停止编译后逐轮运行，工作树干净。

| DPR | 准备 p95，三轮（ms） | Paint p95，三轮（ms） | 两图峰值（MiB） | 空闲图像字节 |
| --- | --- | --- | --- | --- |
| 1 | 5.398／5.373／5.405 | 0.149／0.148／0.147 | 7.03 | 0 |
| 1.5 | 4.356／4.401／4.353 | 0.774／0.779／0.775 | 15.82 | 0 |
| 2 | 8.337／8.619／8.463 | 1.124／1.244／1.126 | 28.13 | 0 |

九轮均满足准备 p95 ≤16.7 ms、Paint p95 ≤8 ms 的设计目标。
后续 `ad542b6` 仅补齐捕获降级后的焦点恢复；此处九轮数据仍明确对应 `3e3dc76`。
报告位于 `build/animated-stack-dpr{1,1.5,2}-round{1,2,3}.json`，包含每项样本数量、
p50／p95／max、真实 DPR、编译模式和源码提交。独立环境文件为
`build/animated-stack-environment.json`，SHA-256 为
`508a28ec2635ebc7431f1c22ccca44fb2e00f021cf61365a5cad5da21693c939`。
报告和环境文件是本机构建产物，不作为历史相对基线提交；本次使用官方 Qt kit，
环境摘要没有复用仓库中经过定时器补丁的参考 Qt kit 摘要。

重新采样前设置 `ZZ_BENCHMARK_COMMIT`、`ZZ_CMAKE_PRESET`、`ZZ_RUNNER_IMAGE_DIGEST`
和 `ZZ_GPU_IDENTITY` 为当前真实环境。执行示例：

```bash
cmake --build build/linux-gcc-benchmarks --target ZzAnimatedStackBenchmark --parallel 2
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 \
  QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough taskset -c 8 \
  xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' taskset -c 10 \
  build/linux-gcc-benchmarks/benchmarks/ZzAnimatedStackBenchmark \
  --report build/animated-stack-dpr1.5-round1.json
```

独立 CTest 入口为 `benchmark.animated-stack`，使用相同元数据环境；资源约束是失败条件，
耗时通过原始报告核对，不改变旧 `benchmark.animation` 或全局门限。

## 框架页面动画已接入

`ZzPureTools::ZzPageHost` 已完成框架接入，库默认 None，Example 显式 Fade。
源 WhileActive 页面在同步激活返回前销毁，动画只保留其快照；factory 失败、历史和
错误页保持原契约。`ZzWorkspaceShell::stackWidget()` 的公开类型与对象身份保留，
工作区不增加第二层动画。配置和本轮验收见[框架页面导航动画](PAGE_HOST_TRANSITION_ZH.md)。

两种容器共用私有播放器，但保留各自请求语义：本组件在动画中只保留最新待切页；
PageHost 每次导航同步提交路由及生命周期，新请求结束旧视效后重新捕获，不排队提交业务。
本组件原有通知顺序、源页删除中断和所有权行为不变，Qt 6.8／6.11 回归均通过。
上文 2026-11-06 的性能数据继续对应原提交，本轮框架采样另见新文档。

独立组件设计见[设计记录](../superpowers/specs/2026-11-06-animated-stacked-widget-design.md)
和[实施计划](../superpowers/plans/2026-11-06-animated-stacked-widget.md)；
框架接入见[框架设计](../superpowers/specs/2026-11-09-page-host-transition-design.md)
和[框架计划](../superpowers/plans/2026-11-09-page-host-transition.md)。
