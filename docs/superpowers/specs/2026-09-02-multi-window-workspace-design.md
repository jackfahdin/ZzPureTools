# 多窗口工作区设计规格

- **状态**：已获需求方确认，等待书面规格审查
- **日期**：2026-09-02
- **适用项目**：ZzPureToolsFrame
- **目标使用方**：ZzClawTerm 以及其他需要 IDE/终端式多窗口工作区的 Qt 应用
- **实现前提**：Qt 6.8+、C++20、Linux 首先验证，Windows（MSVC/MinGW）和 macOS 做静态兼容检查

## 1. 目标与边界

本规格为 `ZzSplitWorkspace` 从“单工作区组件”扩展为“同进程多顶层窗口工作区”的公共契约。目标是让应用能够：

1. 将标签拖出窗口或执行撕出命令，创建独立顶层窗口；
2. 在多个工作区之间拖放或程序化转移同一个页面 widget；
3. 关闭撕出窗口时按策略回收页面，而不是静默销毁；
4. 保存并恢复多个窗口的几何、分屏树、活动页和页面归属；
5. 让窗口级配置（标题、图标、关闭策略、置顶、尺寸）可独立设置，并支持可选的字段级继承。

本批次不实现跨进程拖放、网络同步、窗口装饰主题重做，也不介入页面内部的二次分屏。`ZzWindowKit` 仍只负责无边框窗口代理与标题栏；业务页面和关闭确认由应用层负责。

## 2. 设计原则

### 2.1 所有权

- `ZzSplitWorkspace` 拥有其组和标签容器，但不拥有页面业务生命周期以外的应用模型。
- 页面 widget 在工作区之间迁移时保持同一实例，不重新创建终端会话、文档模型或业务对象。
- `ZzPureApplication` 继续独占全部 `ZzApplicationWindow`；协调器只登记窗口 id、策略和来源关系，避免形成第二套窗口所有权。
- `ZzWorkspaceTransferRegistryPrivate` 只在转移事务期间保存临时记录，永远不拥有页面。

### 2.2 线程与同步

- 所有公共 API 只能在页面所属 GUI 线程调用；跨线程调用返回 `InvalidArgument` 并携带 `wrong_thread` 原因，不会排队隐式执行。
- 转移、撕出、回收和恢复均为同步事务：成功才发出提交信号，失败恢复入口状态。
- 任何 Qt 信号槽回调都不得在事务中销毁来源/目标工作区；检测到对象失效即回滚或返回结构化错误。

### 2.3 兼容性

- 现有 `ZzSplitWorkspace`、`ZzTabWidget` 和 `ZzWorkspaceShell` 公共签名保持不变；新接口以追加方式提供。
- 现有 `ZZSW` schema v1 单工作区格式保持可读；多窗口使用新 schema v2。
- 每个工作区独立执行现有 64 组、16 层上限，全进程不额外设置组数上限。

## 3. 公共对象与职责

### 3.1 `ZzWorkspacePageId`

`ZzFluentUI` 中新增轻量、不可变的页面身份值。它使用 UUID 字节表示，创建后在当前进程内保持稳定；页面被迁移时身份不变。该 id 只用于拖放、审计和窗口恢复关联，不替代应用层的业务 id。

```cpp
class ZZ_FLUENT_UI_EXPORT ZzWorkspacePageId final
{
public:
    static ZzWorkspacePageId create();
    static ZzWorkspacePageId fromString(const QString &value);
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] QString toString() const;
    friend bool operator==(const ZzWorkspacePageId &, const ZzWorkspacePageId &) = default;
};
```

`ZzSplitWorkspace` 为其拥有的页面按需登记 id，并提供 `pageId(QWidget *)`、`pageForId(...)` 查询。页面首次参与查询、拖放或持久化时创建 id；迁移事务把 id 一同转交目标工作区。页面被第三方直接从容器取走后，登记立即失效；库不会凭旧 id 强行夺回页面。

### 3.2 进程内转移注册表

`ZzFluentUI` 内部新增进程级、线程绑定的 `ZzWorkspaceTransferRegistryPrivate`，负责跨 `ZzSplitWorkspace` 实例传递拖放令牌。它是拖放实现细节，不作为公共 API 暴露，应用层只能通过工作区转移 API 使用该能力。

