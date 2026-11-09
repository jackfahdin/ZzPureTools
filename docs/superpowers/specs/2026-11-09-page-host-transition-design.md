# 页面动画的框架接入设计

**状态：** 已实施并完成本机验收（2026-11-09）；库默认关闭，示例开启。
**实现记录：** [使用与实际验收边界](../../development/PAGE_HOST_TRANSITION_ZH.md)。
**核对基点：** `a6e1c93`；Qt 6.8+、C++20。
**目标：** 在保留同步导航、页面生命周期及公开工作区接口的前提下，为 ZzPageHost 增加可选页面动画。

## 1. 设计时的代码事实

| 位置 | 当前行为 | 本次约束 |
| --- | --- | --- |
| `ZzWorkspaceShell::stackWidget()` | 返回 `QStackedWidget *`，调用者直接切页 | 保留返回类型、对象身份和直接切页行为 |
| `ZzPageHostPrivate::activate()` | 先创建目标，再离开旧页，再显示目标 | factory 失败不能提前销毁旧页或改变缓存 |
| `deactivateCurrentUnchecked()` | WhileActive、零容量 Recreatable 当场销毁 | 快照必须先于 `prepareForDestruction()`；不延长展示对象寿命 |
| `ZzPageInstancePrivate::prepareForDestruction()` | 禁用 View、取消任务、断展示对象连接、销毁 View／Presenter／ViewModel | 截图必须在禁用和取消之前完成 |
| `ZzNavigationControllerPrivate::navigate()` | 激活失败后主动调用 `showFrameworkError()`，维护错误页历史 | 不能把普通导航失败改为停留旧页 |
| `goBack()`／`goForward()` | 激活失败保留原页面与历史栈 | 失败时不播放错误页动画 |
| `ZzAnimatedStackedWidget` | 动画期间只保留最后待切页，逻辑提交可能延后 | 不把这个排队策略用于同步返回的导航 API |
| `ZzPureTools/CMakeLists.txt` | 私有链接 FluentUI，已使用私有跨模块桥 | 共享播放器编译进 FluentUI，PureTools 只调用内部导出，不重复编译实现 |

PageHost 的内部 QStackedWidget 没有公开 accessor，但生命周期测试通过 findChild 查找它。
这些测试需要改用 factory 保存的父对象和 View 验证所有权，不再断言私有 Qt 类型。

## 2. 方案比较与选择

| 方案 | 优点 | 代价／结论 |
| --- | --- | --- |
| 内部播放器＋PageHost 事务（采用） | 保留业务所有权与同步导航，共用捕获／绘制实现 | 需要先抽取播放器，再重构 PageHost 私有宿主 |
| 直接把 PageHost 堆栈换成 ZzAnimatedStackedWidget | 容器 API 简单 | 延后提交、源页删除中断与 WhileActive 立即销毁冲突；额外引入一套页面所有权 |
| 给工作区 QStackedWidget 包装或派生动画 | 接入入口表面上较少 | Qt 非虚 setCurrentWidget 无法完整拦截；跨层动画与公开类型兼容成本高 |

仅 PageHost 播放框架导航动画。中央工作区、标签分组和面板切换不增加动画层。
普通应用默认 None；Example 在窗口装配中显式设置 Fade。沿用主题 PageTransition 时长。

## 3. 公开接口

在 `ZzPageHost.h` 添加嵌套枚举和方法，公开头不包含 FluentUI Widgets 类型：

```cpp
enum class ZzTransitionMode { None, SlideHorizontal, SlideVertical, Fade };
enum class ZzTransitionDirection { Forward, Backward };
Q_ENUM(ZzTransitionMode)
Q_ENUM(ZzTransitionDirection)

ZzTransitionMode transitionMode() const noexcept;
ZzCore::ZzResult<void> setTransitionMode(ZzTransitionMode mode);
int transitionDuration() const noexcept;
ZzCore::ZzResult<void> setTransitionDuration(int milliseconds);
bool isTransitionRunning() const noexcept;
ZzCore::ZzResult<void> setRouteTransitionEnabled(const ZzRouteId &route, bool enabled);
bool isRouteTransitionEnabled(const ZzRouteId &route) const;

// 保留原单参数函数符号；单参数委托到 Forward。
ZzCore::ZzResult<void> activate(
    const ZzPageRegistration &registration, ZzTransitionDirection direction);
```

