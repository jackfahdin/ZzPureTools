# ZzWindowKit 第 4 批软件材质背景详细实施计划

**目标：** 在不扩大 `ZzWindowKit` 公共 API、不过度依赖 QWindowKit 上游实现的前提下，为 `ZzWindowBackdrop::Automatic` 增加可审计的跨平台软件材质 fallback，并保持 Windows 原生 Mica/Acrylic、macOS 原生 Blur 和 Linux/Wayland 的能力边界清晰。

**前置基线：** 本计划基于提交 `d405cf6` 后的源码、Qt 6.11.1 公共 API、当前 `ZzQWindowKitBackend`、`ZzWindowAgentPrivate` 和既有 QWindowKit 适配计划。生产实现只使用 Qt 6.8+ Core/Gui/Widgets 公共 API 和 C++20；Linux 是实际构建与测试平台，Windows MSVC、Windows MinGW、macOS arm64/x86_64 只做静态检查，未执行不得写成通过。

## 1. 审计结论与不可违反的边界

### 1.1 当前事实

- `ZzWindowAgent` 的公开背景入口已经存在，QWindowKit 类型只允许出现在 `ZzWindowKit/src/private`，不新增公共上游依赖。
- `ZzQWindowKitBackend` 当前在 Windows 使用 QWindowKit 属性切换原生 `mica`、`mica-alt`、`acrylic-material` 和 `dwm-blur`；macOS 使用 QWindowKit `blur-effect`；Linux 对除 `None` 外的请求返回 `Unsupported`。
- `ZzWindowAgentPrivate` 只保存宿主 `QPointer<QWidget>` 和后端，不拥有宿主窗口。软件层必须遵循同样的非拥有宿主约束，并在宿主销毁时先解除事件连接再销毁自身。
- Qt 公共 API 没有跨平台“读取窗口背后的桌面纹理”或“把桌面材质应用到 QWidget”的接口。Wayland 也不允许依赖 X11 根窗口截图语义；`QScreen::grabWindow(0, ...)` 不能作为生产路径。

### 1.2 本批定论

1. **不实现真正的桌面采样模糊。** 软件 fallback 只绘制宿主窗口内的缓存材质层：以宿主 `QPalette::Window`/`QPalette::Base` 为基色，叠加低频确定性纹理和透明度，内容子控件仍在材质层之上绘制。文档称为“软件材质背景”，不宣称等价于系统 Mica/Acrylic。
2. **不使用桌面截图、平台 native handle、Qt Private、QGraphicsEffect 或每帧 blur。** 这些路径要么破坏 Wayland/跨平台，要么会在窗口移动/缩放时产生不可控开销。
3. **只改变 `Automatic` 的 fallback。** Windows/macOS 的原生能力优先级保持不变；Linux `Automatic` 使用软件材质，显式 `Blur/Acrylic/Mica/MicaAlt` 仍返回 `Unsupported`。原生后端已报告能力但运行时属性失败时，`Automatic` 才尝试软件材质；显式请求不静默降级。
4. **软件能力不加入 `ZzWindowCapability`。** 该枚举描述原生窗口后端能力，软件材质由 `Automatic` 的返回状态体现，避免调用方把软件层误认为可用原生系统效果。
5. **软件层是宿主的私有子控件。** 它透明接收鼠标、始终位于宿主子控件底部，不改变 QWindowKit 标题栏命中测试、系统按钮、焦点链或业务页面所有权。
6. **禁用策略只通过 `None` 明确表达。** 本批不猜测操作系统“高对比”状态，不读取平台私有设置；调用方在高对比、性能受限或无障碍模式下请求 `None`。软件层本身使用不透明 palette 基色，保证误启用时仍可读。

## 2. 软件材质层设计

### 2.1 文件与所有权

新增私有文件：

- `ZzWindowKit/src/private/ZzSoftwareBackdrop.h`
- `ZzWindowKit/src/private/ZzSoftwareBackdrop.cpp`
- `ZzWindowKit/src/private/ZzSoftwareBackdropPrivate.h`
- `ZzWindowKit/src/private/ZzSoftwareBackdropPrivate.cpp`
- `ZzWindowKit/tests/ZzSoftwareBackdropTest.cpp`

`ZzSoftwareBackdrop` 是 `ZzWindowKit` 内部 QObject，不安装、不出现在公共头、不暴露给示例。它只持有 `QPointer<QWidget>` 宿主和一个固定的材质 layer；layer 的具体 QWidget 子类放在 private 实现中。所有公开 QWindowKit 适配仍由 `ZzQWindowKitBackend` 负责。

建议私有接口：