```cpp
class ZzWorkspaceTransferRegistryPrivate final : public QObject
{
    Q_OBJECT
public:
    static ZzWorkspaceTransferRegistryPrivate *instance();

    [[nodiscard]] ZzCore::ZzResult<QByteArray> publish(
        ZzSplitWorkspace *source,
        const ZzTabGroupId &sourceGroup,
        int sourceIndex,
        QWidget *page);

    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> consume(
        const QByteArray &token,
        ZzSplitWorkspace *target);

    void invalidateWorkspace(ZzSplitWorkspace *workspace) noexcept;
};
```

记录字段固定为：版本、随机 token、来源工作区 id、来源组 id、来源索引、`ZzWorkspacePageId`、创建时间和 5 秒截止时间。MIME 数据只携带版本化不透明 token 与公开 id，不携带 QObject 裸指针。

令牌只允许由注册表创建，应用不能从任意字符串构造可消费令牌。`ZzWorkspaceTransferRecordPrivate` 包含来源工作区观察指针、来源组、来源索引、页面 id 和页面观察指针。注册表不把这些观察指针转为拥有关系，消费前必须再次验证对象仍存活且页面仍由来源工作区拥有。

安全规则：令牌只能消费一次；超过 5 秒、来源工作区已销毁、页面 id 不匹配、目标线程不匹配、载荷超过既有限制或重复消费均拒绝。`invalidateWorkspace` 在工作区析构时调用，清理该工作区产生的全部令牌。

注册表首次使用时创建为 `QCoreApplication` 的 GUI 线程子对象，并随应用销毁；没有有效应用对象或不在 GUI 线程时拒绝发布令牌。这样可避免静态析构顺序问题，并保证静态库和动态库构建使用同一应用级实例。

### 3.3 `ZzSplitWorkspace` 跨实例 API

在现有 API 之后追加以下接口和信号：

```cpp
[[nodiscard]] ZzWorkspacePageId pageId(const QWidget *page) const;

[[nodiscard]] ZzCore::ZzResult<void> transferTabToWorkspace(
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *targetWorkspace,
    const ZzTabGroupId &targetGroup,
    int targetIndex = -1,
    ZzWorkspaceDropZone zone = ZzWorkspaceDropZone::Center);

void setEmptyGroupPolicy(ZzEmptyGroupPolicy policy) noexcept;
[[nodiscard]] ZzEmptyGroupPolicy emptyGroupPolicy() const noexcept;

Q_SIGNALS:
    void activePageChanged(QWidget *page, const ZzWorkspacePageId &id);
    void pageActivityChanged(QWidget *page, const ZzWorkspacePageId &id,
                             bool modified, bool attention);
    void groupEmptied(const ZzTabGroupId &id);
    void tabTearOffRequested(const ZzTabGroupId &sourceGroup,
                             int sourceIndex,
                             const ZzWorkspacePageId &pageId,
                             const QPoint &globalPosition,
                             const QSize &recommendedSize);
```

`ZzEmptyGroupPolicy` 定义为 `Keep`、`Remove`、`RemoveUnlessLast`；`RemoveUnlessLast` 是默认值。`tabTearOffRequested` 是由现有 `ZzTabWidget::tearOffRequested` 提升后的工作区级意图，仍不包含窗口指针，也不要求 `ZzFluentUI` 知道应用的顶层窗口类型。程序化撕出直接调用协调器的 `tearOff`。

`transferTabToWorkspace` 是唯一允许应用层直接执行跨实例迁移的入口。`Center` 使用 `targetIndex`；物理边缘区域忽略 `targetIndex`，在目标组旁创建分屏后落位。来源和目标为同一个工作区时复用既有组内事务，不建立跨实例令牌。事务步骤如下：

1. 在来源和目标 GUI 线程、组 id、索引、页面 id、布局键冲突、目标插槽、目标树深及组上限上做完整预检；
2. 将来源页与元数据（标题、图标、固定、修改、attention、关闭能力、`pageLayoutKey`）复制到临时 escrow；
3. 从来源模型移除页，将同一 widget 插入目标容器；
4. 更新双方页面 id、布局键映射、当前页和活动组；
5. 按 `EmptyGroupPolicy` 处理来源空组；
6. 对双方运行不变量审计；全部通过后发出 `tabTransferCommitted` 和相关活动信号；
7. 任一步骤失败，按逆序恢复来源容器、目标容器、元数据和当前选择，返回带阶段信息的 `ZzResult`。

默认空组策略为 `RemoveUnlessLast`：非最后一个空组自动删除，工作区最后一个组保留。应用可以选择 `Keep` 或 `Remove`，但删除最后一个组仍被拒绝以保持工作区可用。

