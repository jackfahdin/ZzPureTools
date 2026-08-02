# ZzPureToolsPro 重构架构设计

> 文档状态：已确认，可用于编写分阶段实施计划
>
> 确认日期：2026-08-02
>
> 目标仓库：`/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`
>
> 基线提交：`a2737073d5a916d54a5b21339635cd9c1019b79d`
>
> 当前范围：架构与实施约束，不包含实现代码

## 1. 文档目的

本文定义 ZzPureToolsPro 新版本的代码级架构、模块边界、公共接口方向、线程与所有权规则、构建矩阵、性能指标和分阶段交付条件。

本文必须能够直接交给另一位开发者或 AI 作为后续实施依据。实施者不得用个人偏好替换本文中的硬性约束；如发现无法实现、相互矛盾或需要扩大范围的条目，必须先补充 ADR 并取得确认。

本文使用下列措辞：

- “必须”“禁止”：不可绕过的验收条件。
- “应”：默认执行；只有明确记录理由后才能偏离。
- “可”：允许但不要求在当前阶段实现。

## 2. 已确认目标

### 2.1 产品与技术目标

1. 使用 Qt 6.8 或更高版本，只支持 Qt 6，不再兼容 Qt 5。
2. 全部一方 C++ 代码使用 C++20，关闭编译器扩展。
3. 以 Qt Widgets 作为第一阶段 UI 技术路线，同时保留未来独立增加 `ZzFluentQuick` 的边界。
4. 使用统一 Fluent 视觉语言，但保留各平台的窗口按钮、系统菜单、快捷键、字体、输入与无障碍行为。
5. Linux 是第一实现和动态验证平台；Windows、macOS 第一阶段完成编译和静态检查，真机交互由用户后续验证。
6. Windows 正式支持两套互不混用的工具链：MSVC 2022 x64，以及当前 Qt SDK 官方配套的 64 位 MinGW-w64。
7. 默认构建共享库，同时把静态库作为持续验证和正式支持的构建方式。
8. UI 只能依赖展示层契约，可使用只读 ViewModel 和 `QAbstractItemModel`，但禁止访问领域模型、数据库、网络和业务服务。
9. 新项目允许破坏旧版 API 和 ABI，只保留经过筛选的视觉、交互和类名概念。
10. 第一阶段采用编译期模块注册与构造函数注入，只预留动态插件扩展边界，不加载外部二进制插件。
11. 控件分阶段实现，不要求第一次提交就重写旧仓库的全部控件。

### 2.2 组件范围

保留四个主要物理组件：

- `ZzCore`：非 UI 基础设施。
- `ZzWindowKit`：跨平台无边框窗口适配。
- `ZzFluentUI`：Fluent 主题、样式和控件。
- `ZzPureTools`：应用框架、导航、页面与窗口组合。

现有基础依赖：

- `ZzThirdParty/ZzLog`：基于 spdlog/fmt 的日志组件，需要进行 C++20 与规范升级。
- `ZzThirdParty/qwindowkit`：QWindowKit 1.5.1.0 源码快照，只能作为 `ZzWindowKit` 的私有后端。

### 2.3 明确非目标

第一阶段不做以下工作：

- 不保证旧版 ZzPureToolsPro 的源码兼容或二进制兼容。
- 不实现 Qt Quick/QML 控件，只保留未来 `ZzFluentQuick` 的依赖边界。
- 不实现可动态加载或卸载的外部插件。
- 不恢复旧版全局 EventBus、全局撤销栈或永久服务定位器。
- 不为了“现代 C++”而手写未经验证的协程调度器、容器或图形算法。
- 不以 `offscreen` 测试替代 Windows、macOS、X11 或 Wayland 的真实窗口验证。
- 不在主题和核心接口稳定前一次性重写全部复杂控件。

## 3. 总体架构

### 3.1 依赖方向

下图中箭头表示“右侧目标依赖左侧目标”：

```text
Qt6::Core/Concurrent + ZzLog
             |
             v
         Zz::Core -------------------+
             |                        |
             +-> Zz::AppCore         +-> Zz::WindowKit <- QWindowKit（PRIVATE）
             |                              |
             +-> Zz::FluentFoundation       |
                        |                    |
                        v                    |
                  Zz::FluentUI               |
                        |                    |
                        +---------+----------+
                                  |
                                  v
                            Zz::PureTools
```

`ZzLog` 是跨组件的私有基础依赖。任何一方组件的公共头都不得暴露 spdlog 或 fmt 类型。

未来 Quick 路径固定为：

```text
Zz::Core + Zz::AppCore + Zz::FluentFoundation
                         |
                         v
                  Zz::FluentQuick
```

`ZzFluentQuick` 不得依赖 `Zz::FluentUI`，因为后者是 Widgets 目标。

### 3.2 CMake 目标与职责

| 物理组件 | 导出目标 | 允许的主要依赖 | 职责 |
|---|---|---|---|
| `ZzCore` | `Zz::Core` | Qt Core/Concurrent、ZzLog | Result/Error、任务、取消、路径、设置、Qt 日志桥 |
| `ZzWindowKit` | `Zz::WindowKit` | ZzCore、Qt Core/Gui/Widgets；私有 QWK | 无边框窗口能力与平台结果语义 |
| `ZzFluentUI` | `Zz::FluentFoundation` | ZzCore、Qt Core/Gui | 主题令牌、排版、尺寸、动画参数 |
| `ZzFluentUI` | `Zz::FluentUI` | FluentFoundation、Qt Widgets/Svg | Widgets 样式、控件和 delegate |
| `ZzPureTools` | `Zz::AppCore` | ZzCore、Qt Core | 模块图、生命周期、路由 ID、无 UI 应用契约 |
| `ZzPureTools` | `Zz::PureTools` | AppCore、WindowKit、FluentUI | Widgets 应用构建器、应用窗口、页面宿主、导航和装配 |

`Zz::` 是 CMake 导出命名空间，不属于用户所禁止的 C++ 链式 namespace 语法。

### 3.3 硬性禁止依赖

1. `ZzCore` 禁止链接或包含 Qt Gui、Widgets、Quick。
2. `ZzWindowKit` 与 `ZzFluentUI` 禁止互相依赖。
3. `ZzAppCore` 禁止依赖 QWidget、ZzWindowKit 或 ZzFluentUI。
4. `ZzFluentUI` 禁止依赖 ZzPureTools、领域层或具体业务模块。
5. 一方代码禁止包含 Qt Private 头；该依赖只允许存在于 vendored qwindowkit 内部。
6. `QWK` 类型、头文件、枚举、字符串属性和 QML URI 禁止出现在 Zz 公共 API 中。
7. UI 目录禁止包含 repository、database、network client、domain entity 的头文件。
8. 领域层应保持纯 C++20；如需 Qt 适配，适配代码位于应用层或展示层。

这些规则必须由 CMake target 可见性、公共头独立编译和架构扫描测试共同执行，不能只依赖代码评审记忆。

## 4. 仓库和目录组织