- 默认 mode None、duration -1；时长接受 [-1,10000]，范围外返回 InvalidArgument，不悄悄裁剪。
- 所有修改在所属 GUI 线程执行；跨线程返回 InvalidState。查询跨线程返回安全默认值。
- 禁用路由存入宿主级 QSet；可在首次创建前配置，重建后仍生效；重新启用移除记录。
  无效 route 返回 InvalidArgument，查询无效 route 返回 false。该策略集合不属于页面 LRU。
- 模式、时长、涉及路由禁用在空闲事务期间改变时，结束当前动画、显示已提交页；
  不相关路由开关不影响当前段。同步激活事务内修改配置返回 InvalidState。
- 普通 navigate／goForward 使用 Forward，goBack 使用 Backward；直接 activate 默认为 Forward。
  横向方向随 RTL 镜像，纵向不随 RTL 镜像，Fade 忽略方向；不按 route 字符串排序。
- 本阶段不新增 PageHost 动效信号；`currentRouteChanged` 仍是逻辑导航通知，
  `isTransitionRunning()` 仅表示视效状态。测试使用内部动画控制时间。
- 保留 `ZzPageRegistration` 布局和 factory 签名，保留 `ZzPageInstance` 销毁契约。

## 4. 内部组件与模块边界

### 4.1 快照播放器

新增 `ZzFluentUI/widgets/src/private/ZzPageTransitionPrivate.{h,cpp}`，
采用 Pimpl、`ZZ_FLUENT_UI_EXPORT` 内部导出，头不安装。
参考现有 `ZzSplitWorkspaceTransactionPrivate` 的跨模块方式。

播放器只负责捕获资格、DPR／预算、图像存储、覆盖层、单动画及结束清理，
不注册页面、不 reparent 页面、不保存路由、不调用 factory、不负责 LRU。
外层负责真实页面显隐和焦点；播放器结束回调释放两图后才通知外层恢复。
捕获和播放通过代次 token 分离，取消会令 token 失效。

内部操作：`beginCapture(settings)` 清理旧帧并创建 token，
`captureSource(token, view)`／`captureTarget(token, view)` 各最多一次，
`play(token)` 安装两图开始，`cancel()` 停止并清空，`running()`／`retainedBytes()` 只读。
settings 包含内容矩形、DPR、背景色、模式、方向和已调整的时长。
所有可回调 QWidget 操作后检查 owner／view QPointer 与 token；失败释放本轮两图。
清理结束回调和 animation finished 回调不许在宿主销毁后访问宿主。

`ZzAnimatedStackedWidgetPrivate` 改用该播放器，继续拥有原有页注册、最新请求队列、
源／目标删除中断规则、信号配对和焦点策略。公共 API 与既有 80／81 行行为不变。
PureTools 的适配器把其枚举映射成内部播放器模式，不把私有类型暴露给 SDK 消费者。

### 4.2 PageHost 适配器与真实宿主

新增 `ZzPageHostTransitionPrivate.{h,cpp}`，负责主题取值、路由开关、
环境变化、源／目标焦点记忆、播放器回调和宿主显隐，生命周期属于 PageHost 私有状态。
适配器采用内部导出与 Pimpl，头不安装；PageHost 只增加该私有类型和
ZzNavigationControllerPrivate 的 friend 声明，供事务预检及测试桥访问，不新增公开私有状态 getter。
普通 QWidget 内容宿主作为 PageHost 的稳定子控件，由内容矩形同步几何；
factory 仍接收该宿主作为唯一 pageParent。
框架错误页也是其子对象。聚合存活注册页与错误页的 sizeHint／minimumSizeHint，
转发 LayoutRequest，维持原堆栈的最大建议尺寸策略；不按当前页改变外层窗口尺寸。
PageHost 增加 sizeHint／minimumSizeHint 和受保护 event 重载，具体逻辑委托到私有状态。

不用 QStackedLayout 切页：上一阶段已证实其 Hide 回调后使用旧局部目标，对同步删除不安全。
逐页显隐与几何操作复制 QPointer 列表，操作后检查宿主存活和当前身份。
不把 PageInstance 拥有的 View 再交给独立动画容器管理。