`ZzSplitWorkspace` 不创建窗口。它只提升带有组、页面 id、全局位置和推荐尺寸的撕出意图；窗口创建交由协调器完成。保留现有 `ZzTabWidget::tearOffRequested` 信号以兼容现有调用方。

现有 `ZzTabWidget::transferTabTo` 签名保留：两个独立标签控件或同一工作区内的调用维持原语义；当来源和目标分别属于两个不同 `ZzSplitWorkspace` 时，自动委托给工作区级事务，禁止绕过布局键和页面 id 迁移。工作区事务使用私有无信号转移原语，避免递归调用公开方法。

事务期间，来源和目标工作区进入不可重入状态，来自同步信号槽的再次分屏、转移、恢复或删除组请求返回 `InvalidState`。内部容器信号在状态提交后按确定顺序补发；对象被外部强制销毁时不得解引用失效指针，并以 `rollback_object_expired` 报告不能完整恢复的异常路径。

### 3.4 `ZzWorkspaceWindowConfiguration`

`ZzPureTools` 新增值对象描述顶层窗口独立配置：

```cpp
enum class ZzWindowClosePolicy : std::uint8_t { Allow, Deny, Delegate };

struct ZzWorkspaceWindowConfiguration {
    QString title;
    QIcon icon;
    ZzWindowClosePolicy closePolicy = ZzWindowClosePolicy::Allow;
    ZzWorkspaceTitleMode titleMode = ZzWorkspaceTitleMode::Application;
    bool alwaysOnTop = false;
    QSize minimumSize;
    QSize maximumSize;
    QRect initialGeometry;
};
```

配置在窗口创建时解析一次，创建后可通过窗口对象的 setter 独立修改，不自动同步到其他窗口。配置来源支持三种模式：协调器默认值、来源窗口快照、调用方显式值。每个字段按“显式值 > 来源快照 > 默认值”解析，因此可实现部分继承。

为支持字段级继承，实际创建参数使用 `std::optional<T>` 包装每个可继承字段；上面的 `ZzWorkspaceWindowConfiguration` 表示解析完成后的值。空的 `minimumSize`/`maximumSize` 表示不覆写 Qt 默认约束，`initialGeometry` 为空表示由协调器根据撕出位置、页面尺寸和屏幕可用区域计算。

### 3.5 `ZzWorkspaceWindowCoordinator`

`ZzPureTools` 新增协调器，负责窗口拓扑，不负责业务页面：

```cpp
class ZZ_PURE_TOOLS_EXPORT ZzWorkspaceWindowCoordinator final : public QObject
{
    Q_OBJECT
public:
    void setWindowFactory(ZzWorkspaceWindowFactory factory);

    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceWindowConfiguration> configuration(
        ZzApplicationWindow *window) const;
    [[nodiscard]] ZzCore::ZzResult<void> applyConfiguration(
        ZzApplicationWindow *window,
        const ZzWorkspaceWindowConfigurationPatch &patch);

    [[nodiscard]] ZzCore::ZzResult<ZzWorkspaceWindowHandle> createWindow(
        const ZzWorkspaceWindowCreateOptions &options = {});

    [[nodiscard]] ZzCore::ZzResult<void> tearOff(
        ZzSplitWorkspace *sourceWorkspace,
        const ZzTabGroupId &sourceGroup,
        int sourceIndex,
        const ZzWorkspaceWindowCreateOptions &options = {});

    [[nodiscard]] ZzCore::ZzResult<void> closeWindow(
        ZzApplicationWindow *window);
    [[nodiscard]] ZzCore::ZzResult<void> approveDelegatedClose(
        ZzApplicationWindow *window);

    [[nodiscard]] ZzCore::ZzResult<QByteArray> saveTopology() const;
    [[nodiscard]] ZzCore::ZzResult<void> restoreTopology(
        const QByteArray &state,
        const ZzWorkspacePageResolver &pageResolver);

Q_SIGNALS:
    void windowCreated(ZzApplicationWindow *window);
    void windowAboutToClose(ZzApplicationWindow *window,
                            const QList<QWidget *> &pages);
    void windowCloseApprovalRequested(ZzApplicationWindow *window);
    void windowConfigurationChanged(
        ZzApplicationWindow *window,
        const ZzWorkspaceWindowConfiguration &configuration);
    void orphanedPages(const QList<QWidget *> &pages);
    void activePageChanged(ZzApplicationWindow *window, QWidget *page,
                           const ZzWorkspacePageId &id);
};
```

