# ZzPureToolsExample 端到端集成实施计划

> 文档状态：已确认，实施中
>
> 确认日期：2026-08-06
>
> 目标仓库：`/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`
>
> 旧版参考：仓库父目录中的 `ZzPureToolsExample`

> **2026-08-07 更新：** 项目所有者 Jackfahdin 已确认 `ZzAwesome.ttf` 与旧版
> 11 个 SVG 的完整使用和分发授权。本文关于“不迁移字体图标和无授权素材”的
> 原始假设对这些固定资源不再适用，当前结论见
> `docs/development/ICON_ASSETS_ZH.md`。

## 1. 目标与边界

新增正式的 `ZzPureToolsExample`，以旧版示例的可见功能、页面结构和操作路径为参考，完成 ZzCore、ZzWindowKit、ZzFluentUI、ZzPureTools 与 ZzLog 的端到端装配。

本示例同时承担以下责任：

- 展示无边框窗口、Fluent 主题、应用导航、页面生命周期和多窗口隔离。
- 证明 View、ViewModel、Presenter 与应用服务之间没有反向依赖。
- 提供 Linux 物理桌面最终效果验收入口。
- 为 Windows MSVC/MinGW 和 macOS 提供可编译、可静态审计的同一源码入口。

现有 `ZzWindowKitDemo`、`ZzFluentFoundationDemo`、`ZzFluentControlsGallery` 和 `ZzPureToolsDemo` 保留，继续承担单组件 smoke 和故障隔离。新示例不替代这些目标，也不默认安装到 SDK 发布包。

旧版 API、全局 EventBus、全局撤销栈、服务定位器、字体图标枚举、Windows DXGI 私有实现和无授权素材不迁移。

## 2. 框架增量契约

### 2.1 窗口装配回调

新增公开类型：

```cpp
namespace ZzPureTools {

using ZzWindowSetupCallback = std::function<ZzCore::ZzResult<void>(
    ZzApplicationWindow &window)>;

} // namespace ZzPureTools
```

`ZzApplicationBuilder::setWindowSetupCallback()` 只允许成功设置一次。空回调、重复设置、Builder 冻结或移动后调用必须返回明确错误。

回调执行顺序固定为：

1. 创建 `ZzApplicationWindow` 和基础 QWidget 子树。
2. 创建窗口独占导航模型、页面宿主和导航控制器。
3. 调用窗口装配回调。
4. 附加 ZzWindowKit 并使用最终标题栏交互控件配置 chrome。
5. 执行首次导航。
6. 把完整窗口提交给 `ZzPureApplication`，随后才允许显示。

回调由应用保存，对首窗和后续 `createWindow()` 创建的全部窗口生效。回调失败时不得显示或收养半初始化窗口；已经创建的 Qt 子对象按窗口所有权树清理。

### 2.2 窗口壳层

`ZzApplicationWindow` 增加以下 GUI 线程观察接口：

- `ZzFluentTitleBar *titleBar() const noexcept`
- `ZzNavigationPane *navigationPane() const noexcept`

标题栏使用 `QMainWindow::setMenuWidget()` 放在标准工具栏、Dock、中央区和状态栏之上。应用装配回调使用继承的 `addToolBar()`、`addDockWidget()` 和 `setStatusBar()`，不访问 private layout。

### 2.3 双向历史

`ZzNavigationController` 增加：

- `goForward()`
- `canGoForward()`
- `historyStateChanged(bool canGoBack, bool canGoForward)`

直接成功导航时，旧当前路由进入 back 栈并清空 forward 栈；成功后退时当前路由进入 forward 栈；成功前进时当前路由进入 back 栈。前进或后退期间页面创建失败时，两个栈和当前路由均不得变化；直接导航失败继续显示既有框架错误页。历史容量同时约束两个方向，每个方向都裁剪最旧项，零值禁用历史。

## 3. 应用结构

### 3.1 Composition root

`main.cpp` 只负责：

1. 准备 ZzWindowKit。
2. 创建 `ZzPureApplication`。
3. 创建共享的 `ZzExampleApplicationContext`。
4. 注册 `ZzExampleApplicationModule`、页面 factory、导航节点和窗口装配回调。
5. 构建并运行应用。

