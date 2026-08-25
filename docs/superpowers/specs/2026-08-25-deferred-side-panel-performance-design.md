# 延迟侧面板与 Example 首帧性能设计

**状态：** 方案一已确认，作为
`docs/superpowers/plans/2026-08-22-ide-workbench-product-expansion.md` 任务 15 的性能回归修复。

**目标：** 在不放宽既有 10% 相对性能阈值、不把工作台协调逻辑下沉到 Example 的
前提下，把侧面板内容移出 `ZzPureToolsExample` 首帧构建路径，同时保持 Activity
入口、布局恢复、跨侧迁移和面板所有权合同。

## 1. 已确认根因

参考机三轮复采稳定观察到：

- 完整 Example 首帧约 107 至 121 ms，idle RSS 约 78.9 MiB；
- 跳过四个 Side 内容后，首帧约 72 至 77 ms，idle RSS 约 67.8 MiB；
- Side 区域贡献约 84 个 QObject、58 个 QWidget 和 11 MiB idle RSS；
- `ZzWorkspaceShell`、action/model、Bottom 和 tab 不是主要回归来源；
- 30 秒 idle 期间对象数稳定，没有泄漏证据。

当前并不存在“已注册但隐藏”的 Side 内容。`registerSidePanel()` 会立即接管 QWidget、
加入 `ZzPanelStack`、设为 active 并展开侧栏。若首屏仍要求四个 Side 面板全部逻辑
可见，任何真实延迟方案都必须在首帧前实例化四个面板，无法改善 startup 或 RSS。

因此本修复明确调整 Example 初始产品状态：四个 Activity 入口立即存在，但对应内容
初始 inactive，左右 SidePane 初始折叠；用户首次激活入口时才创建内容。截图测试仍
独立覆盖双侧多面板的完整展开视觉。

## 2. 方案选择

采用 `ZzWorkspaceShell` 公共 factory 注册 API。Shell 已经拥有 PanelId、Activity、
SidePane、布局 envelope 和跨侧事务，只有这一层可以在不创建 QWidget 的情况下保存
完整逻辑身份，并在显示前原子完成内容创建。

不采用以下方案：

- 不在 `ZzPanelStack` 增加 factory。该组件只以 QWidget 指针管理内容，不知道 PanelId、
  Activity 和布局事务；引入稳定逻辑身份会把同一协调状态复制到两个模块；
- 不使用通用 showEvent 延迟 QWidget。factory 失败发生在 Shell 已把外层宿主设为
  visible 之后，`showPanel()` 无法返回正确错误，也无法保证可见状态不变；
- 不在 Example 私有延迟注册。未注册内容在首次创建前没有 Activity 入口、稳定 PanelId
  和布局身份，且晚注册会破坏保存顺序与跨侧迁移；
- 不刷新历史 metric 或提高阈值。当前差异由稳定功能成本造成，不是环境噪声。

## 3. 公开合同

在 `ZzWorkspaceShell.h` 增加与现有 `ZzPageFactory` 风格一致的工厂类型：

```cpp
using ZzWorkspacePanelFactory =
    std::function<ZzCore::ZzResult<std::unique_ptr<QWidget>>()>
;
```

新增公开方法：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> registerSidePanelFactory(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    ZzWorkspacePanelFactory factory);
```

该方法只注册逻辑 Side 面板，固定语义为 lazy 且初始 inactive/hidden。需要立即创建并
显示内容的调用方继续使用原 `registerSidePanel(..., QWidget *)`；旧方法的源码、二进制
符号、所有权和默认可见语义全部保持不变，不新增 bool 或模式参数混淆两种合同。

新增类型和公开方法使用简体中文 Doxygen。`ZzWorkspaceShell` 继续 PIMPL，公开对象大小
与 vtable 不变。新签名与既有 `ZzPageFactory` 一样受 C++ 标准库 ABI 约束，不宣称在
MSVC、MinGW 和不同 libstdc++ ABI 之间传递 factory 对象。

## 4. 记录与状态机

`ZzPanelRecord` 增加 factory 和 materialization 状态：

```text
Pending -> Materializing -> Ready
   ^            |
   `-- failure -'
```

