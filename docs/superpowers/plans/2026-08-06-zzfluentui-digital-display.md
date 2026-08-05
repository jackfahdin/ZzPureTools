# ZzFluentUI 数字显示控件实施计划

**目标：** 让标准 `QLCDNumber` 在应用级 `ZzFluentStyle` 下获得稳定、清晰且高性能的 Fluent 数字显示表面，同时完整保留 Qt 原生数值、文本、进制、段样式、溢出和无障碍语义。

**架构：** 本批不新增 `ZzLCDNumber` 空包装类。`QLCDNumber` 继续拥有全部显示状态，`ZzFluentStyle` 只在 `CE_ShapedFrame` 且目标 widget 确认为 `QLCDNumber` 时绘制 Fluent frame；数字段仍由 Qt 公共实现绘制。透明显示使用标准 `QFrame::NoFrame`，实时钟表由应用 presenter 提供时间文本，UI 样式不创建 timer 或读取系统时间。

**技术约束：** Qt 6.8+、C++20、简体中文 Doxygen、传统命名空间、无 Qt Private API、无 QSS、无动态属性、无每实例 proxy style、无平台原生头、无业务时间源。

## 1. 批次边界

本批实现：

- 应用级 `ZzFluentStyle` 对标准 `QLCDNumber` 的圆角表面、边框、禁用态和主题响应。
- `Dec`、`Hex`、`Oct`、`Bin` 模式，`Outline`、`Filled`、`Flat` 段样式、整数/浮点/文本显示和 small decimal point 的兼容验证。
- `QFrame::NoFrame` 透明路径，不额外发明 `isTransparent` 状态。
- 单元测试、安装消费、控件画廊、性能与三主题四档 DPR 视觉基线。

本批不实现：

- 自动时钟、秒表、倒计时、定时刷新、时区或日期格式。调用方用 presenter/controller 产生字符串并调用 `display()`。
- 第二份 displayed value、字符串格式器、数据模型、单位换算或业务阈值颜色。
- 自定义七段数码管引擎。Qt 已提供成熟的段布局、字符约束、进制和溢出协议。
- 旧版 `ZzLCDNumber` 源码/API 兼容。新项目允许破坏旧 API，只保留经审计有价值的视觉意图。

## 2. 旧版逐行审计

审计来源为旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI`，只读取产品意图，不复制实现。

### 2.1 `ZzLCDNumber.h`

| 行 | 结论 |
|---:|---|
| 1-8 | include guard 可用，但公共头依赖旧导出宏和属性宏；新版标准控件路径不增加任何公开头。 |
| 10-17 | 为三个属性和 d-pointer 新增包装类；自动时钟是业务行为，透明状态可由 `QFrame::NoFrame` 表达，均不足以支持新类型。 |
| 19-22 | 两个构造函数只转发 `QLCDNumber` 已有构造能力，没有独立协议价值。 |
| 24-25 | `paintEvent()` 只用于反复修正 palette，表明旧主题传播不稳定；新版由主题 snapshot 和应用级 style 一次传播。 |

### 2.2 `ZzLCDNumber.cpp`

| 行 | 结论 |
|---:|---|
| 1-7 | UI 类型直接依赖主题单例、系统日期时间、timer 和独立 style，混合视觉、数据源和调度职责。未使用的 `QDebug` 应删除。 |
| 8-20 | 每个实例分配 private 和 proxy style，并通过 QSS 清透明背景；新版全部复用单个应用级 `ZzFluentStyle`，不产生实例样式或 QSS。 |
| 13-16 | 默认时钟格式反向决定 digit count，普通数字显示构造后也得到日期长度；新版保留 Qt 默认值，不暗改显示容量。 |
| 21-24 | 每个实例无条件创建 `QTimer`，即使从未开启时钟；200 ms 轮询对只显示秒的格式产生 5 倍无效唤醒。新版 UI 路径 timer 数为 0。 |
| 26-27 | 每个实例连接永久主题单例，扩大对象图并隐藏生命周期；新版 style 只连接显式注入的 `ZzThemeController`。 |
| 30-34 | digit 构造只是 Qt API 转发，不迁移。 |
| 36-40 | 手工删除已安装到 widget 的 style 存在 QObject/style 所有权歧义；新版没有每实例 style。 |
| 42-58 | 启停时钟同时清空显示值，混合业务调度和 UI 状态；相同值也重复发信号。整体不迁移。 |
| 60-78 | auto clock getter/format 只服务被删除的业务功能；返回内部 `QString` 引用也扩大 ABI 约束。 |
| 80-92 | 透明属性只控制独立 style 是否画 frame；标准 `setFrameStyle(QFrame::NoFrame)` 已提供相同意图。 |
| 94-102 | 每帧比较并重写 palette 会触发额外事件，掩盖主题传播问题；新版 paint 热路径不修改状态。 |

### 2.3 `ZzLCDNumberPrivate.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-15 | private 继承 `QObject` 并启用 meta-object，只为主题槽和属性存储；对简单显示控件是无必要的 QObject。 |
| h:17-25 | timer、实例 style、主题模式和显示状态形成三套间接状态；新版全部删除。 |
| cpp:4-11 | 空构造/析构没有独立资源协议。 |
| cpp:13-20 | 主题槽直接改控件 palette，只更新 Text/WindowText 且未完整处理 disabled/high contrast；新版使用完整 theme snapshot palette。 |