`ZzExampleApplicationContext` 由 `std::shared_ptr` 共享，包含设置存储、任务执行器、活动模型和只读应用能力，不包含 QWidget。页面 factory 可以捕获它完成构造注入。

`ZzExampleApplicationModule` 管理 ZzLog、`ZzQtLogBridge`、任务停止和最终设置同步。模块停止协议保持幂等，ZzLog 初始化失败必须通过 `ZzResult` 阻止应用构建。

### 3.2 展示边界

- View 只创建控件、设置可访问文本、显示 ViewModel 状态并发出用户意图。
- ViewModel 只保存展示值或实现 `QAbstractItemModel`，不访问设置、日志、文件或窗口。
- Presenter 连接 View 意图与注入的主题、导航、任务、设置和活动接口。
- UI 源码禁止 include ZzLog、ZzQtSettingsStore 或平台 API。
- 复杂 View、Window Shell 与 Presenter 使用四文件 Pimpl；简单值模型和无状态 factory 使用两文件结构。

### 3.3 窗口壳层

每个窗口由独立 `ZzExampleWindowShell` 装配：

- 顶部命令栏：返回、前进、页面搜索、主题切换、新建窗口和窗口菜单。
- 右侧 Dock：活动日志与更新内容。
- 状态栏：当前路由、任务状态和平台名称。
- 关闭守卫：取消、最小化和确认关闭。

多窗口共享主题、设置和活动数据；导航控制器、历史、页面实例、Dock 状态、关闭守卫和 ZzWindowAgent 必须逐窗口隔离。

活动 Dock 不观察 spdlog sink。Presenter 将同一条结构化活动同时追加到有界活动模型并写入 ZzLog，避免给日志热路径增加 GUI 回调。

## 4. 页面与路由

| 路由 | 展示位置 | 生命周期 | 旧版对应 | 新版内容 |
|---|---|---|---|---|
| `home` | Workspace | Persistent | `T_Home` | 主视觉、快捷卡片、最近活动 |
| `controls` | Controls | Recreatable | `T_BaseComponents` | 按钮、输入、选择、开关、进度、日期和滚轮 |
| `cards` | Controls | Recreatable | `T_Card` | ActionCard、ImageCard、Carousel、LCD |
| `list-view` | Data views | Recreatable | `T_ListView` | 列表 delegate、筛选和选择 |
| `table-view` | Data views | Recreatable | `T_TableView` | 排序、列调整和模型更新 |
| `tree-view` | Data views | Recreatable | `T_TreeView` | 层级模型、展开和选择 |
| `navigation` | Interaction | Recreatable | `T_Navigation` | Breadcrumb、Tab、标签转移和历史 |
| `feedback` | Interaction | Recreatable | `T_Popup` | Menu、Dialog、MessageBar、SuggestBox |
| `icons` | Resources | Recreatable | `T_Icon` | 本地示例图标和 Qt 标准图标搜索 |
| `platform` | System | Persistent | `T_ZzScreen` | 屏幕、DPI、窗口状态和 WindowKit 能力 |
| `settings` | Footer | Persistent | `T_Setting` | 主题、日志等级、Dock 与窗口设置 |
| `about` | Footer | Persistent | `T_About` | 版本、编译器、许可证和第三方信息 |

导航继续使用平面值模型。旧版仅用于压力演示的多层 `TEST_EXPAND_NODE` 不迁移；分区使用 section metadata，设置和关于使用 footer placement。

## 5. 视觉、资源与本地化

- 高还原指页面结构、导航层级、控件组合、Dock、状态栏和主要操作路径，不复制旧版实现。
- 旧版动漫图片、来源不明的控制截图和两个约 45 MiB GIF 禁止进入新仓库。
- 首页和卡片使用原创轻量位图，总资源预算不超过 8 MiB，不在运行时下载。
- 示例 SVG 图标只属于示例资源，不形成新的生产图标枚举或字体依赖。
- 每个非代码资源记录来源、作者、许可证、用途和 SHA-256。
- 中文为源码语言；英文 translator 仅在系统 locale 为英文时由 Builder 加载。
- Light、Dark、HighContrast、LTR/RTL 和 100% 至 200% DPR 必须保持可操作且无文字重叠。