## 5. 激活事务与失败语义

成功路径严格顺序：

1. 校验线程、参数与事务占用；同路由已活动则成功返回，不重播、不取消原段。
2. 创建或复用目标。新实例先局部持有，不加入活动页表或修改 LRU；factory 失败清理
   本次新增子对象，原页、历史、缓存和已有动画不受框架主动修改。
3. 目标合法后结束已有视效、恢复其已提交真实页；没有旧图像存储后才开始下一次捕获。
4. 若两端适合动画，记住旧焦点，在旧页禁用／取消任务之前捕获源；
   给隐藏目标设置最终内容几何并激活其布局，捕获目标。首屏、错误页、禁用或超预算不截图。
5. 每次捕获及布局回调后核对存活和事务代次；环境变化取消捕获，业务激活可继续直接提交。
   回调主动销毁目标则清理新实例、返回 InvalidState，未被用户销毁的原页保持活动。
6. 在不跨越回调保留 map iterator 的前提下，把目标登记、移出 LRU；离开旧页并按原策略
   立即取消任务和销毁 WhileActive／零容量 Recreatable。普通 Recreatable 进入原 LRU。
7. 提交 activeRoute、错误页标志和真实目标，恢复目标尺寸／可见性。
   如目标在显隐／取消任务回调中被销毁，释放快照并清理失效页项，返回 InvalidState；
   不承诺复活已经按生命周期销毁的旧页，宿主保持无悬空引用的空闲状态。
8. 若捕获 token 仍有效，隐藏真实宿主、播放快照；否则显示目标并恢复仍归本区域的焦点。
   activate 返回前路由及生命周期已生效。控制器随后按既有规则提交历史、发导航信号。

外部回调主动破坏已有页面不属于 factory 失败回滚保证；必须记录这一边界并测试清理。
直接 delete 正在执行 paintEvent 的页面属于 Qt 渲染生命周期禁用行为，测试用销毁其他页、
删除宿主的非 paint 回调及 deleteLater 覆盖可支持路径，不宣称可修复 Qt 自身的自删绘制。

| 调用与结果 | 页面与动画 | 历史 |
| --- | --- | --- |
| PageHost::activate factory 失败 | 保留旧页与已有段；不生成新段 | PageHost 不管理历史 |
| navigate 已注册页 factory 失败 | 沿用原逻辑显示框架错误页，直接切换并清空快照 | 沿用原错误页历史逻辑 |
| navigate 无效／未注册 route | 校验失败，保留现态 | 不变 |
| goBack／goForward factory 失败 | 保留旧页与已有段 | 两个历史栈不变 |
| showFrameworkError／deactivateCurrent | 直接清理动画，遵守原销毁和错误页显示逻辑 | 由调用方维护 |

## 6. 连续导航、重入与关闭

连续 A→B→C→D 每次函数成功返回都同步提交其路由与历史；新成功请求终止旧段，
以当前逻辑页为下一段源。允许快速点击出现视效收敛跳变，不捕获上一段中间帧，
不另外保存图像、不排队 factory、不把成功返回解释成“仅入队”。
动画播放本身不占用激活事务，后续正常事件可以导航。

同步 factory／render／显隐／取消任务／导航通知回调期间的重入必须拒绝：

- PageHost 的返回值修改操作返回 InvalidState；`deactivateCurrent() noexcept` 重入直接无操作。
- NavigationController 对 navigate／goBack／goForward 及历史容量修改设置事务守卫，
  守卫覆盖 history 更新和通知。调用方需要再次导航可投递到下一次事件循环。
- Controller 从 PageHost 私有桥预检事务占用，忙状态不进入普通 factory 失败分支，
  不显示错误页、不污染历史、不发送误导性 navigationFailed。
- QPointer 守卫跨越 factory、取消任务和 signal；宿主／控制器被同步销毁时不再访问私有状态。
  不能用持有裸 this 的 scope guard 在对象销毁后回写事务标志。
- 页面移走／销毁清理注册与 LRU、终止涉及目标的段；业务策略主动销毁旧源页时，
  仅图像继续播放，不让旧 View 的 destroyed 取消这段已脱离 View 的画面。