目标目录结构：

```text
ZzPureToolsPro/
├── CMakeLists.txt
├── CMakePresets.json
├── CMakeUserPresets.json.example
├── cmake/
│   ├── ZzCompilerWarnings.cmake
│   ├── ZzSanitizers.cmake
│   ├── ZzInstallPackage.cmake
│   └── ZzArchitectureChecks.cmake
├── docs/
│   ├── adr/
│   ├── development/
│   ├── performance/
│   ├── superpowers/
│   │   ├── plans/
│   │   └── specs/
│   └── third-party/
├── examples/
├── benchmarks/
├── tests/
│   ├── Architecture/
│   └── InstallConsumer/
├── ZzCore/
│   ├── include/ZzCore/
│   ├── src/private/
│   └── tests/
├── ZzWindowKit/
│   ├── include/ZzWindowKit/
│   ├── src/private/
│   ├── src/platform/
│   └── tests/
├── ZzFluentUI/
│   ├── foundation/include/ZzFluentUI/
│   ├── foundation/src/private/
│   ├── widgets/include/ZzFluentUI/
│   ├── widgets/src/private/
│   ├── resources/
│   └── tests/
├── ZzPureTools/
│   ├── appcore/include/ZzPureTools/
│   ├── appcore/src/private/
│   ├── widgets/include/ZzPureTools/
│   ├── widgets/src/private/
│   └── tests/
└── ZzThirdParty/
    ├── ZzLog/
    └── qwindowkit/
```

只有 `include/<组件名>/` 下的文件可以安装。`src/private/`、`src/platform/`、测试辅助类和第三方头一律不安装。

禁止通过 `../../` 穿越组件包含头文件。所有包含关系必须由 target 的 include directory 明确表达。

## 5. C++ 代码规范

### 5.1 类、文件和 namespace

1. 除 `main.cpp` 外，一个类文件组只定义一个主要自定义类。
2. 所有自定义类、结构体、枚举和概念名称均以 `Zz` 开头。
3. 文件名必须与主要类型完全一致，包括大小写。
4. 公共头使用 `.h`，实现使用 `.cpp`；Objective-C++ 平台实现可使用 `.mm`。
5. C++ namespace 使用单层或传统嵌套写法。

正确写法：

```cpp
namespace ZzWindowKit {

namespace Internal {

class ZzPlatformCapability final
{
};

} // namespace Internal

} // namespace ZzWindowKit
```

禁止写法：

```cpp
namespace ZzWindowKit::Internal {
}
```

### 5.2 中文 Doxygen

公开类、公开枚举、公开方法、信号以及复杂算法必须使用简体中文 Doxygen。公开方法至少说明：

- 方法责任。
- 参数含义与是否允许为空。
- 返回值和失败语义。
- 所有权关系。
- 线程要求。
- 有效生命周期或状态前提。

禁止用“设置值”“返回值”等无信息注释机械覆盖代码。简单私有 getter、显然的 override 可以不添加重复注释。

### 5.3 PIMPL 与四文件结构

下列公开类型必须采用四文件 PIMPL：

- 导出的有状态 QObject 或 QWidget。
- 包含平台 API、第三方实现或长期 ABI 状态的公开类。
- 状态较复杂、需要稳定公共头依赖面的公开控制器。

标准文件组：

```text
ZzWindowAgent.h
ZzWindowAgent.cpp
ZzWindowAgentPrivate.h
ZzWindowAgentPrivate.cpp
```

标准所有权：

```cpp
namespace ZzWindowKit {

class ZzWindowAgentPrivate;

class ZZ_WINDOWKIT_EXPORT ZzWindowAgent final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzWindowAgent)

public:
    explicit ZzWindowAgent(QObject *parent = nullptr);
    ~ZzWindowAgent() override;

private:
    std::unique_ptr<ZzWindowAgentPrivate> d_ptr;
};

} // namespace ZzWindowKit
```

约束：

- 析构函数必须在 `.cpp` 中定义，确保 private 类型完整。
- private 对象由 `std::unique_ptr` 唯一拥有。
- private 对象不得同时由 QObject parent 和智能指针拥有。
- private 对象可以保存指向 public 对象的非拥有指针，但二者生命周期关系必须固定。
- public QObject/QWidget 默认禁用复制和移动。

以下类型不强制四文件：

- 纯抽象接口。
- 枚举和 Concept。
- 小型不可变值类型。
- 模板或 header-only 算法。
- 不导出的 `final` 私有辅助类。

PIMPL 对每个对象引入一次分配和一次间接访问，对窗口、页面和普通控件不是主要性能瓶颈。paint、布局、模型遍历等热路径仍必须避免额外分配、层层虚调用和临时容器。

### 5.4 C++20 使用原则

应使用：

- Concept 约束模板接口。
- `std::span` 表达不拥有的连续数据。
- ranges 简化不会隐藏复杂度的数据变换。
- `std::jthread` 和 `std::stop_token` 实现可取消长期任务。
- `std::source_location` 记录诊断位置。
- `[[nodiscard]]` 标记 Result 和关键操作结果。

禁止仅为展示语法而在 paint、事件分发或简单 Qt 信号链中引入协程或 ranges 管道。

## 6. 前后端分离

### 6.1 数据流

```text
Domain
  -> UseCase
  -> Presenter
  -> 只读 ViewModel / QAbstractItemModel
  -> QWidget

QWidget 用户操作
  -> intent 信号或 ZzCommand
  -> Presenter
  -> UseCase
```

### 6.2 UI 可以做的事情

- 读取 ViewModel 的只读属性。
- 连接 ViewModel 的变更信号。
- 使用 `QAbstractItemModel`、delegate 和 proxy model 展示数据。
- 发出点击、提交、选择、导航等用户意图。
- 根据展示状态更新可见性、样式、焦点和动画。

### 6.3 UI 禁止做的事情

- 调用 repository、数据库、网络 client 或文件存储。
- 包含领域实体并直接解释业务规则。
- 在点击槽中执行完整业务流程。
- 从全局容器查找业务 service。
- 在 worker 线程直接修改 QWidget 或 UI model。

### 6.4 展示模型约束

- ViewModel 与 UI model 固定属于 GUI 线程。
- UI 只使用 `QPointer` 保存外部 QObject，不接管 ViewModel 所有权。
- 后台结果先回到 Presenter，再由 Presenter 在 GUI 线程修改展示模型。
- 列表增量变化使用 `beginInsertRows()`、`beginRemoveRows()`、`dataChanged()` 等协议。
- 禁止为了方便频繁使用 `beginResetModel()`。
- 排序、过滤、分页和展示格式转换属于展示层，不属于 QWidget。
- 页面实例必须在销毁前取消与其关联的异步任务。

## 7. ZzCore 设计

### 7.1 责任范围

`ZzCore` 只依赖 Qt Core/Concurrent、标准库和私有 ZzLog。第一批公共类型：