## 6. 验收与提交

每个逻辑批次通过对应测试后立即提交，提交标题为中文简述，正文为中文修改细节。禁止把多个未验证批次堆积到单个提交。

自动验证必须覆盖：

- 窗口装配回调顺序、失败回滚和后续窗口复用。
- back/forward、搜索导航、失败不修改历史和窗口隔离。
- 页面生命周期、设置持久化、主题切换、任务取消和模块关闭。
- 全部路由可达，工具栏、Dock、状态栏和关闭守卫可操作。
- Linux shared/static、GCC/Clang、ASan、clang-tidy 和完整 CTest。
- Windows MSVC/MinGW 与 macOS 的 CMake 和源码静态审计。
- 四档 DPR 与 Light、Dark、HighContrast 的确定性截图。
- 启动、空闲、页面切换、主题切换和大模型滚动继续满足现有性能门禁。

Linux 物理桌面最终验收以 `ZzPureToolsExample` 为主入口，检查窗口拖动、系统按钮、主题、搜索、双向导航、Dock、多窗口、DPI、平台能力与关闭流程。

## 7. 2026-08-06 实施记录

### 7.1 已完成代码范围

- `ZzPureToolsExample` 已作为独立 CMake 目标接入，十二条正式路由均由页面
  factory 构造，Persistent 与 Recreatable 生命周期按第 4 节执行。
- Composition root、共享 `ZzExampleApplicationContext`、应用模块、窗口壳层、
  View/ViewModel/Presenter 边界和英文翻译已经落地；英文资源共 241 条 finished
  词条，运行时只在英文系统 locale 装载。
- 每窗口独占导航控制器、WindowAgent、Shell、Dock 和关闭守卫；活动模型、主题、
  设置及任务服务按应用上下文共享。
- 自动 smoke 已覆盖十二路由、英文装载、双窗口隔离，以及关闭取消、转换为最小化、
  确认关闭三条真实模态路径。活动模型另有容量、角色、信号和幂等清理单元测试。
- 十二路由 smoke 不只直调导航控制器：它还通过首页快捷卡片、搜索框
  `returnPressed`、工具栏返回/前进 action 和主题 action 完成真实 UI 串联，并校验
  当前路由、历史 action 状态、搜索清空和主题状态确实同步。
- 多窗口 smoke 通过工具栏“新建窗口” action 创建第二个窗口，装配回调同步捕获
  新实例后再检查 WindowAgent、导航控制器、Shell、Dock 和关闭状态隔离；测试不再
  绕过用户命令直接调用 `ZzPureApplication::createWindow()`。

### 7.2 视觉基线契约

综合示例新增 `screenshot` smoke 场景，CTest 以独立进程固定以下条件：

- Qt offscreen、LTR、C locale、DejaVu Sans 10pt、1280x800 逻辑窗口。
- Light、Dark、HighContrast 三种显式主题，主题切换时禁用动画。
- `QT_SCALE_FACTOR` 分别为 1.0、1.25、1.5、2.0，并验证主屏实际 DPR。
- 每档生成三张 PNG，共十二张，保存于
  `examples/ZzPureToolsExample/tests/baselines/linux/dpr-*`。

比较器对可见 QLabel、按钮、编辑器、组合框、item view、tab、进度条、group box
和文本编辑器建立文字像素遮罩。遮罩必须非空且不得覆盖一半以上画布；尺寸、非空
画面、主题颜色、布局、边框和图标仍参加逐通道比较。Qt 6.11 参考环境允许最多
0.5% 非文字像素差异，其他 Qt minor 只允许 2% 兼容差异。失败证据写入现有
`reports/fluent-screenshots/example-puretools` 工件树。