- 宿主 hide、失活、resize、DPR／主题／语言／布局方向变化直接恢复最新已提交页；
  关闭窗口先断播放器回调并停止，再执行 PageInstance 清理。

## 7. 输入、嵌套动画与资源

继承独立组件的输入隔离、减少动态效果、预算和焦点外移保留规则。
None 路径不产生额外临时焦点。只恢复目标仍可见、可用、在正确宿主中的记忆焦点。
应用全局快捷键与外部导航保持可用；截图不形成第二套可访问内容树。

框架只在 PageHost 播放；工作区直接切换不动画。页面内部嵌套 ZzAnimatedStackedWidget
如正在播放，宿主本段降级到直接切换，避免捕获运动中的子快照；其同步 Hide 会使子动画收敛。
静止的嵌套容器不妨碍宿主动画。宿主播放时真实页隐藏，子容器的导航走其原隐藏降级路径。
不同窗口各有独立预算，不承诺应用全局只有一个动画。

每个播放器最多一个 QVariantAnimation、两图合计 ≤64 MiB；结束／中断零图像。
新段在旧图完全释放后准备，不延长 View／Presenter／ViewModel 生命周期。
性能测试同时报告完整 activate 和纯视效准备耗时；factory 耗时不可隐去，也不把任意业务
factory 纳入 16.7 ms 的保证。固定表单场景以准备 p95 ≤16.7 ms、Paint p95 ≤8 ms 为目标。
基准通过内部导出适配器的 observeCapture 桥挂载瞬时观测回调；默认无回调、无产品计时器。
准备观测从 beginCapture 后的尺寸检查／分配之前，到 captureTarget 完成或取消为止，
回调必须配对，正常宿主路径中包括两次分配、布局和 render，不包括 factory。

## 8. 验收和交付

- 默认 None 与显式 None 均保持生命周期、历史和最终外观。
- 三种生命周期与 LRU 容量 0／1／3、factory 返回失败／抛异常／无效实例都覆盖。
- 源 WhileActive 对象在 activate 返回前已销毁，动画仍显示其有效快照；
  验证 View→Presenter→ViewModel 顺序及 cancellation 恰好一次。
- 连续导航和回退前进逐次核对 controller／host route、历史及动画目标，没有排队业务提交。
- 显隐／render／销毁／信号重入、有父窗口关闭、多窗口／分屏迁移、焦点和无障碍通过。
- 两版 Qt、四档 DPR、X11、ASan／UBSan；共享／静态安装、公开头及既有架构门禁通过。
- Example 显式 Fade，等待动画确定结束再截图；不增加固定 sleep，不改遮罩／门限／参考 PNG。
- 配置 CI 八项契约继续全跑；Windows／macOS 无环境时仅记录静态验证，原生验收另列。

保持本地 master 线性提交、不建分支或 worktree、不推送、不访问 docs/research、不改第三方。
全部 7 项任务已实施，完成状态见[实施计划](../plans/2026-11-09-page-host-transition.md)。

### 实施补充

播放器使用 shared_ptr Pimpl 保护外层在回调中被删除时的短期内部状态，仍不拥有页面；
新增 present 阶段先安装首帧再开始计时，保留独立容器的原信号顺序。内部类型使用
ZzMode／ZzSettings 及 Mode／Settings 别名，满足既有架构命名检查。

Qt 的 QWidget destroyed 通知早于 QPointer 清空；宿主在 ChildAdded 阶段预先观察，
保护销毁期间的重入并解除对正在析构 View 的重复删除责任。回调中直接删除宿主时，
将该 View 从父树摘出，剩余失效记录异步清理。正常页面取消与销毁顺序保持原契约。
宿主保留 Qt::NoFocus 默认策略，逐页隐藏后再隐藏内容父对象，避免回调删除兄弟时
Qt 显隐遍历继续使用失效指针。以上行为均有回归及 sanitizer 证据。

Example 中英文全路由在真实渐变下约需 11～12 秒，集成总 timeout 改为 30 秒；
已有截图流程继续使用其原 reducedMotion 设置，未调整截图基线或像素门限。
最终性能报告来自 `f23ff8d` 的同一干净提交，18 轮及两个 CTest 基准入口通过。