| 类型 | 责任 |
|---|---|
| `ZzError` | 稳定错误码、技术描述和上下文 |
| `ZzResult<T>` | 值或错误的显式返回 |
| `ZzTaskExecutor` | 应用拥有的独立线程池 |
| `ZzTaskHandle<T>` | 任务结果、状态和协作式取消 |
| `ZzApplicationPaths` | 基于 `QStandardPaths` 的应用路径 |
| `ZzSettingsStore` | 可注入设置接口 |
| `ZzQtSettingsStore` | 基于 `QSettings` 的默认实现 |
| `ZzQtLogBridge` | Qt message handler 到 ZzLog 的桥接 |

`ZzCore` 不承担主题、图标、导航、EventBus、业务命令或全局撤销。

### 7.2 Result 与错误

```cpp
namespace ZzCore {

template<typename T>
class [[nodiscard]] ZzResult final
{
public:
    [[nodiscard]] bool hasValue() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] T &value() &;
    [[nodiscard]] const T &value() const &;
    [[nodiscard]] const ZzError &error() const & noexcept;

private:
    std::variant<T, ZzError> storage_;
};

} // namespace ZzCore
```

需要提供 `ZzResult<void>` 专门实现。预期失败使用 Result，不使用异常控制正常流程。

规则：

- `ZzError` 不直接携带最终用户 UI 文案；Presenter 负责映射可本地化消息。
- 第三方、任务和模块边界必须捕获异常并转换成 `ZzError`。
- Qt 信号槽边界禁止传播异常。
- Debug 下对程序员错误使用断言；Release 仍要返回可诊断错误或进入明确失败状态。
- `value()` 在无值时的契约必须明确；内部优先使用已检查访问。

### 7.3 任务执行与取消

```text
ZzTaskExecutor
  -> 自有 QThreadPool
  -> QPromise/QFuture
  -> ZzTaskHandle<T>
  -> 带 QObject context 的 GUI 线程回调
```

规则：

1. 不使用 Qt 全局线程池承载所有业务任务。
2. `submit()` 使用 Concept 限制 callable 的参数与返回类型。
3. 取消是协作式的，不调用强制线程终止。
4. 长生命周期 worker 使用 `std::jthread` 和 `std::stop_token`。
5. worker 不捕获 QWidget、ViewModel 裸指针或可变 UI 状态。
6. 完成回调必须带 QObject context；context 销毁后自动丢弃结果。
7. shutdown 开始后拒绝新任务，统一请求取消并按 deadline 等待。
8. 第一阶段不手写独立协程调度器。
9. 只有在开始、挂起、恢复线程、取消、接收者销毁和异常路径均有测试后，才增加基于 `QFuture` 的 coroutine awaiter。

### 7.4 路径和设置

- 所有应用目录通过 `QStandardPaths` 和 `QDir::filePath()` 生成。
- 禁止手工拼接 `/` 或 `\\`。
- UI 不直接使用 `QSettings`。
- 设置读写通过 `ZzSettingsStore` 注入 Presenter 或应用服务。
- 设置 key 应集中定义并具备类型、默认值和迁移策略。
- 重要配置文件写入应使用原子替换策略。

### 7.5 通信与撤销

- 不提供字符串 EventBus。
- 同一组件内优先直接接口调用或强类型 Qt 信号。
- 跨线程连接必须明确使用 queued connection，并提供接收 context。
- 每个文档或窗口拥有自己的 `QUndoStack`/`QUndoGroup`。
- 禁止使用进程级固定字符串 domain 共享撤销历史。

## 8. ZzLog 设计

### 8.1 当前基线

现有 ZzLog 已具备独立 CMake target、控制台 sink、异步轮转文件 sink、并发 smoke test、安装导出和许可证清单。当前需要修正：

- CMake 仍要求 C++17。
- 使用 `namespace zz::log` 链式 namespace。
- `Level`、`Config` 等自定义类型没有 `Zz` 前缀。
- 公开注释不是简体中文 Doxygen。
- fmt 类型出现在公开 API。
- `initialize()`/`shutdown()` 要求生命周期操作期间没有线程写日志，该条件需要由应用生命周期保证。

### 8.2 新公共规则

- namespace 改为传统的 `namespace ZzLog { ... }`。
- 类型改名为 `ZzLogLevel`、`ZzLogOverflowPolicy`、`ZzLogConfig`、`ZzLogInitError`、`ZzLogInitResult` 等。
- target 要求 `cxx_std_20`。
- spdlog 和 fmt 保持实现细节，不出现在其他一方组件公共头中。
- 对外格式化入口使用 `std::format_string<Args...>` 进行 C++20 编译期格式检查；MSVC、Qt 官方 MinGW 和 Apple Clang 必须先通过编译探针。
- 日志方法保持 `noexcept`，并且只在对应等级启用时格式化。
- 使用 `std::source_location` 记录调用位置，但不把绝对源码根路径写入发布日志。
- Release 默认异步文件 sink 使用不阻塞 UI 的溢出策略，并暴露丢弃计数。
- Critical 和显式 flush 的语义必须有测试。

共享库模式下必须只存在一个 ZzLog 运行时，禁止把静态 ZzLog 分别嵌入多个共享组件。静态模式下最终消费者链接同一个 ZzLog target。

### 8.3 Qt 日志桥

`ZzQtLogBridge` 由应用显式拥有：

- 安装时保存旧 handler，卸载时恢复。
- handler 内禁止调用 `qDebug()`、`qWarning()` 等 Qt 日志函数。
- 使用 thread-local 防重入。
- 不从任意日志线程直接发出 UI QObject 信号。
- Debug、Info、Warning、Critical、Fatal 均有固定映射。
- 是否链式调用旧 handler 由配置明确决定。
- 停止顺序必须保证所有 worker 停止记录日志后才关闭 ZzLog。

## 9. ZzWindowKit 设计

### 9.1 qwindowkit 审查结论

vendored qwindowkit 自报版本为 1.5.1.0，主体许可证为 Apache-2.0，bundled qmsetup/syscmdline 为 MIT。它支持 Windows Win32、macOS Cocoa、Linux X11/Wayland 和 Qt fallback。

必须记录的风险：

1. 当前目录没有上游 Git commit/tag 元数据，只能确认源码自报版本。
2. qwindowkit 使用 Qt Core/Gui/Widgets Private 模块，二进制与 Qt minor 版本绑定。
3. 安装 Config 的 Qt 主版本与 component 校验不可靠，不得直接作为 Zz 的已安装依赖。
4. 默认 `QWINDOWKIT_INSTALL=ON`，源码集成时必须关闭。
5. qmsetup 缺失时会在配置阶段同步构建并写入顶层 `_build/_install`，CI 必须固定版本和目录行为。
6. macOS system button area API 标记为 experimental，不进入 Zz 公共 API。
7. Linux system menu 在 X11/Wayland 都是 best-effort，不能承诺必然成功。
8. `setWindowAttribute(QString, QVariant)` 依赖未类型化字符串，必须封装。
9. `setup()` 只能调用一次，setup 前调用其他 API 可能解引用空 context。
10. 设置新 title bar 会清空已登记的系统按钮和 hit-test widget。
11. `qmsetup/src/corecmd/utils_win.cpp` 有一段注明修改自 `windeployqt 5.15.2`，发布前必须核实再许可依据。

