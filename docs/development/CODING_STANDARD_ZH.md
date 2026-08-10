# ZzPureToolsPro 编码规范

本文档是新增和修改一方代码时的强制规范。架构设计文档提供背景和完整设计，本文件给出代码评审、自动检查和提交时应直接执行的规则。

## C++ 与 Qt 基线

- 项目只支持 C++20，必须关闭编译器语言扩展，不得为旧编译器增加降低语言标准的兼容分支。
- Qt 最低版本为 6.8。优先使用 Qt 6.8 以后公开且跨平台的 API，不以 Qt Private API 换取短期便利。
- 可使用 Concept、ranges、`std::span` 和 `std::source_location`；跨平台任务取消统一使用 `ZzStopSource` 与 `ZzStopToken`，不得在公共接口直接暴露当前 Apple libc++ 尚未实现的 `std::stop_token`。
- 协程只能进入经过设计的异步边界。开始、挂起、恢复线程、取消、接收者销毁和异常路径必须有测试；不得在 paint、事件分发或简单信号链中只为语法形式引入协程。
- paint、布局、模型遍历和高频事件属于热路径。热路径禁止不必要的堆分配、字符串查找、临时容器、锁和层层虚调用。
- 关键返回值使用 `[[nodiscard]]`；能够证明不抛异常的析构、移动和只读查询使用 `noexcept`。

## 类型、文件与命名空间

- 除 `main.cpp` 外，所有一方自定义类、结构、枚举类和 Concept 都以 `Zz` 开头。
- 每个公开头只有一个主要公开类型，文件名必须与该类型名逐字一致。例如 `ZzWindowAgent` 位于 `ZzWindowAgent.h` 和 `ZzWindowAgent.cpp`。
- 私有实现类命名为公开类型加 `Private`，文件名与类名一致。内部小型辅助类型可与所属 private 实现同文件，但不得形成第二个公开接口。
- 命名空间按功能使用 `ZzCore`、`ZzWindowKit`、`ZzFluentUI`、`ZzAppCore`、`ZzPureTools` 等单一命名空间。需要层级时使用传统嵌套形式。
- 禁止 `namespace a::b` 链式声明。允许的形式如下：

```cpp
namespace ZzAppCore {
namespace Network {

class ZzRequest final
{
};

} // namespace Network
} // namespace ZzAppCore
```

- 公共接口不得暴露第三方类型、平台句柄、Qt Private 类型或 private 头路径。

## 简体中文 Doxygen

公开类、公开方法、公开信号、复杂算法和重要状态机必须使用简体中文 Doxygen。注释至少说明责任；有参数、返回值、所有权、线程或状态前提时必须分别写明。

```cpp
/**
 * @brief 将窗口绑定到无边框后端。
 * @param window 由调用方持有且属于 GUI 线程的窗口，不能为空。
 * @return 成功时为空值结果，失败时包含可诊断错误。
 */
[[nodiscard]] ZzCore::ZzResult<void> attach(QWidget *window);
```

注释必须解释契约和原因，不得机械复述函数名。简单私有 getter、显然的 override 和局部变量不要求重复注释。

## PIMPL 与四文件结构

以下公开类型采用四文件结构和 PIMPL：

- 导出的有状态 QObject 或 QWidget。
- 包含平台 API、第三方实现或需要稳定 ABI 状态的公开类。
- 状态复杂、公共头依赖面需要长期稳定的控制器。

标准文件组为 `ZzClassName.h`、`ZzClassName.cpp`、`ZzClassNamePrivate.h`、`ZzClassNamePrivate.cpp`。公开类以 `std::unique_ptr<ZzClassNamePrivate>` 唯一持有 private 对象，析构函数在 `.cpp` 中定义。

PIMPL 通常增加一次对象分配和一次间接访问，对窗口、页面和普通控件的影响很小。不得把 PIMPL 放入逐像素、逐单元格或紧密数值循环中的短生命周期对象。纯抽象接口、小型不可变值类型、枚举、Concept、模板和不导出的私有辅助类不强制四文件。

## QObject 所有权与生命周期

- 一个 QObject 只能有一种所有权：QObject parent、`std::unique_ptr` 或明确的外部所有者三者选一。
- 禁止同一对象同时由 QObject parent 和智能指针拥有。借用 QObject 使用裸指针或 `QPointer`，并在接口和注释中说明非拥有关系。
- 公开 QObject/QWidget 默认使用 `Q_DISABLE_COPY_MOVE`。
- GUI 对象只在 GUI 线程创建、访问和销毁。后台任务不得捕获 QWidget、ViewModel 裸指针或可变 UI 状态。
- 异步完成回调必须绑定 QObject context；context 销毁后回调自动失效。页面销毁前取消关联任务。
- 退出顺序固定为停止接收操作、关闭窗口和页面、取消任务、停止模块、关闭日志。不得依赖进程退出回收仍有事件过滤器或 native hook 的对象。