```cpp
class ZzSoftwareBackdrop final : public QObject
{
    Q_OBJECT
public:
    explicit ZzSoftwareBackdrop(QObject *parent = nullptr);
    ~ZzSoftwareBackdrop() override;

    [[nodiscard]] bool attach(QWidget *host);
    void detach();
    [[nodiscard]] bool setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept;
    [[nodiscard]] std::size_t rebuildCount() const noexcept;
};
```

实际签名以现有私有命名和 warning 规则为准；`rebuildCount()` 仅供测试访问，不进入安装头。`attach()` 必须拒绝空指针、非顶层窗口、跨线程窗口和重复绑定。

### 2.2 layer 绘制契约

- layer 固定创建一次，`WA_TransparentForMouseEvents`、`WA_NoSystemBackground` 和 `autoFillBackground=false`；不安装全局 event filter。
- layer geometry 始终等于宿主 `rect()`，并在 `Resize`、`ContentsRectChange`、`ScreenChangeInternal`、`DevicePixelRatioChange` 和 `PaletteChange` 时失效缓存。
- layer 使用宿主当前 `QPalette::Window`，没有有效 Window 色时回退到 `QPalette::Base`；不写入硬编码主题色，不读取 FluentUI 业务 token，避免 WindowKit 反向依赖 FluentUI。
- 纹理是固定尺寸的低频 alpha 图案，构造/缓存失效时一次生成；`paintEvent` 只把已缓存 `QImage/QPixmap` 绘制到目标 rect，不创建 QObject、QTimer、动画或临时大图。
- 高 DPI 下缓存 key 必须包含逻辑尺寸、DPR 和 palette 基色；缓存使用 `devicePixelRatioF()`，不把物理像素尺寸泄露到公共 API。
- layer 置于宿主子控件底部，不能调用 `raise()` 覆盖页面、标题栏或按钮。宿主已有子控件新增后通过 `ChildAdded` 或一次性 `lower()` 保持顺序，禁止为每个子控件安装独立监听器。
- 软件层只提供终态材质，无入场/移动动画；窗口拖动和缩放期间只在尺寸或 palette 变化时重建。

### 2.3 缓存失效与生命周期

1. `attach()` 创建 layer、连接宿主销毁信号、初始化缓存并返回成功。
2. `setEnabled(false)` 隐藏 layer、清理缓存但不销毁 layer；再次启用只重建一次。
3. 宿主销毁时 `QPointer` 先清空，解除所有连接，layer 由宿主 QObject 父子关系销毁；manager 析构不得访问已失效宿主。
4. 重复设置相同启用状态必须幂等，不触发重建。
5. 重建次数是可测试的诊断计数，不作为发布 API 或业务逻辑依据。

## 3. 与 QWindowKit 后端集成

### 3.1 `ZzQWindowKitBackend` 调整

- 新增 `std::unique_ptr<ZzSoftwareBackdrop> softwareBackdrop_`，只在 `Automatic` 需要 fallback 或 Linux `Automatic` 路径创建；普通显式原生材质不创建软件层。
- `attach()` 完成 QWindowKit setup 后，把宿主交给软件层但保持 disabled；失败时不影响原生后端 attach 结果，只有实际启用软件层失败才返回 `Backend`。
- `setBackdrop(None)` 先关闭软件层，再按当前平台关闭原生材质；结果为 `Applied`。
- Linux `setBackdrop(Automatic)` 启用软件层并返回 `Applied`；Linux 显式非 `None`/非 `Automatic` 返回 `Unsupported`，不得调用未知 QWindowKit 属性。
- Windows/macOS `Automatic` 先按现有版本/能力选择原生路径；原生路径成功则关闭软件层并保持原返回状态。原生路径因运行时属性失败时，尝试软件层并返回 `Applied`；软件层失败才返回原生 `Backend` 错误，避免吞掉真正的后端故障。
- Windows/macOS 显式 `Mica/Acrylic/Blur` 失败时不软件降级，保持现有错误语义；显式请求的可预测性优先于“看起来能用”。
- `capabilities()` 继续只返回原生能力位；软件材质的成功只能从 `setBackdrop()` 返回值判断。
- QWindowKit 原生材质的互斥关闭顺序保持不变，软件 layer 的开关必须发生在原生属性提交成功之后或明确回退之前，避免短暂双重材质。

### 3.2 不改动的公共契约

- 不新增 `ZzWindowBackdrop` 枚举值，不修改 `ZzWindowApplyState` 和 `ZzWindowCapability` 的含义。
- 不把 `ZzSoftwareBackdrop` 或其配置暴露给 `ZzWindowAgent` 公共头。
- 不把 QWindowKit 上游头、平台宏或 native key 类型带入安装目标和公共 CMake Config。

## 4. 测试计划

### 4.1 `ZzSoftwareBackdropTest`

覆盖以下真断言：