关键证据位置：

| 结论 | 当前源码位置 |
|---|---|
| 版本、默认选项 | `ZzThirdParty/qwindowkit/CMakeLists.txt:3-22` |
| Core/Widgets/Quick 导出目标 | `ZzThirdParty/qwindowkit/src/CMakeLists.txt:80-88` |
| Widgets 使用 Qt Private | `ZzThirdParty/qwindowkit/src/widgets/CMakeLists.txt:21-29` |
| Core 使用 Qt Private | `ZzThirdParty/qwindowkit/src/core/CMakeLists.txt:90-98` |
| setup 只允许一次 | `ZzThirdParty/qwindowkit/src/widgets/widgetwindowagent.cpp:47-72` |
| title bar 会清空注册 | `ZzThirdParty/qwindowkit/src/widgets/widgetwindowagent.cpp:88-99` |
| setup 前 context 风险 | `ZzThirdParty/qwindowkit/src/core/windowagentbase.cpp:87-94` |
| macOS experimental API | `ZzThirdParty/qwindowkit/src/widgets/widgetwindowagent.h:32-39` |
| 配置期 qmsetup bootstrap | `ZzThirdParty/qwindowkit/qmsetup/cmake/modules/private/InstallPackage.cmake:53-173` |

### 9.2 集成配置

```cmake
QWINDOWKIT_BUILD_STATIC=ON
QWINDOWKIT_INSTALL=OFF
QWINDOWKIT_BUILD_WIDGETS=ON
QWINDOWKIT_BUILD_QUICK=OFF
QWINDOWKIT_BUILD_EXAMPLES=OFF
QWINDOWKIT_BUILD_DOCUMENTATIONS=OFF
QWINDOWKIT_ENABLE_STYLE_AGENT=OFF
```

顶层必须先执行 `find_package(Qt6 6.8 REQUIRED ...)`。qwindowkit 通过 `add_subdirectory(... EXCLUDE_FROM_ALL)` 加入，且只有 `ZzWindowKit` 的 private 实现可以链接 QWindowKit target。

### 9.3 公共类型

建议第一批公共类型：

- `ZzWindowAgent`
- `ZzWindowAgentState`
- `ZzWindowChromeConfiguration`
- `ZzWindowCapability` / `ZzWindowCapabilities`
- `ZzWindowBackdrop`
- `ZzWindowColorScheme`
- `ZzWindowApplyState`
- `ZzWindowKitBootstrap`

公开代理草案：

```cpp
namespace ZzWindowKit {

class ZZ_WINDOWKIT_EXPORT ZzWindowAgent final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzWindowAgent)

public:
    explicit ZzWindowAgent(QObject *parent = nullptr);
    ~ZzWindowAgent() override;

    [[nodiscard]] ZzCore::ZzResult<void> attach(QWidget *window);

    [[nodiscard]] ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration);

    [[nodiscard]] ZzWindowAgentState state() const noexcept;
    [[nodiscard]] ZzWindowCapabilities capabilities() const noexcept;

    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop);

    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme);

    [[nodiscard]] ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition);
};

} // namespace ZzWindowKit
```

### 9.4 状态机和生命周期

```text
Detached -> Attached -> Configured
              |             |
              +-------> Invalidated
              |
              +-------> Failed
```

- `attach()` 只能成功一次。
- 必须检查当前线程、host 线程、顶层 QWidget 条件和 backend 状态。
- host 销毁后进入 `Invalidated`，同一 agent 禁止绑定另一窗口。
- 不提供 `detach()` 或运行时恢复原生 frame。
- agent 应与 host 同线程，并由 host 或更短生命周期的窗口控制器拥有。
- private 层用 `QPointer` 跟踪 host、标题栏、按钮和 hit-test widget。
- 原生事件回调期间如需销毁，必须采用延迟销毁策略。

### 9.5 标题栏配置

```cpp
namespace ZzWindowKit {

struct ZzWindowChromeConfiguration final
{
    QWidget *titleBar = nullptr;
    QWidget *windowIcon = nullptr;
    QWidget *minimizeButton = nullptr;
    QWidget *maximizeButton = nullptr;
    QWidget *closeButton = nullptr;
    QList<QWidget *> interactiveWidgets;
};

} // namespace ZzWindowKit
```

传入指针均为非拥有输入。`configureChrome()` 必须先完成以下校验，再调用后端：

1. title bar 非空并属于 host。
2. 所有按钮和交互 widget 都是 title bar 后代。
3. 所有对象都属于同一 GUI 线程。
4. 列表无空元素和重复元素。
5. 当前状态允许配置。

校验通过后按固定顺序调用：

```text
QWK::WidgetWindowAgent::setTitleBar
 -> setSystemButton(WindowIcon/Minimize/Maximize/Close)
 -> setHitTestVisible(interactiveWidgets)
```

更换 title bar 时必须重新应用完整配置，不能要求调用者理解 QWK 的清空行为。

`ZzWindowKit` 只登记系统按钮角色，不持有 Fluent 视觉逻辑。最小化、最大化、恢复和关闭的点击行为由 `ZzPureTools` 窗口组合层统一连接，并根据平台能力更新按钮状态。

### 9.6 强类型平台效果

`ZzWindowBackdrop` 至少包含：

- `None`
- `Blur`
- `Acrylic`
- `Mica`
- `MicaAlt`
- `Automatic`

QWK 的 `mica`、`mica-alt`、`acrylic-material`、`dwm-blur` 和 `blur-effect` 字符串只存在于 `ZzQWindowKitBackend` 私有实现中。

结果语义：

- `Applied`：已经由当前 native context 应用。
- `Deferred`：等待原生窗口句柄创建后重试。
- `Unsupported`：当前平台、窗口系统或系统版本不支持。
- 后端失败不属于成功状态，由外层失败的 `ZzResult<ZzWindowApplyState>` 携带 `ZzError`。

QWK 在窗口句柄创建前返回 true 只代表属性已暂存，不能直接映射为 `Applied`。

### 9.7 后端隔离与安装

- public 类组合 private `ZzWindowBackend`，默认实现是 `ZzQWindowKitBackend`。
- 不继承 QWK 类。
- 假后端只用于测试 attach、状态机、错误和调用顺序。
- 共享模式把静态 QWK 链入单一共享 `ZzWindowKit`，并隐藏 QWK 符号和部署细节。
- 静态安装模式把 QWK archive 作为重命名的内部实现归档安装到私有目录，由 Zz 生成的内部 imported target 连接。
- 静态消费者不得执行 `find_package(QWindowKit)`，不得需要 QWK 头文件。
- 如果某平台无法满足上述静态封装，静态安装测试必须失败，禁止通过泄露 QWK 公共依赖绕过。
- 二进制包必须记录构建所用 Qt major/minor，并在 Config 阶段拒绝不匹配的 Qt minor。