### 2.4 `ZzLCDNumberStyle.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-17 | 每实例 `QProxyStyle`、meta-object 和全局主题模式只服务一个 frame 绘制，成本和所有权面过大。 |
| cpp:7-14 | 构造参数 `style` 未传给 `QProxyStyle`，主题连接捕获 `this` 的生命周期依赖隐式 QObject 断连。 |
| cpp:20-29 | 在 `CE_ShapedFrame` 分支绘制背景的入口正确；新版将该逻辑收敛到应用级 style，并严格限定 `QLCDNumber` 上下文。 |
| cpp:30-41 | 圆角边框与半透明表面是可保留的视觉意图，但硬编码颜色、重复 pen/brush 和整数/半像素矩形混用会产生 DPR 差异；新版使用 theme token 和统一 `QRectF`。 |
| cpp:44-51 | 命中分支无条件 return，透明时连 base frame 也不绘制；新版 `NoFrame` 由 Qt 控件决定是否请求 frame，不引入隐藏布尔状态。 |

## 3. 实现设计

### 3.1 样式路由

在 `ZzFluentStyle::drawControl()` 中增加唯一入口：

```cpp
if (element == CE_ShapedFrame
    && qobject_cast<const QLCDNumber *>(widget) != nullptr) {
    const auto *frame = qstyleoption_cast<
        const QStyleOptionFrame *>(option);
    if (frame != nullptr && painter != nullptr) {
        d_ptr->drawDigitalDisplayFrame(frame, painter);
        return;
    }
}
```

- 只有标准 `QLCDNumber` 及其派生类型进入新路径；普通 `QFrame`、`QListView`、`QTableView` 和其他 shaped frame 继续委托 base style。
- 空 option、错误 option 类型或空 painter 继续走 `QProxyStyle`，不解引用空指针。
- 不读取对象名、动态属性或旧版类型，不增加字符串路由。

### 3.2 绘制契约

在 `ZzFluentStylePrivate` 增加带中文 Doxygen 的 `drawDigitalDisplayFrame()`：

- 使用 `option->rect` 的逻辑坐标，向内收缩 `0.5` 像素，以 6 逻辑像素圆角一次绘制 fill 和 1 像素 stroke。
- enabled 状态使用 `SurfaceSecondary` 填充和 `ControlStroke` 边框；disabled 使用 `ControlFillDisabled` 填充，边框仍保持高对比可辨识。
- 颜色来自当前不可变 theme snapshot，不读文件、不创建 pixmap、不分配 QObject、不修改 widget palette 或 geometry。
- frame 绘制后直接返回，数字段继续由 `QLCDNumber::paintEvent()` 的后续 Qt 原生路径绘制。
- `QFrame::NoFrame` 保持透明：Qt 即使请求 shaped frame，样式也直接返回且不绘制 surface；样式不保存第二份透明状态。