- 空指针、子窗口、跨线程和重复 attach 被拒绝。
- attach 后 layer 只有一个，`WA_TransparentForMouseEvents` 为真，layer 不改变宿主已有子控件顺序和鼠标命中。
- 启用/禁用幂等；启用后重建次数为 1，重复 repaint 不增加；resize、palette、DPR 变化各只触发一次重建。
- layer 绘制非空、使用宿主 palette 基色，且不包含固定主题色或透明全屏黑块。
- 宿主销毁后 manager 可安全析构，不能访问悬空指针。
- 1000 次 enable/disable、resize 和 update 后 QObject 子对象数量、layer 地址和缓存上限不增长。

### 4.2 `ZzWindowAgentTest` 扩展

- fake backend 继续验证 facade 不改变调用顺序和错误传播。
- 生产后端在 Linux offscreen/forced Qt context 下：`None` 仍 `Applied`，显式材质仍 `Unsupported`；软件层测试单独运行，不把 offscreen 伪装为原生窗口成功。
- 在可用 X11/Wayland runner 上增加 `Automatic` 返回状态和 layer 可见性检查；没有原生显示 runner 时保持“未执行”，不以 offscreen 结果替代。

## 5. 性能与基准

### 5.1 单独 benchmark

新增 `benchmarks/ZzBackdropBenchmark` 和 `benchmark.backdrop`，只测软件 layer 的：

- 初次启用重建耗时；
- 固定尺寸重复 repaint 的 frame-time；
- resize/DPR/palette 失效后的单次重建；
- 1000 次 enable/disable 的对象数量和累计耗时。

benchmark 不采集桌面截图，不启动动画，不与 QWindowKit native 属性混测。报告必须包含 Qt、编译器、DPR、平台插件和 `software-material` 实现标识。

### 5.2 门禁规则

- 先运行三轮噪声分析，再为 `backdrop.rebuild`、`backdrop.frame-time` 和 `backdrop.object-count` 分别设置 `gate` 或 `observe`；不复用全局 10% 阈值。
- 在 Mesa llvmpipe/Xvfb 上若软件材质不满足绝对 16.7ms frame-time，必须降低纹理分辨率或改为 `observe`，不能靠提高线程数、绕过 taskset 或删除场景。
- `scripts/ci/run-linux-gates.sh` 只有在 benchmark 报告 schema、基线和比较器合同测试同时补齐后才接入新场景。

## 6. 平台静态检查

- Windows：确认新代码不 include `windows.h`、DWM、WinUI、COM 或 native handle；原生 fallback 只通过已有私有后端入口。
- MinGW：确认没有 MSVC 专属属性、异常模型或链接选项；软件层只使用 Qt 公共 API。
- macOS：确认不 include Objective-C、Cocoa、CoreAnimation 或 Carbon；`Automatic` 原生 Blur 失败时进入同一软件层。
- Linux/X11/Wayland：确认不 include Xlib/XCB/Wayland client；不使用 `QScreen::grabWindow(0)`，Automatic 的软件层只绘制宿主内部材质。
- 架构审计：`QWK/QWindowKit/qwindowkit` 仍只出现在 `ZzWindowKit/src/private/ZzQWindowKitBackend.*` 和既有允许位置；软件层不增加第三方依赖。

## 7. 文档与提交顺序

1. `文档：细化 ZzWindowKit 软件材质背景路线`：本计划、边界和失败语义。
2. `窗口：新增跨平台软件材质背景层`：四文件私有实现、QWindowKit fallback、单元测试。
3. `性能：补充软件材质背景基准与门禁`：benchmark、报告、阈值和 CI 接线；未满足门禁时只提交诊断与 observe 记录。
4. `文档：同步软件材质平台支持状态`：更新 `PLATFORM_SUPPORT_ZH.md`、README 和人工验收清单。

每次提交前执行 `git diff --check`，只提交本批明确文件。`temp_image/`、build 目录、性能临时报告和 QWindowKit 上游源码不得进入提交。

## 8. 退出条件

- 软件层无公共头、无平台 native API、无 Qt Private、无桌面截图、无每帧缓存重建。
- Linux GCC Debug/Release、Static、LTO 与定向 WindowKit 测试通过；Clang-Tidy shared/static 通过；Sanitizer 测试本体通过并如实记录 LSan 环境限制。
- `Automatic` 在 Linux 可验证地启用软件材质，显式平台材质保持 `Unsupported`；Windows/macOS 只完成静态检查时不得写成通过。
- benchmark 报告能区分软件材质与原生材质，并有版本化 profile、基线和逐指标比较结果。
- 平台支持文档明确：“软件材质不是系统 Mica/Acrylic，不采样桌面，不替代原生材质；高对比或性能策略由调用方请求 `None`”。