### 9.8 第三方治理

必须新增 vendor manifest，记录：

- 上游 URL。
- 声明版本和准确 commit。
- 导入日期。
- 源码归档 SHA-256。
- 许可证和 notice。
- 本地补丁列表。
- 已验证 Qt 与平台矩阵。

无法确认准确 commit 时可以继续开发，但属于发布阻塞项。所有上游补丁集中放入 `patches/` 并附原因，禁止把业务改动散落到 qwindowkit 源码。

## 10. ZzFluentUI 设计

### 10.1 Foundation 与 Widgets 分离

`Zz::FluentFoundation` 依赖 Qt Core/Gui，不依赖 Widgets。它提供：

- `ZzThemeController`
- `ZzThemeSnapshot`
- `ZzThemeMode`
- `ZzColorToken`
- `ZzMetricToken`
- `ZzTypographyToken`
- `ZzMotionToken`
- 图标描述和缓存 key

`Zz::FluentUI` 依赖 Foundation，提供 Widgets style、控件、delegate、popup 和标题栏视觉。

### 10.2 主题快照

```cpp
namespace ZzFluentUI {

enum class ZzColorToken : quint16
{
    TextPrimary,
    TextSecondary,
    ControlFill,
    ControlFillHover,
    ControlFillPressed,
    Accent,
    FocusStroke,
    Surface,
    Count
};

class ZzThemeSnapshot final
{
public:
    [[nodiscard]] QColor color(ZzColorToken token) const noexcept;
    [[nodiscard]] qreal metric(ZzMetricToken token) const noexcept;
    [[nodiscard]] int duration(ZzMotionToken token) const noexcept;
    [[nodiscard]] quint64 revision() const noexcept;
};

} // namespace ZzFluentUI
```

实现约束：

- snapshot 不可变。
- 令牌存储使用定长数组，读取为 O(1)。
- 外部传入的枚举值必须检查边界。
- paint 路径不使用字符串查找、锁或堆分配。
- Controller 在 GUI 线程构造完整新 snapshot 后一次交换。
- 颜色变化只请求重绘；字体、尺寸变化才触发布局。
- 支持 Light、Dark、System、HighContrast。
- 系统主题使用 Qt 6.8 的公开 `QStyleHints` API。
- 第一阶段采用应用级主题；每窗口独立主题不是首个里程碑的必要条件。

### 10.3 样式和绘制

- `ZzFluentStyle` 基于 `QProxyStyle`，保留平台基础行为并覆盖 Fluent primitive、control 和 metric。
- 应用显式创建 `ZzThemeController` 和 `ZzFluentStyle`，不使用永久主题单例。
- Zz 控件优先继承对应 Qt 控件，例如 `ZzPushButton : QPushButton`。
- 禁止建立深层通用控件继承树；共享状态和 painter 使用私有组合对象。
- 禁止大型全局 QSS 作为主要主题机制。
- 禁止 QWidgetPrivate、QTabBarPrivate 等 Qt Private API。
- `paintEvent()` 中禁止读取文件、解析 SVG、构造大容器或创建临时 pixmap。
- painter 必须考虑 DPR、禁用态、焦点态、高对比度和布局方向。

### 10.4 动画

- 优先使用 Qt 已有统一动画时钟。
- 动画对象按需创建并复用，不在每次 hover 时重新分配。
- 隐藏、禁用或 reduced-motion 状态下停止非必要动画。
- 快速反向操作从当前进度平滑反向，不重置为初始状态。
- 页面切换动画必须可取消，回调使用 `QPointer`。
- 动画不得改变业务状态。

### 10.5 图标和资源缓存

缓存 key 至少包含：

```text
resource id + logical size + DPR + RGBA + theme revision
```

缓存必须设置容量或字节上限，并提供清理测试。任何图标、字体、图片和动图都必须记录来源、许可证、校验和与用途。

### 10.6 第一阶段控件顺序

```text
主题与绘制原语
  -> 文字、图标、按钮
  -> 输入框、复选框、单选框、开关
  -> ComboBox、Menu、ToolTip
  -> Dialog、Message、Progress
  -> Navigation、Tab、Breadcrumb
  -> List/Table/Tree delegate 和 style
  -> ZzFluentTitleBar
```

第二阶段再实现日历、复杂卡片、可撕标签页等重型控件。

### 10.7 标题栏责任

`ZzFluentTitleBar` 只负责布局、绘制、按钮状态和用户意图信号，不包含 QWK 或 ZzWindowKit 调用。

`ZzPureTools` 负责读取平台能力并生成 `ZzWindowChromeConfiguration`：

- Windows/Linux 默认使用 Fluent 自定义按钮。
- macOS 优先保留原生系统按钮。
- 不支持的效果提供明确 fallback，不隐藏失败。

### 10.8 可访问性与本地化

- 所有交互控件必须支持键盘导航、焦点可见、禁用态和屏幕阅读器名称。
- 自绘控件必须检查是否需要自定义 QAccessible interface。
- 文本不得写死在 painter 中。
- 布局必须支持动态文本、字体缩放和从右到左方向。
- 截图基准不能使用像素完全相等来掩盖不同平台字体差异，应采用区域和容差策略。

## 11. ZzPureTools 设计

### 11.1 AppCore 与 Widgets 外壳

`Zz::AppCore` 不依赖 Widgets，提供：

- `ZzModuleGraphBuilder`
- `ZzApplicationRuntime`
- `ZzApplicationModule`
- `ZzModuleDescriptor`
- `ZzModuleId`
- `ZzRouteId`
- 无 UI 的模块顺序和生命周期契约

`Zz::PureTools` 提供：

- `ZzApplicationBuilder`
- `ZzPureApplication`
- `ZzApplicationWindow`
- `ZzNavigationController`
- `ZzNavigationModel`
- `ZzPageRegistration`
- `ZzPageInstance`
- `ZzPageHost`

### 11.2 Composition root

`main.cpp` 是唯一了解全部具体模块依赖的 composition root。Widgets 层的 `ZzApplicationBuilder` 内部组合无 UI 的 `ZzModuleGraphBuilder`，因此 AppCore 不会接触 `ZzPageRegistration` 或 QWidget：

```cpp
int main(int argc, char *argv[])
{
    const auto bootstrapResult =
        ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrapResult) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzPureApplication application(argc, argv);

    ZzPureTools::ZzApplicationBuilder builder;
    builder.addModule(std::make_unique<ZzSettingsModule>(/* 显式依赖 */));
    builder.addModule(std::make_unique<ZzMainModule>(/* 显式依赖 */));
    builder.addPage(/* 展示层 factory */);

    auto runtimeResult = builder.build(application);
    if (!runtimeResult) {
        return EXIT_FAILURE;
    }

    return application.exec();
}
```

具体 API 可在实施计划中微调，但必须保留：启动前 bootstrap、显式构造依赖、build 后冻结、失败显式返回。

### 11.3 模块生命周期