协调器由 `ZzPureApplication` 延迟创建和独占，通过新增的 `workspaceWindowCoordinator()` getter 返回观察指针；不开放公共构造函数，从而保证每个应用只有一个窗口拓扑来源。协调器也不会接管窗口或 Shell 所有权。

`ZzWorkspaceWindowCreateOptions` 包含 `std::optional` 配置字段、配置来源模式（默认值/来源窗口/显式）、来源窗口观察指针、是否显示及是否成为活动窗口。`ZzWorkspaceWindowFactory` 是应用注册的统一装配回调：输入创建选项，调用应用现有的 `ZzPureApplication::createWindow()` 和既有 `ZzWindowSetupCallback` 完成业务表面装配，返回 `ZzWorkspaceWindowHandle`。该 handle 只含同线程的 `ZzApplicationWindow` 与 `ZzWorkspaceShell` 观察指针；两者仍分别由 `ZzPureApplication` 和应用壳层拥有。工厂成功返回前必须保证 Shell 已装入窗口且两者生命周期关联，失败则自行撤销未提交装配。

`ZzWorkspacePageResolver` 是布局恢复回调：输入非空且在拓扑内全局唯一的 `pageLayoutKey`，返回应用创建或找回的无父页面 widget；空键页面不会跨重启恢复。协调器通过已注册的窗口工厂创建窗口，不把窗口所有权暴露给页面回调。协调器将 `tearOff` 实现为：创建不可见窗口 → 获得已装配的独立 `ZzWorkspaceShell`/`ZzSplitWorkspace` → 调用跨工作区事务迁移页面 → 应用配置 → 提交并显示。创建或迁移失败时请求应用关闭本次暂存窗口，来源页保持不变。

`applyConfiguration` 使用 `ZzWorkspaceWindowConfigurationPatch` 的可选字段只修改指定配置，并同步 `ZzWorkspaceShell`、标题栏与 `ZzWindowKit`。其他窗口不受影响。协调器为已登记窗口安装关闭事件过滤器：`Allow` 同步执行页面回收，`Deny` 忽略关闭，`Delegate` 忽略本次事件并只发出一次 `windowCloseApprovalRequested`；应用确认后调用 `approveDelegatedClose`。该接口只批准当前待决请求，不能绕过 `Deny`。

## 4. 窗口关闭与页面回收

关闭请求必须经过以下顺序：

1. 读取窗口关闭策略；`Deny` 立即忽略，`Delegate` 发出 `windowCloseApprovalRequested` 交给应用确认，`Allow` 直接进入迁移阶段；
2. 收集窗口内所有页面，并读取协调器为每页维护的回收来源栈；每次跨窗口转移把即时来源窗口、来源组和来源索引压栈，转回栈中已有窗口时截断已经返回的路径；
3. 选择回收目标：显式指定目标窗口，其次主窗口，再其次仍存活的第一个窗口；
4. 目标不存在时不销毁页面，发出 `orphanedPages` 并保持源窗口打开；应用处理页面后可以再次请求关闭；
5. 所有页面迁移成功后才真正关闭窗口。任意页面迁移失败则回滚已迁移页面，窗口继续保持可见。

回收时先弹出页面来源栈顶并尝试恢复原组；原组不存在时迁入目标窗口活动组。栈顶窗口不存在时继续尝试下一层，然后依次回退到显式目标、主窗口和首个存活窗口。这保证 A → B → C 的页面在 C、B 依次关闭时可按 C → B → A 返回，同时不影响用户主动拖回某个既有来源窗口。

应用整体退出由 `ZzPureApplication::beginShutdown()` 标记，协调器此时跳过普通回收，避免退出阶段窗口之间互相迁移。

## 5. 多窗口布局持久化

新增 `ZZWT`（Zz Workspace Topology）二进制信封，沿用当前 `ZZSW` 的 magic、schema、长度上限、Qt 版本标记和 SHA-256 校验策略：