更新参考图必须显式设置 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1`，并且只能在已审 Linux
参考发布机、Qt 6.11、上述字体和四档 DPR 环境执行。普通构建不得修改基线。

### 7.3 本批验证证据

- Linux GCC shared 与 static 严格警告构建及完整 CTest 均为 `115/115` 通过，
  包括安装消费、包重定位、公开头、架构边界、二进制依赖、组件截图与十五项
  示例测试。
- Linux Clang shared 构建通过；ASan+UBSan 完整 CTest `115/115` 通过，不只执行
  定向截图场景。
- shared 与 static 两份编译数据库的完整 `ZzClangTidy` 均分析 182 个项目源文件
  并通过。
- 四档综合截图关闭更新模式后连续运行两轮均通过；人工检查 100% 三主题和
  200% Light，未发现空白、裁切、重叠或主题串色。
- 视觉检查发现并修复标题栏子按钮调色板传播滞后导致的深色图标低对比问题，
  回归测试保证系统图标从标题栏当前调色板取色。
- 性能实现提交 `e749273d630936ae7f249947ae827f8ca9312d9a` 已在干净 HEAD、
  固定 Xvfb 1920x1080x24、Xvfb CPU 8 与 benchmark CPU 10 的参考条件下完成
  37/37 测试；同一次运行生成 12 份报告并通过 15 项绝对门限。
- 综合示例启动 `external-total` P95 为 77.006070 ms，页面切换 P95 为
  11.293115 ms，主题切换 P95 为 9.383993 ms，十万行滚动 P95 为
  0.458241 ms；30 秒空闲 CPU 为 0%，RSS 增长 0.193599%。五份报告已经与
  七份组件报告一同逐字固化到 `docs/performance/reference/linux/`，并接入
  `run-linux-gates.sh` 的同环境 10% 相对回归比较。

### 7.4 本地临时素材预览

- CMake 新增高级缓存变量 `ZZ_EXAMPLE_LOCAL_ASSET_DIR`。变量为空时继续使用
  确定性 palette 预览，确保仓库、CI 和发布构建不依赖本机文件。
- 指定目录时，`home.png`、`card-performance.png`、`card-windowing.png` 和
  `card-data.png` 以固定资源别名编入综合示例；缺少任意文件会在配置阶段失败，
  图片解码失败则在运行时回退到 palette 预览。
- 本次仅把旧版图片复制到被 Git 忽略的 `build/local-assets/ZzPureToolsExample/`
  用于本机效果验证，图片及旧版绝对路径均不进入提交，也不构成正式资源的来源、
  作者、许可证或 SHA-256 记录。
- 无素材与本地素材两种全新配置均完成 GCC 15 严格警告构建。无素材配置的真实
  交互、多窗口和四档截图共 7 项通过；本地素材配置的真实交互、英文和多窗口共
  3 项通过。另在被忽略目录生成并人工检查 100% DPR 的 Light、Dark、
  HighContrast 截图，未发现图片解码失败、比例失真、文字重叠或布局越界。

### 7.5 尚未完成且不得误报的范围

- 首页与卡片页的正式原创轻量位图及逐文件资源 provenance 尚未加入。当前复用的
  旧版图片仅用于不入库的阶段性验证；用户替换正式文件后，仍须记录来源、作者、
  许可证和 SHA-256，并重新生成及审核仓库截图基线。
- Linux X11 KDE、X11 GNOME、Wayland KDE、Wayland GNOME 和强制 Qt fallback
  五种物理桌面会话仍需按 `MANUAL_LINUX_CHECKLIST_ZH.md` 人工签署；offscreen
  截图不能代替窗口拖动、resize、系统菜单、多显示器和辅助技术验收。
- 2026-08-06 曾从当前自动化终端尝试启动 Wayland KDE 采证，但该 compositor
  socket 没有发布 `wl_output`，应用只能获得 Qt placeholder screen，因此本次运行
  已终止且不计入真机结果。采证脚本现会在创建证据前拒绝同类无输出会话。
- Windows MSVC、Windows Qt MinGW 与 macOS 的原生构建和真机交互结果仍待外部
  环境验证；本机只能保留跨平台源码与 CMake 静态约束。