```cpp
namespace ZzPureTools {

class ZzApplicationModule
{
public:
    virtual ~ZzApplicationModule() = default;

    [[nodiscard]] virtual ZzModuleDescriptor descriptor() const = 0;
    [[nodiscard]] virtual ZzCore::ZzResult<void> start() = 0;
    virtual void requestStop() noexcept = 0;
    virtual void stop() noexcept = 0;
};

} // namespace ZzPureTools
```

规则：

- 模块在 `main.cpp` 或专用 composition class 中构造。
- 业务依赖通过构造函数传入。
- `ZzModuleGraphBuilder` 不提供运行期通用 `getService<T>()`。
- descriptor 使用稳定 ID、版本和依赖 ID。
- build 阶段检测重复 ID、缺失依赖和环。
- 按拓扑顺序启动，按相反顺序停止。
- 启动失败时只回滚已成功启动的模块。
- 第一阶段不支持运行时卸载。

未来插件必须通过版本化、窄接口的 host contract 获取能力，不能直接读取内部 registry。

### 11.4 业务模块与 UI 贡献分离

- `ZzApplicationModule` 负责业务生命周期，不依赖 QWidget。
- 页面通过独立 `ZzPageRegistration` 注册。
- 页面 factory 位于 composition 层，可以同时构造 Presenter、ViewModel 和 View。
- View 仍然只接收展示层对象，不接收领域 service。
- 未来 Quick 使用独立视图贡献接口，不修改业务模块接口。

### 11.5 页面所有权

`ZzPageInstance` 统一管理一页的 View、ViewModel、Presenter 和任务句柄。销毁顺序必须明确：

```text
禁止新用户操作
 -> 请求取消页面任务
 -> 断开展示连接
 -> 销毁 View
 -> 销毁 Presenter
 -> 销毁 ViewModel
```

不得同时使用 QObject parent 和 `unique_ptr` 双重拥有同一对象。具体所有权方式由类固定，不能由每个页面自行决定。

页面策略：

- `Persistent`：首次创建后保留到窗口销毁。
- `WhileActive`：离开后按规则销毁。
- `Recreatable`：可由有界缓存回收并重新创建。

默认首次访问时延迟创建页面，禁止启动时同步构造所有页面。

### 11.6 导航

```text
ZzRouteId
  -> ZzNavigationModel 展示节点
  -> ZzNavigationController
  -> ZzPageRegistration
  -> ZzPageInstance
```

- 路由使用强类型 `ZzRouteId`，不使用数组 index 或 QObject 动态 property 建立身份。
- 导航模型与页面实例分离。
- 导航历史按窗口拥有并设置上限。
- 导航历史不复用业务撤销栈。
- 快速连续导航取消旧动画。
- 页面创建失败进入框架错误页，并清理半初始化对象。
- 页面 factory 和 route 注册在 Builder 冻结后不可修改。

### 11.7 多窗口

每个 `ZzApplicationWindow` 独立拥有：

```text
ZzWindowAgent
ZzFluentTitleBar
ZzNavigationController
ZzNavigationModel
ZzPageHost
窗口级导航历史
窗口级 QUndoGroup（可选）
```

只有线程池、日志和不可变应用配置可以跨窗口共享。导航、撤销、页面、动画和窗口代理不得跨窗口共享可变状态。

### 11.8 翻译

- 翻译资源由模块显式注册，不使用硬编码模块名列表。
- 安装 translator 前验证资源存在。
- 语言状态只在加载成功后更新。
- QWidget 通过标准 `LanguageChange` 更新静态文本。
- ViewModel 负责更新动态展示文本。

### 11.9 启动与关闭顺序

启动：

```text
prepare window attributes
 -> QApplication
 -> paths/settings
 -> ZzLog + Qt log bridge
 -> task executor/services
 -> theme/style
 -> application modules
 -> windows
```

关闭：

```text
停止接收用户命令
 -> 通知模块 requestStop
 -> 请求取消页面和后台任务
 -> 销毁窗口与展示对象
 -> 等待任务 deadline
 -> 按逆序 stop 模块
 -> 持久化设置
 -> 卸载 Qt log bridge
 -> flush/shutdown ZzLog
```

所有 QObject 必须在事件循环和所属线程仍有效时销毁。

## 12. CMake 与 Preset

### 12.1 根 CMakeLists.txt

根文件是唯一构建事实来源：

```cmake
cmake_minimum_required(VERSION 3.23)

project(ZzPureToolsPro
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Svg
    Concurrent
)

if(ZZ_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

add_subdirectory(ZzThirdParty/ZzLog)
add_subdirectory(ZzCore)
add_subdirectory(ZzWindowKit)
add_subdirectory(ZzFluentUI)
add_subdirectory(ZzPureTools)
```

实际实现中必须在引用 `ZZ_BUILD_TESTS` 前定义 option。代码片段表达依赖顺序，不替代完整 CMake 实现。

统一设置：

```text
CMAKE_CXX_STANDARD=20
CMAKE_CXX_STANDARD_REQUIRED=ON
CMAKE_CXX_EXTENSIONS=OFF
BUILD_SHARED_LIBS=ON（默认）
CMAKE_POSITION_INDEPENDENT_CODE=ON
CXX_VISIBILITY_PRESET=hidden
VISIBILITY_INLINES_HIDDEN=ON
```

不得由项目强制写入 `CMAKE_BUILD_TYPE`。不得把用户的全局 compiler flags 覆盖掉。警告、sanitizer 和 LTO 必须以 target 或项目 option 控制。

### 12.2 项目选项

```text
ZZ_BUILD_TESTS
ZZ_BUILD_EXAMPLES
ZZ_BUILD_BENCHMARKS
ZZ_ENABLE_SANITIZERS
ZZ_ENABLE_CLANG_TIDY
ZZ_WARNINGS_AS_ERRORS
ZZ_ENABLE_LTO
ZZ_BUILD_FLUENT_QUICK
```

ZzLog 的 shared/static 选择由顶层 `BUILD_SHARED_LIBS` 映射。QWindowKit 无论顶层模式如何都先构建为静态私有后端。

### 12.3 CMakePresets.json

提交的 Preset 不包含任何用户绝对路径。建议配置名称：

```text
linux-gcc-debug
linux-gcc-release
linux-static-release
linux-clang-release
linux-clang-asan
linux-clang-ubsan

windows-msvc2022-release
windows-msvc2022-static
windows-mingw-release
windows-mingw-static

macos-clang-release-arm64
macos-clang-release-x86_64
macos-clang-static-arm64
macos-clang-static-x86_64
```

每个名称同时提供 configure、build 和 test preset：

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

Preset 只负责：

- generator。
- `binaryDir=${sourceDir}/build/${presetName}`。
- Debug/Release、shared/static 和检查选项组合。
- 测试输出、超时和环境。

`CMakeUserPresets.json` 必须加入 `.gitignore`。仓库提供 `CMakeUserPresets.json.example`，说明如何设置本机 `QT_ROOT`、编译器和 SDK，但示例中不得填写开发者真实绝对路径。