- `Pending`：逻辑注册完成，Activity 行存在，content 为空，factory 尚未成功；
- `Materializing`：阻止 factory 内同步调用同一面板的 show/take/move/restore；
- `Ready`：content 已被对应 SidePane 接管，factory 不再调用；
- factory 失败后回到 `Pending`，允许下一次显式显示或 Activity 激活重试。

不持久保存 Failed 状态。项目现有 `ZzPageHost` 已采用“创建失败不改变当前状态，后续可
重试”的合同，侧面板沿用同一行为。

逻辑 Side 顺序来自 Activity 行和 `ZzPanelRecord::activityArea`；物理 PanelStack 顺序
只包含已经 Ready 的 QWidget 子集。所有审计必须分别验证“逻辑全量”和“物理子集”，
不得继续假设注册 Side 数量等于两个 PanelStack 内容总数。

## 5. 创建事务与错误处理

`showPanel(id, true)` 和 Activity 激活在改变可见/current/active 状态前调用内部
`materializeSidePanel()`。调用顺序固定为：

1. 校验宿主、记录、目标 SidePane、Activity model 和 GUI 线程；
2. 标记 `Materializing`，捕获 PanelId、registration generation 和对象守卫；
3. 在 try/catch 中调用 factory；
4. 校验成功值为非空、无父对象、当前 GUI 线程中的 QWidget；
5. 按同侧逻辑顺序计算其在 Ready 子序列中的物理插入位置；
6. 使用现有 PanelStack 所有权观察和回滚机制接管内容；
7. 提交 `Ready`，再执行原 show/current/active 更新。

factory 返回错误、空指针、带父对象内容、错误线程内容，或抛出标准/未知异常时：

- `showPanel()` 返回稳定 `ZzError`；
- 记录回到 `Pending`，factory 保留以便重试；
- Activity 行、Area、顺序、badge、可见集合、尺寸和 current/active 状态不变；
- Shell 不接管非法内容；无父且线程合法的非法返回在局部作用域销毁；
- 带父对象的非法返回释放给原 Qt 父对象，原父对象和第三方所有权不被 Shell 修改；
- 异常不得穿过 Qt 事件循环。

factory 自身对外部系统产生的副作用无法由 Shell 回滚，公开注释只保证 Shell UI、注册表
和 QWidget 所有权事务。

Activity 点击路径当前没有可返回值的调用者。失败时入口保持 inactive，Shell 不新增
meta-object 信号或平台依赖；需要处理具体错误的业务命令应显式调用 `showPanel()` 并读取
返回的 `ZzError`。

## 6. take、迁移与布局

### 6.1 takePanel

`takePanel()` 必须维持“成功结果归还一个无父 QWidget”合同。对 Pending 面板，它调用
factory 但不显示内容，成功后移除 Activity 行和记录并直接归还无父内容。factory 失败
时注册状态完全不变并返回错误，禁止 `success(nullptr)`。

### 6.2 跨侧迁移

Pending 面板迁移只事务更新 `activityArea` 和 Activity 逻辑顺序，不调用 factory，也不
访问 PanelStack。Ready 面板继续走现有 QWidget 跨侧移动事务。Pending 面板以后首次
实例化时，根据最新逻辑 Area 和顺序插入对应物理 Ready 子序列。

### 6.3 保存与恢复

`saveLayout()` 不触发 factory：

- `sideEntries` 保存全部逻辑 Side PanelId、Area 和顺序；
- left/right visible ID、sizes 和 current 只来自 Ready 且可见的物理内容；
- Pending 面板自然保存为已注册但不可见。

`restoreLayout()` 只实例化目标布局要求 visible/current 的 Pending PanelId。若恢复中第 N
个 factory 或后续提交失败，反向移除并销毁本次新建内容、恢复 factory 与 Pending 状态，
再恢复原 visible/current/active/order/sizes。旧布局若保存了四个 Side 面板均可见，恢复
时会正确创建四个内容；不得为了性能篡改用户已保存的布局。