### 3.3 Qt 原生语义

- `display(int/double/QString)`、`value()`、`intValue()`、`digitCount()`、`mode()`、`segmentStyle()`、`smallDecimalPoint()`、`checkOverflow()` 和 `overflow()` 完全由 Qt 维护。
- 不 override paint/input/event API，不重新解释负号、小数点、冒号和合法字符。
- 不启动 timer 或 animation；频繁更新只触发 Qt 本身必要的 update。
- 应用需要时钟时，在 UI 外部 presenter 选择刷新频率、时区和格式，再把展示字符串写入 `QLCDNumber`。

## 4. 测试计划

扩展 `ZzFluentStandardControlsTest.cpp`：

- 标准 `QLCDNumber` 安装 `ZzFluentStyle` 后仍正确处理 digit count、整数、浮点、文本、四种进制和三种 segment style。
- `checkOverflow()` 与 `overflow()` 保持 Qt 原生行为。
- enabled、disabled、Light、Dark、HighContrast 的 frame token 正确；主题切换后无需重建控件或 style。
- `QFrame::NoFrame` 不绘制 Fluent panel，切回 `QFrame::Box` 后恢复。
- 普通 `QFrame` 的 `CE_ShapedFrame` 输出仍与 base style 一致，证明样式路由未外溢。
- 构造、显示、主题切换和销毁过程中没有新增 `QTimer`、`QAbstractAnimation` 或每实例 `QStyle` 子对象。
- QAccessible 接口可获取，name/description 由调用方设置，显示数值和 enabled 状态保持标准 Qt 语义。

安装消费在 `tests/InstallConsumer/Gui/main.cpp` 使用安装后的 `ZzFluentStyle` 渲染一个标准 `QLCDNumber`，不依赖私有头或未导出符号。

## 5. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 创建 100 个可见标准 `QLCDNumber`，预热后循环更新显示值并渲染 120 帧。
- reference 发布机记录 P50、P95、max；P95 不超过 `8 ms`，低于 60 Hz 单帧预算的一半。
- 1000 轮显示值、enabled、frame style 和主题切换后，QObject 后代、animation、timer 和 style 对象数量不得增长。
- benchmark 只测 UI 更新和绘制，不把日期格式化、系统时钟读取或业务计算混入结果。

## 6. 示例与视觉基线

- 控件画廊新增“数字显示”区域，展示 decimal、hex、flat、disabled 和 transparent 五种标准 `QLCDNumber` 状态；页面只写固定展示值。
- 截图测试增加固定尺寸 `digital-display` surface，覆盖常规、负数、小数、hex、disabled 和 no-frame，且对关键区域做非空与不重叠断言。
- 生成 Light、Dark、HighContrast 三主题与 DPR 1.0、1.25、1.5、2.0 共 12 张图片。
- 人工检查至少三主题 DPR 1.0 和 Light DPR 2.0，确认段线清晰、边框连续、数字不裁切且 no-frame 无意外底板。

## 7. 构建与跨平台边界

- 本批只修改 `ZzFluentStyle` 私有绘制、既有测试、安装消费者、benchmark 和示例，不新增公开 ABI 或安装头。
- 新增代码只使用 Qt Core/Gui/Widgets 公共 API 和标准 C++20；不引入平台条件分支。
- 运行 GCC shared/static、Clang ASan+UBSan、shared/static Clang-Tidy、安装消费、重定位、公开头、完整架构、Fluent 边界、二进制依赖、preset matrix 和 gate script contract。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 只做源码、preset、公开 ABI、依赖和条件编译静态审计；没有对应工具链证据前不得记录为原生编译或真机通过。

## 8. 提交顺序

每个逻辑批次验证后立即提交：

```text
文档：规划Fluent数字显示批次
样式：完善Fluent数字显示表面
测试：接入数字显示质量与性能门禁
测试：补齐数字显示多主题视觉基线
文档：记录数字显示批次交付结果
```

提交标题使用中文简述，正文用多个中文段落说明修改细节。不 push，不调用 GitHub CLI，不处理远端 CI，不下载 Qt。