`CMakePresets.json` 使用 schema version 4，与最低 CMake 3.23 对齐；当前阶段不使用要求更高 schema 的 workflow preset。需要完整工作流时由 configure/build/test preset 显式组合。

### 12.4 编译器规则

- GCC/Clang：至少 `-Wall -Wextra -Wpedantic`；一方代码在 CI 中开启 `-Werror`。
- MSVC：`/W4 /WX /permissive- /Zc:__cplusplus /utf-8`。
- MinGW：使用 GCC 严格警告集，并确保工具链来自对应 Qt SDK。
- MOC、RCC 生成代码和第三方代码不得继承一方代码的 `-Werror`。
- clang-tidy 启用 `clang-analyzer-*`、`bugprone-*`、`performance-*` 和筛选后的 `modernize-*`。

### 12.5 安装与消费

导出目标：

```text
Zz::Core
Zz::WindowKit
Zz::FluentFoundation
Zz::FluentUI
Zz::AppCore
Zz::PureTools
```

要求：

- 使用 `GNUInstallDirs`。
- Config Package 可重定位。
- 禁止把 Qt SDK 绝对路径写入 RPATH。
- shared 与 static 使用不同安装前缀。
- 每次安装测试都从全新 consumer build tree 执行 `find_package()`。
- installed consumer 不得依赖源码树或构建树路径。
- ZzWindowKit Config 校验 Qt major/minor。
- 发布包安装项目许可证、第三方许可证、notices 和 vendor manifest。

## 13. 平台支持策略

| 平台 | 基线 | 编译器 | 第一阶段验证等级 |
|---|---|---|---|
| Linux | Ubuntu 22.04 兼容基线；X11/Wayland；GNOME/KDE | GCC、Clang | 构建、测试、性能与真实运行 |
| Windows | Windows 10 22H2、Windows 11 | MSVC 2022 x64 | 编译、静态检查、安装消费 |
| Windows | Windows 10 22H2、Windows 11 | Qt SDK 官方 MinGW-w64 x64 | 编译、静态检查、安装消费 |
| macOS | macOS 12+ | Apple Clang arm64/x86_64 | 编译、静态检查、安装消费 |

Windows 的 MSVC 与 MinGW 产物完全分离：

- 不互相链接。
- 不共享 C++ ABI 假设。
- 所有一方库、ZzLog 和 qwindowkit 必须使用同一工具链重建。
- 预编译发布包只承诺 Qt SDK 官方 MinGW 组合。

Windows/macOS 的“静态检查”至少包含：

- 对应平台原生编译器完整编译所有条件分支。
- shared/static 构建。
- 严格警告和 clang-tidy 可用检查。
- 公共头独立编译。
- install consumer 编译与链接。
- 平台源和 framework/system library 链接检查。

它不等于真实窗口交互验证。Windows 的 Snap Layout、DPI、多屏和系统材质，以及 macOS 的原生按钮、全屏、Retina 和 blur，必须在真机清单中单独确认。

Linux 必须分别验证：

- X11 KDE。
- X11 GNOME。
- Wayland KDE。
- Wayland GNOME。
- forced Qt fallback。

环境无法覆盖全部桌面时，报告必须列出实际覆盖项，不得笼统写“Linux 已验证”。

## 14. 性能预算

### 14.1 统一测量条件

- 使用 Release 构建；profiling 可额外使用 RelWithDebInfo。
- 记录 CPU、内存、GPU、Qt、编译器、窗口系统、DPR、显示刷新率和构建参数。
- 预热与正式迭代次数固定。
- 报告 P50、P95 和最大值，不只记录最好一次。
- CI 的相对回归与参考机器的绝对指标分开报告。

### 14.2 验收指标

| 指标 | 固定场景 | 目标 |
|---|---|---|
| 暖启动 | 进程入口到首窗完成首帧且可接收输入 | 不超过 300 ms |
| 空闲 CPU | 无动画、任务和输入，连续 30 秒 | 平均低于 0.5% |
| 动画 | 常规 hover、切换和导航 | 60 FPS，帧耗时 P95 不超过 16.7 ms |
| 主题切换 | 500 个可见基础控件 | P95 不超过 50 ms |
| 大模型滚动 | 10 万行 model，约 40 行可见 | 绘制成本不得随总行数线性增长，P95 不超过 16.7 ms |
| 窗口生命周期 | 连续创建销毁 100 个窗口 | 无泄漏、UAF、残留 native filter |
| 内存回归 | 相同场景和工具链 | 未解释增长不得超过已确认基线 10% |
| 缓存 | 图标、页面和渲染缓存 | 必须有容量或字节上限 |

启动计时必须同时保留外部进程计时和内部阶段 marker，以区分动态加载、Qt 初始化、模块启动、页面创建和首帧绘制。

性能优化顺序：先测量，定位热点，建立回归测试，再修改实现。禁止用复杂缓存掩盖所有权或算法问题。

## 15. 测试与质量门禁

### 15.1 测试层次

| 标签 | 内容 |
|---|---|
| `unit` | Result、状态机、token、模块排序、错误路径 |
| `component` | WindowKit 假后端、主题切换、页面生命周期 |
| `architecture` | 依赖、命名、注释、Private/QWK 泄漏 |
| `headers` | 每个安装公共头以 C++20 独立编译 |
| `install` | shared/static 安装后独立消费 |
| `sanitizer` | Linux ASan、UBSan；可行时单独运行 TSan |
| `benchmark` | 启动、主题、paint、滚动、内存和缓存 |
| `platform` | Linux 原生窗口；Windows/macOS 编译门禁 |

测试使用 CTest 统一注册和 label。纯 Qt 组件优先使用 Qt Test，避免仅为断言框架增加大型依赖。

### 15.2 架构自动检查

必须自动检查：

- 自定义类、结构体、枚举和 Concept 的 `Zz` 前缀。
- 主类型与文件名一致。
- 禁止链式 namespace 声明。
- 公开 API 的简体中文 Doxygen。
- ZzCore 无 QtGui/Widgets include 或 link。
- 非 ZzWindowKit private 文件无 QWK include。
- 一方代码无 Qt Private include。
- UI 目录无被禁止的业务/存储依赖。
- 安装公共头无构建树相对路径。
- installed Config 无本机绝对 Qt 路径。

简单文本规则可能误报时，应使用编译数据库或 Clang AST 补充，而不是删除检查。

### 15.3 WindowKit 专项测试

- 空 host。
- 非顶层 QWidget。
- 错误线程。
- 重复 attach。
- attach 前调用。
- title bar 更换后完整重绑。
- 按钮早于 title bar 销毁。
- host 销毁后调用。
- close/show 和 WinId 重建。
- 连续创建销毁 100 次。
- installed public headers 不含 QWK/Qt Private。
- shared consumer 不需要 QWK 动态库。
- static consumer 不需要 QWK package 或头。

### 15.4 视觉和交互测试