v1/v2 magic、版本、摘要、1 MiB 上限和 Qt Dock state version 保持不变，不新增序列化
字段。Pending 是当前运行时实例化状态，不进入持久格式。

## 7. Example 接入

Sessions、Files、Properties 和 Tasks 均通过 `registerSidePanelFactory()` 注册。初始化完成后：

- 四个 Activity 行立即存在且顺序不变；
- 两个 Activity Bar 可见，左右 SidePane 折叠；
- 四个 factory 调用数均为零；
- 首次点击任一入口只创建该面板；
- 同侧第二个入口激活后仍可展示 Stacked 双面板；
- 后续显隐、迁移和布局恢复复用同一 Ready 内容，不再次调用 factory。

Example 只提供 factory 和初始产品状态，不创建 QSplitter、不直接修改 Activity model、
不编码布局 envelope 或跨组件回滚算法。现有 Bottom 三个工具保持 eager；消融证据表明
它们不是主要回归来源，本任务不扩大到 Bottom/Dock factory。

## 8. 测试合同

严格先红后绿，一项行为一轮。`ZzWorkspaceShellTest` 至少覆盖：

1. 旧 `registerSidePanel(QWidget *)` 继续立即创建、显示和激活；
2. factory 注册发布 Activity 行，但不调用 factory、不添加物理 Stack 内容；
3. `showPanel(true)` 和 Activity 单击分别只在首次成功时创建一次；
4. factory 错误、空指针、有父对象、错误线程和异常保持注册、Pane、Activity 与所有权
   状态不变，并可在下一次调用成功；
5. factory 同步重入同一 ID 被拒绝，不发生二次创建；
6. `takePanel(Pending)` 创建但不显示并返回无父内容，失败时保留注册；
7. save 不实例化 Pending，并 round trip 逻辑 Area 与顺序；
8. restore 只实例化保存为 visible/current 的 ID；
9. restore 中 factory 失败把本次全部新实例回滚为 Pending；
10. Pending 跨左右迁移不调用 factory，首次创建后物理顺序正确；
11. Pending/Ready 混合 move、take、save/restore 的逻辑全量与物理子集审计一致；
12. 实例化前 `setPanelBadge()` 有效。

增强 `ZzExampleWorkspaceSmokeTest`：

- 首建仍有四个 Activity 行，四个 Side factory 调用数为零；
- 单击 Sessions/Files 后左侧两个面板同时可见，每个 factory 只调用一次；
- Properties/Tasks 同样按需创建；
- 标签分屏、Bottom、CommandBar、跨侧迁移和布局 round trip 继续通过；
- 原“首建 sessions+files 已同时 visible”断言由按需激活后的同等断言取代。

公共头、安装消费和 ArchitectureAudit 必须覆盖新 API，shared/static 均可只通过安装后
公开头注册一个最小延迟 Side 面板。

## 9. 性能验收

实现后先运行三轮 Example startup/idle 复采，再运行统一 Linux 门禁：

- `example-startup` P95/max 相对历史 reference 不超过既有 gate；
- `example-idle` 起止 RSS 和增长率相对历史 reference 不超过既有 gate；
- 首帧前四个 Side factory 调用数均为零；
- 四个 Side 内容不计入首帧 QObject/QWidget；
- 不修改历史 reference metric，不提高 10% 相对阈值。

历史 RSS 终值 64,475,136 bytes 的 10% 上限约为 70,922,649 bytes；跳过四个 Side 的
诊断 idle RSS 约 67.8 MiB，存在约 3 MiB 余量。历史启动上限与消融结果距离更小，且
诊断首帧口径不完全等同 reporter，因此这里只记录通过可能，不预先宣称门禁成功。

若仍失败，继续以阶段计时、对象/RSS 和调度证据定位，不得直接覆盖基线或放宽阈值。

## 10. 平台边界

Linux 使用 Qt 6.11.1 动态运行和性能验证。Windows MSVC、Windows MinGW 与 macOS
只进行公共 C++20/Qt API、CMake 源清单、导出和安装消费静态检查，仍记录为真机运行
待验证。实现不使用 Qt Private API、平台分支、stylesheet、timer 或 animation。