## Result 与异常边界

- 可预期失败使用 `ZzResult<T>` 或 `ZzResult<void>`，不得以异常控制普通业务分支。
- 第三方库、线程任务、插件和模块边界捕获异常并转换为 `ZzError`。Qt 信号槽、事件过滤器和 C ABI 边界禁止传播异常。
- `ZzError` 保存稳定错误码、技术描述和上下文，不保存最终用户文案。Presenter 将错误映射为可本地化展示文本。
- 程序员错误在 Debug 中使用断言；Release 中仍要进入明确失败状态或返回可诊断错误。
- 调用 `value()` 前必须证明结果有值。跨模块调用优先显式传播错误，不得静默使用默认值掩盖失败。

## UI 与业务分离

允许的数据方向是：Domain 到 UseCase，再到 Presenter、只读 ViewModel 或 `QAbstractItemModel`，最后到 QWidget。用户操作由 QWidget 发出 intent 或 command，经 Presenter 返回 UseCase。

UI 可以读取展示模型、更新焦点和视觉状态、驱动纯展示动画，也可以发出点击、提交、选择和导航意图。UI 不得：

- 访问 repository、数据库、网络 client、文件存储或领域实体。
- 在点击槽中执行完整业务流程，或从全局容器查找业务 service。
- 直接读写 `QSettings`；设置通过可注入接口进入 Presenter 或应用服务。
- 在 worker 线程修改 QWidget 或 UI model。
- 用动画回调改变业务真值。

展示模型固定属于 GUI 线程。增量数据使用 `beginInsertRows()`、`beginRemoveRows()` 和 `dataChanged()`，不得用频繁 reset 掩盖模型设计问题。

## QWindowKit 与 Qt Private 边界

- `ZzThirdParty/qwindowkit` 是原样 vendored 后端，只有 `ZzWindowKit/src/private` 可以包含 QWK 头或链接 QWindowKit target。
- 所有一方代码禁止包含 Qt Private 头。vendored qwindowkit 自身的 Qt Private 使用不得扩散到适配层、公共头或安装接口。
- `ZzWindowKit` 公共 API 只表达项目自己的强类型能力和结果，不返回 QWK 对象、枚举、字符串命令或平台句柄。
- qwindowkit 上游升级通过独立依赖维护任务完成；业务或 UI 修改不得顺手改动 vendored 源码。
- 安装后的公开头和 CMake Config 不得要求消费者查找 QWindowKit 或 Qt Private 包。

## 缓存、绘制与动画

- 主题使用不可变 snapshot。控件读取颜色和尺寸应为 O(1)，主题切换先构造完整新状态再一次交换。
- 图标缓存 key 至少包含资源 ID、逻辑尺寸、DPR、RGBA 和主题 revision。缓存必须有条目或字节上限，并有清理测试。
- `paintEvent()` 中禁止文件 IO、SVG 解析、大容器构造和临时 pixmap 创建。DPR、高对比度、禁用态、焦点态和布局方向必须进入绘制契约。
- 动画对象按需创建并复用，不在每次 hover 时重新分配。隐藏、禁用或 reduced-motion 状态停止非必要动画。
- 快速反向操作从当前进度平滑反向。页面切换动画可取消，回调使用 `QPointer`。
- 优化必须由 benchmark、采样器或对象计数证明；不得以删除所有抽象作为性能方案。

## 测试与变更范围

- 修复先增加能够稳定复现的失败测试，再实现最小修复。窄改动运行定向测试，共享行为和跨模块契约运行更广矩阵。
- 新公开头必须进入逐头编译消费；新增 target 必须进入架构和安装重定位检查。
- 平台交互不能用另一平台结果代替。静态检查、自动原生 runner 和人工真机证据分别记录。
- 影响已有截图场景的视觉变更，必须在同一逻辑批次更新对应 Linux 参考基线，关闭更新模式重新比较，并人工检查 Light、Dark、HighContrast、常用 DPR 和 RTL；缺少任一步骤时变更不得视为完成或进入发布候选。
- 禁止提交 `build/`、`install/`、缓存、临时性能报告、本机绝对路径或未经审核的发布证据。

## Git 提交

每个逻辑任务完成并通过对应验证后立即提交。提交标题使用中文简述，正文使用多段中文详细说明行为、边界和验证依据，例如：

```text
窗口：封装无边框窗口适配层

新增 ZzWindowAgent 公共契约，并把 QWindowKit 类型限制在 private 后端。

补充窗口生命周期和安装消费测试，验证重复创建销毁后无适配层对象残留。
```

不得把无关重构、格式化和功能修改塞入同一提交。不得在提交正文中声称未执行的平台或真机结果。