- Linux 自动截图覆盖 Light、Dark、HighContrast 和常用 DPR。
- 截图比较使用容差和稳定区域，避免字体栅格化造成无意义失败。
- 键盘、Tab 顺序、焦点环、Enter/Space/Escape 和菜单行为必须有自动测试。
- Windows/macOS 平台特有行为保留人工验收清单。

## 16. 分阶段实施

### 阶段 0：第三方与文档基线

交付：

- qwindowkit/ZzLog vendor 清单。
- 许可证和 notice 清单。
- qwindowkit commit 与 `windeployqt` 衍生代码核查项。
- 编码规范和平台支持文档框架。

退出条件：所有无法确认的信息被标为明确发布阻塞项，而不是默认为合规。

### 阶段 1：工程骨架

交付：

- 根 `CMakeLists.txt`。
- `CMakePresets.json` 与用户 Preset 示例。
- 四组件 target 骨架、导出宏、安装 Config。
- warnings、sanitizer、架构检查和 install consumer 基础设施。

退出条件：Linux shared/static 均能 configure、build、install 和独立 consume。

### 阶段 2：ZzLog

交付：

- C++20 API 与命名迁移。
- 中文 Doxygen。
- 并发、overflow、flush、生命周期测试。

退出条件：shared/static smoke 和并发测试通过，公共 API 不泄露 spdlog。

### 阶段 3：ZzCore

交付：

- ZzError/ZzResult。
- 任务、取消、路径、设置和 Qt 日志桥。
- 单元与 sanitizer 测试。

退出条件：无 QtGui/Widgets 依赖，错误和停止路径全部通过测试。

### 阶段 4：ZzWindowKit

交付：

- facade、状态机和假后端。
- QWK private backend。
- shared/static 安装封装。
- Linux X11/Wayland/fallback smoke。

退出条件：Linux 无边框示例可运行，生命周期测试和 install consumer 通过。

### 阶段 5：FluentFoundation

交付：

- token、snapshot、controller、style 基础。
- 图标缓存和资源许可清单。
- 主题切换 benchmark。

退出条件：Light/Dark/System/HighContrast 基础路径正确，500 控件指标达标。

### 阶段 6：基础控件

交付：

- 文本、图标、按钮、输入、选择、菜单、反馈、导航、Model/View style 和标题栏。
- 键盘、DPI、无障碍、截图与性能测试。

退出条件：第一批控件在 Linux 形成可使用、可测试的完整控件面。

### 阶段 7：ZzAppCore

交付：

- ZzModuleGraphBuilder、模块 descriptor、依赖排序、启动回滚和停止顺序。
- route、page registration 和页面生命周期。

退出条件：模块失败回滚、多窗口隔离和页面销毁测试通过。

### 阶段 8：ZzPureTools

交付：

- ZzPureApplication、ZzApplicationWindow、PageHost、导航外壳。
- WindowKit 与 FluentTitleBar 组合。
- 无业务逻辑的示例应用。

退出条件：Linux 完整工作流和已定义性能门禁通过。这是首个可用里程碑。

### 阶段 9：跨平台编译门禁

交付：

- MSVC shared/static。
- Qt 官方 MinGW shared/static。
- macOS arm64/x86_64 shared/static。
- 各平台 install consumer 和静态检查报告。

退出条件：Windows/macOS 无编译阻塞；报告明确区分自动验证与待人工验证项。

### 阶段 10：扩展控件

日历、复杂卡片、可撕标签页等按独立批次实现，每批都必须满足相同代码、测试、性能与文档门禁，不阻塞基础版本发布。

## 17. 文档交付

最终文档集合：

```text
docs/superpowers/specs/2026-08-02-zzpuretoolspro-architecture-design.md
docs/development/CODING_STANDARD_ZH.md
docs/development/BUILDING_ZH.md
docs/development/PLATFORM_SUPPORT_ZH.md
docs/performance/PERFORMANCE_BASELINE_ZH.md
docs/third-party/THIRD_PARTY_NOTICES.md
docs/third-party/qwindowkit-vendor.json
docs/adr/
```

本文是总体设计。后续必须按阶段生成独立实施计划，不能把全部工作塞进一个巨型计划。

## 18. Git 提交规范

所有 Git 操作只在内层新仓库执行。一个提交只包含一个逻辑改动，测试与对应实现放在同一提交或紧邻提交中。

提交格式：

```text
文档：确定 ZzPureToolsPro 重构架构

记录组件边界、公共接口、线程模型和性能目标。
明确 Linux、Windows、macOS 的构建与验证范围。
补充 qwindowkit 隔离策略和分阶段实施条件。
```

后续摘要示例：

```text
构建：建立跨平台 CMake 工程基线
日志：升级 ZzLog 的 C++20 公共接口
核心：实现错误与结果模型
窗口：建立 QWindowKit 私有适配层
主题：实现 Fluent 设计令牌系统
控件：实现第一批基础交互组件
框架：实现模块生命周期与页面导航
测试：建立跨平台安装消费门禁
```

要求：

- 第一行使用中文简述。
- 第一行后空一行。
- 正文使用中文详细说明行为、测试和平台影响。
- 提交前运行对应阶段验证。
- 禁止提交 build、cache、安装产物、临时报告和本机绝对路径。

## 19. 实施者验收清单

任何实现任务结束前必须逐项确认：

- [ ] 改动位于正确组件，没有反向依赖。
- [ ] 新自定义类型使用 `Zz` 前缀，文件名匹配主类型。
- [ ] 没有链式 namespace 声明。
- [ ] 公开 API 有简体中文 Doxygen、所有权和线程说明。
- [ ] 导出有状态类按规则使用 PIMPL。
- [ ] UI 未访问领域、存储或网络对象。
- [ ] QWK 只存在于 ZzWindowKit private 实现。
- [ ] 一方代码没有 Qt Private include。
- [ ] 预期失败使用 Result，异步任务可取消且不会回调已销毁对象。
- [ ] 新缓存有明确上限和完整 key。
- [ ] 新动画不在每次状态变化时重复分配对象。
- [ ] 新功能先有失败测试，再有实现。
- [ ] Linux 对应测试和必要性能检查通过。
- [ ] shared/static 公共头与 install consumer 通过。
- [ ] Windows/MSVC、Windows/MinGW、macOS 条件代码完成相应静态门禁。
- [ ] 第三方资源和代码有来源、许可证和校验信息。
- [ ] 文档同步更新，提交信息符合中文格式。

## 20. 后续计划拆分

用户审核本文后，按以下顺序分别编写可执行实施计划：

1. 仓库基线、CMake 与工程规范。
2. ZzLog C++20 合规升级。
3. ZzCore 基础设施。
4. ZzWindowKit/qwindowkit 适配。
5. ZzFluentFoundation 主题内核。
6. 第一批基础控件。
7. ZzAppCore 与 ZzPureTools 应用框架。
8. 性能、跨平台与发布门禁。

每份实施计划必须列出精确文件路径、测试名称、先失败后通过的命令、预期结果和提交边界。