- schema v2 顶层包含窗口列表、窗口 id、窗口配置、屏幕几何、可见/最大化/置顶状态和该窗口的 `ZZSW` 工作区状态；
- 页记录使用 `ZzWorkspacePageId` 与可选 `pageLayoutKey`，表达“页 → 窗口 → 组 → 索引”，并保存页面回收来源栈；
- `restoreTopology` 先完整解码为有界纯值对象，再通过已注册的窗口工厂逐个创建暂存窗口、通过 `ZzWorkspacePageResolver` 解析页面，最后执行页面归属事务；
- `restoreTopology` 只允许在协调器尚无已提交业务页面时调用；已有页面时返回 `InvalidState`，避免覆盖运行中的工作区；
- 任一窗口或页面恢复失败，销毁本次恢复创建的全部暂存窗口，调用前空拓扑保持不变；
- v2 读取器接受现有 `ZZSW` v1，将其包装为一个默认窗口；单工作区 `saveLayout/restoreLayout` 保持原行为。

窗口几何同时记录屏幕稳定名称和逻辑坐标。恢复时屏幕不存在或几何完全离屏，则按目标屏幕 `availableGeometry()` 收敛；找不到原屏幕时使用当前主屏幕，不能恢复历史 DPI 下的物理像素尺寸。

禁止从存档反序列化 QObject 指针、函数对象或任意路径；窗口数量、页面数量、字符串长度和树深度均受现有硬上限约束。

## 6. 错误与可观测性

所有新增可失败 API 返回 `ZzCore::ZzResult`，错误码复用现有 `ZzCore::ZzErrorCode`：参数或句柄不满足约束使用 `InvalidArgument`，对象已销毁/状态不允许使用 `InvalidState`，超时使用 `TimedOut`，找不到窗口、组或页面使用 `NotFound`，后端或平台拒绝使用 `Backend`/`Unsupported`，存档读写错误使用 `Io`，无法归类的事务失败使用 `Unknown`。错误的 `technicalMessage/context` 必须包含阶段名和稳定对象 id，不包含页面内容。令牌拒绝、容量超限和回滚失败作为上下文字符串中的稳定原因标签，不新增错误枚举。

协调器和工作区新增结构化日志事件：`window.create`、`window.tear_off`、`page.transfer`、`page.reclaim`、`layout.restore`。日志只记录 id、阶段、耗时和结果，默认不打印业务标题或终端数据。

## 7. 测试与验收

### P0 必须通过

- 同一 widget 撕出到新窗口，页面指针、标题、图标、固定/修改/attention 状态保持不变；
- 两个工作区双向程序化转移和 overlay 拖放均成功，来源组按策略回收；
- 令牌伪造、过期、重复消费、来源销毁、线程不匹配全部拒绝；
- 转移失败时来源/目标树、页面顺序、布局键和当前页完全恢复；
- 新窗口独立分屏后可再次转移回原窗口；
- 关闭策略 `Allow/Deny/Delegate`、无回收目标和应用整体退出路径均有测试。

### P1 必须通过

- 关闭标签拖空组自动按策略处理，最后一组边界稳定；
- 两窗口切换任意组/页时窗口级 `activePageChanged` 只报告真实变化；
- 页面修改和 attention 元数据在工作区聚合查询中一致。

### P2 必须通过

- 标签右键菜单 provider 不需要继承 `ZzTabBar`；
- `ZZWT` v2 双窗口保存/恢复完整，v1 单窗口存档可读；
- Linux GCC Debug/Release 运行测试；Windows MSVC/MinGW 和 macOS 至少完成 CMake 配置、头文件和静态编译检查。

## 8. 实施顺序与提交边界

1. **P0-A**：页面 id、进程内转移注册表、跨工作区事务 API、单元测试。
2. **P0-B**：窗口配置值对象、窗口工厂、协调器撕出与关闭回收、集成测试。
3. **P1**：空组策略、窗口级活动页聚合、日志和验收脚本。
4. **P2**：右键菜单 provider、attention 聚合视觉、`ZZWT` v2 持久化。

每个阶段单独提交，提交标题使用中文简述，正文说明 API、测试和兼容性影响。实现前必须先依据本规格生成逐文件 TDD 计划；未列入本规格的 UI 或业务功能不得混入多窗口提交。

## 9. 规格自检结论

- 未发现占位符、未决接口或未定义的关键错误路径。
- 所有跨工作区页面移动均通过同一事务入口，未保留应用层手工搬运模型的隐式路径。
- v1 单窗口布局、既有撕出信号和 `ZzWorkspaceShell` API 均保持兼容。
- 窗口配置继承只在创建时发生，满足窗口创建后独立修改的要求。
- 非目标明确排除了跨进程、业务模型和窗口装饰，范围可由一个后续实现计划分批覆盖。
