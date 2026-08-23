# WorkspaceShell 事务引擎重构设计

## 1. 背景与目标

任务 9 已完成布局 schema 2、schema 1 迁移、Activity 重排和跨侧迁移的原型，
但五轮独立审查持续发现同步 Qt 信号可以污染事务中间状态。原型的共同缺陷不是缺少某个
空指针判断，而是事务在修改 UI 后从实时控件捕获所谓“已提交投影”，从而把同步回调造成
的错误状态学习成成功目标。

本重构的目标是建立一个可证明的 WorkspaceShell 事务模型：事务开始修改任何 QObject
之前，必须从请求与不可变快照推导完整目标；提交、终态审计和回滚只能引用这些预先计算的
值，绝对禁止从提交后的 UI 实况更新预期。

重构必须同时满足：

- 保持 `ZzWorkspaceShell` 现有公开 API 和布局 schema 2 不变；
- 保持 schema 1 到 schema 2 的迁移能力；
- 支持公开 Qt signals 和组件 API 中发生的同步重入；
- Shell 自身公开变更 API 在事务期间返回 `InvalidState`；
- 不支持页面自定义 `ParentChange` 或 event filter 在换父期间删除无关 workspace 对象；
- 第三方已经接管的 QWidget 不得被回滚逻辑强行夺回；
- Qt 6.8+、C++20、无 Qt Private API、无 stylesheet、无业务模型依赖；
- Linux 完整运行验证，Windows MSVC/MinGW 与 macOS AppleClang 静态检查；
- 所有新增自定义类型使用 `Zz` 前缀和简体中文 Doxygen。

## 2. 不采用的方案

### 2.1 继续在 Shell Private 中增加检查

该方案改动最少，但当前 `ZzWorkspaceShellPrivate.cpp` 已超过 3700 行，布局 codec、
请求归一化、QObject mutation、终态审计和回滚互相穿插。第五轮审查仍能找到新的
learned-state 路径，说明局部 guard 无法建立完整正确性边界，因此不采用。

### 2.2 修改 Fluent 组件以屏蔽或延迟信号

可以为 SidePane、SplitWorkspace、BottomPane 增加批处理模式，事务期间阻塞信号或延迟
通知。但这会改变通用组件对所有消费者的同步信号合同，也无法覆盖调用方直接操作
QMainWindow 或 QWidget parent 的情况。该方案范围过大，且与“支持同步重入”的决定冲突，
因此不采用。

### 2.3 采用独立私有事务引擎

本设计采用该方案。codec、值快照、目标投影、布局恢复事务和 Activity move 事务分别承担
单一职责。Shell 只负责持有对象、注册表和公开入口转发。

## 3. 文件与职责

新增文件：

```text
ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.h
ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp
ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h
ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp
ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.h
ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp
ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h
ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
```

修改文件：

```text
ZzPureTools/CMakeLists.txt
ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h
ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp
ZzPureTools/tests/ZzWorkspaceShellTest.cpp
```

各主类职责：

- `ZzWorkspaceLayoutStatePrivate`：定义布局 DTO、组件身份、事务快照、规范化目标投影和
  纯值比较；不调用 QWidget mutation API。
- `ZzWorkspaceLayoutCodecPrivate`：负责 `ZZWS` envelope、schema 1/2、Split 子布局的
  有界读取、规范化编码和静态校验；读取阶段不得访问 QWidget。
- `ZzWorkspaceLayoutTransactionPrivate`：构造布局恢复计划，执行固定阶段，审计并反向回滚。
- `ZzWorkspaceActivityMoveTransactionPrivate`：从 move 意图构造固定计划，提交 Activity
  重排或跨侧迁移，审计并回滚。
- `ZzWorkspaceShellPrivate`：持有稳定控件、panel 注册表、生命周期连接和全局事务门；
  不再包含布局格式解析和大段事务算法。

这些类全部是 ZzPureTools 的私有实现，不增加公开 facade。采用私有 `.h/.cpp` 两文件结构，
避免为不可见实现再增加一层 Pimpl；没有虚函数调用，也不要求额外堆分配。

## 4. 值模型

`ZzWorkspaceLayoutStatePrivate` 提供以下嵌套值类型，所有嵌套类型同样使用 `Zz` 前缀：

```cpp
class ZzWorkspaceLayoutStatePrivate final
{
public:
    struct ZzPanelIdentity final;
    struct ZzSubsystemIdentity final;
    struct ZzSideProjection final;
    struct ZzBottomProjection final;
    struct ZzDockProjection final;
    struct ZzSplitProjection final;
    struct ZzActivityProjection final;
    struct ZzTitleProjection final;
    struct ZzWorkspaceProjection final;
    struct ZzWorkspaceSnapshot final;
    struct ZzLayoutRequest final;
};
```

### 4.1 Panel 身份

每条 Panel 身份至少包含：

- `ZzWorkspacePanelId`；
- Panel kind；
- `QPointer<QWidget>`；
- 捕获时的 raw QWidget identity；
- registration generation；
- Dock 时的 `QPointer<ZzDockPanel>` 和 raw Dock identity。

终态身份相等必须同时满足 ID、kind、generation、QPointer 和 raw identity。只比较 ID 或
QPointer 不足以识别同 ID replacement。

### 4.2 Side 投影

每侧投影包含：

- Pane 与 PanelStack identity；
- 按 PanelId 表示的完整 stack order；
- 可见 PanelId 顺序；
- 与可见顺序一一对应的 sizes；
- current PanelId；
- collapsed 和 pane width；
- 每个内容的实际 stack membership 与 ancestry 要求。

### 4.3 Bottom、Dock、Split、Activity 和 Title 投影

- Bottom：Pane/内部 stack identity、完整注册顺序、current、collapsed、pane height、
  stack membership 和 ancestry。
- Dock：Dock/content identity、QMainWindow 中的 area、floating、visible 和实际 owner。
- Split：规范化树、active group、group 顺序、splitter sizes、keyed page 的组/顺序/current，
  以及规范化后的 schema 1 或 schema 2 Split blob。
- Activity：模型 identity、完整 rows、左右 current source 和 active source ID 集合。
- Title：title mode、宿主标题和标题栏标题。

## 5. 事务不可变性

事务对象分为三个时间段：

1. `prepare()`：解码请求、捕获原始快照、构造目标投影；允许创建和修改 builder。
2. `execute()`：目标投影以 `const` 值保存；任何 QObject mutation 后都不得修改目标。
3. `rollback()`：只使用原始快照转换出的固定回滚投影。

生产代码中禁止以下模式：

```cpp
applyPhase();
expected = captureCurrentUi(); // 禁止：可能学习同步回调造成的污染
```

允许的模式是：

```cpp
const ZzWorkspaceProjection expected = buildExpected(snapshot, request);
if (!applyPhase(expected) || !auditPhase(expected)) {
    return rollback(snapshot);
}
```

## 6. 纯值规划器

目标投影由一个不访问实时 QWidget 的纯值规划过程生成。规划器在原始快照的副本上模拟
固定操作序列：area/order、take/add、visibility、sizes、current fallback、collapsed、
Activity rows 和 title mode。模拟器和执行器必须使用同一组显式步骤定义，不能各自维护
一套隐含 fallback。

关键规范化规则：

- 布局请求未描述的已注册 Side panel 保留快照 area、相对顺序、可见性和 size；
- 请求描述但当前未注册的 PanelId 被忽略，不创建占位 QWidget；
- Side current 优先使用目标侧已注册的 requested current；否则使用该 SidePane 的快照
  current；不能使用 Activity current 代替 Side current；
- fallback panel 已迁出该侧或已失效时，按 SidePane 固定的可见顺序规则选择 current，
  没有可见 panel 时 current 为空且 pane 折叠；
- Bottom current 不存在时保留快照中仍注册在 Bottom stack 的 current；
- Activity current 与 active 集合从最终 Side 投影派生，不反向影响 Side 投影；
- schema 1 的 current tab index 在纯 Split 投影中应用到初始根组，再编码为规范化 Split
  目标；
- title mode 直接取请求值，标题文本从目标 active group/current page 的展示信息推导。

## 7. 布局恢复事务

### 7.1 Prepare

执行任何 UI 修改前完成：

1. 校验线程、Shell 全局事务门和 subsystem 存活状态；
2. 解码 envelope 与 payload；
3. 捕获 subsystem、panel identity 和完整原始投影；
4. 从请求与原始投影构造固定目标投影；
5. 验证目标自身满足数量、唯一性、owner 和尺寸不变量；
6. 设置 `layoutRestoreInProgress` RAII 门。

所有返回 `ZzResult` 且会读取或修改布局的 Shell API，包括 Side/Bottom/Dock 注册、
take/remove、show、badge、save 和递归 restore，在门开启时返回 `InvalidState`；内部
Activity activate/move 意图在门开启时不执行 mutation。`setAlwaysOnTop()`、应用标题和
自定义标题不属于持久化布局，可以在事务期间修改并保留。公开 `setTitleMode()` 是不能返回
错误的既有 `void` API，因此同步回调中的外部调用允许执行，但最终 title mode 审计必须
检测偏差并使外层恢复进入回滚；事务执行器设置目标 title mode 时直接使用 Private 入口，
不能经过公开重入路径。

### 7.2 Commit

顺序保持现有规格：

```text
Qt Dock -> Split -> Side -> Bottom -> Activity/Title
```

每个阶段：

1. 使用 `QPointer` 和捕获的 raw identity 调用一个 mutation；
2. mutation 返回后立即复核本次调用涉及的 subsystem、panel identity 和 owner；
3. 阶段结束后把实时状态捕获为 observed，只用于与固定 expected 比较；
4. observed 不得写回 expected；
5. 不相等立即进入 rollback。

### 7.3 Qt Dock 目标

QMainWindow state 是 Qt 的不透明格式，不能用字符串解析。事务 prepare 阶段创建不显示的
临时 QMainWindow 和与注册 Dock objectName 对应的临时 QDockWidget，在临时窗口中从原始
Dock 投影应用请求 state，得到无业务回调污染的目标 Dock 投影。临时对象在 prepare 完成后
销毁，不进入真实对象树，不连接业务信号。

真实宿主执行 `restoreState(requestedQtState, 1)` 后，按目标 Dock 投影审计 area、floating、
visible、Dock identity、content identity 和 owner。不得在真实 restore 后调用 `saveState()`
生成 expected。

### 7.4 Split 目标

Split codec 将请求 blob 解码为有界纯值树并重新编码为规范化目标。真实
`ZzSplitWorkspace::restoreLayout()` 返回后，即使 `layoutChanged` 同步回调再次调用
`splitGroup()`，终态 `saveLayout()` 也必须与 prepare 阶段的规范化目标一致，否则回滚。

### 7.5 最终审计

所有阶段成功后，再统一审计：

- subsystem identity；
- panel ID/kind/generation/content identity；
- Side/Bottom stack membership 和 ancestry；
- Dock 实际 owner；
- Split、Side、Bottom、Activity、Title 的完整固定目标投影。

只有最终审计通过才能清除事务门并返回成功。

## 8. 回滚

失败后按以下顺序应用原始快照：

```text
Activity/Title -> Bottom -> Side -> Split -> Qt Dock
```

回滚使用与正向提交相同的 executor 和 audit，不维护第二套弱化逻辑。完成后统一审计原始
subsystem、所有 snapshot panel 和原始投影：

- 全部恢复成功：返回 `InvalidState`，消息包含 `was rolled back`；
- 任一步失败、对象被删除、generation 变化或内容已由第三方接管：返回 `InvalidState`，
  消息包含 `rollback failed`；
- 第三方 owner 下的内容只记录失败并清理失真的 Shell 注册，不强行 reparent。

## 9. Activity move 事务

Activity move 使用独立事务类，但遵循相同模型：

1. 从 source index 解析稳定 PanelId；
2. 捕获左右 Pane、PanelStack、Activity model、panel identity 和原始投影；
3. 从原快照与 `targetArea/targetRow` 纯值计算目标 rows、order、visible、sizes、current、
   active 集合和 area；
4. 同侧只执行固定 order 变换，跨侧执行 take/add；
5. 最后提交 Activity model，但 `modelReset` 返回后仍需审计 model identity；
6. `movePanel`、`setWidgetVisible`、`setCurrentWidget`、`setPanelSizes`、`replaceRows` 每个
   信号边界后检查 Pane/stack/model/content identity、membership 和 ancestry；
7. 最终 observed 只与 prepare 阶段的固定 move target 比较；禁止在最后一个 sizes 信号后
   捕获 learned checkpoint；
8. 失败时使用原始投影回滚，第三方接管时不夺回内容。

## 10. Codec 边界

现有 schema 2 格式和常量保持不变：

```cpp
constexpr quint16 zzWorkspaceEnvelopeVersion = 2;
constexpr quint16 zzLegacyWorkspaceEnvelopeVersion = 1;
constexpr int zzQtMainWindowStateVersion = 1;
constexpr auto zzLayoutStreamVersion = QDataStream::Qt_6_8;
```

继续强制：

- 总 envelope 不超过 1 MiB；
- 每侧最多 32 个可见 Side panel；
- Side entries 最多 4096；
- Split group 最多 64、树深最多 16、节点最多 127、saved page 最多 4096；
- ID/key 最多 256 个 UTF-16 code unit；
- 数量读取前检查上限，禁止根据恶意 count 预分配；
- visible 与 sizes 一一对应，size 为正且有界；
- ID、order、area 和枚举合法且集合内唯一；
- writer 和 reader 使用同一 validator，writer 不能生成 reader 拒绝的 blob；
- 读取阶段不访问 QWidget，不产生部分 UI 修改。

## 11. 错误合同

- envelope、payload、摘要、枚举、数量或 Split blob 非法：`InvalidArgument`；
- 当前 UI 状态不能编码、超过上限或 subsystem 已失效：`InvalidState`；
- 事务重入：`InvalidState`，消息包含 `transaction is in progress`；
- 提交失败且回滚成功：`InvalidState`，消息包含 `was rolled back`；
- 回滚不完整：`InvalidState`，消息包含 `rollback failed`；
- 编码流写入失败：`Io`。

公开错误消息继续使用稳定英文技术文本，Doxygen 和设计文档使用简体中文。

## 12. TDD 与审查

### 12.1 必须先建立的三个 RED

1. SidePane current 与 Activity current 不同，恢复 unknown/empty current 时必须保留真实
   SidePane current，旧 projection 应误拒绝。
2. Split restore 的 `layoutChanged` 回调调用 `splitGroup()`，事务必须拒绝污染结果并回滚。
3. Activity move 最后一次 `panelSizesChanged` 回调分别隐藏 moved content、交给第三方、
   删除 Activity model，事务必须失败且不得学习污染状态。

### 12.2 回归矩阵

保留现有 WorkspaceShell 测试，并按 mutation 边界覆盖：

- Qt Dock、Split、Side、Bottom、Activity/Title 每阶段的 phase-local 覆盖；
- 后续阶段覆盖已经提交阶段；
- subsystem 删除和 replacement；
- panel 删除、同 ID replacement 和 generation 变化；
- stack membership 与 ancestry 分裂；
- 第三方 owner；
- 成功恢复、失败后完整回滚和 rollback failed；
- schema 1 迁移、schema 2 round trip 与全部有界 DTO。

测试必须通过公开 API 或真实 Qt 信号触发，不通过 mock 断言内部 helper。

### 12.3 审查门

本重构作为新的任务 9R，重新获得最多五轮独立审查额度。每轮修复必须有目标 RED/GREEN
证据；第 5 轮仍存在承重 Critical/Important 时再次熔断，不进入任务 10。

## 13. 性能与资源

- codec、规划和审计相对 panel/group 数量保持线性；
- 所有容器在已验证 count 后 reserve，不做无界预分配；
- 事务同步完成，不调用 `processEvents()`，不创建 timer 或 animation；
- QWidget mutation 不使用协程，避免跨事件循环悬挂 UI identity；
- 值快照使用移动语义、Qt 隐式共享和预留容量；
- 每次事务允许为 Qt Dock 目标创建一个短生命周期 shadow QMainWindow，完成后 QObject 数
  必须回到基线；
- 1000 次成功/失败事务后 QObject、连接、timer 和 animation 数量稳定；
- 不在 paintEvent、resizeEvent 或持续输入热路径中执行布局序列化。

任务 15 的 render P95、结构操作 P95 和对象稳定预算保持不变，不因本重构放宽。

## 14. 平台与构建

- 新源文件加入 `ZzPureTools/CMakeLists.txt`，继续由 CMakePresets 驱动；
- Linux GCC 15 + Qt 6.11.1 完成 Debug、Release、static、Clang、ASan/UBSan 和 clang-tidy；
- Windows MSVC 2022 与 MinGW 检查标准 C++20、UTF-8、静态/动态符号和无 GCC-only 扩展；
- macOS AppleClang 检查无平台私有 API、无固定 ELF/Windows 假设；
- 不重新下载本机 Qt，本机使用 `/home/zz/Qt/6.11.1/gcc_64`；
- 暂不处理 GitHub CI，不调用 GitHub CLI，不 push。

## 15. 迁移与提交边界

当前未提交的任务 9 原型只作为测试与问题证据：

- 保留已经完成 RED/GREEN 的真实行为测试；
- codec 中已验证正确的 schema 1/2 逻辑迁移到独立 codec；
- 删除 learned projection 和混杂在 Shell Private 中的事务算法，不复制保留两套实现；
- `ZzWorkspaceShellPrivate.cpp` 在重构后应明显缩小；
- 每个独立任务只提交计划点名的文件；
- commit 标题使用中文简述，正文使用中文详细说明；
- `temp_image/` 永远不读取、不修改、不暂存、不提交。

## 16. 完成标准

任务 9R 只有同时满足以下条件才完成：

1. 三个最终阻塞发现均有先失败后通过的真实行为测试；
2. WorkspaceShell 完整测试全绿且输出干净；
3. `puretools.workspace-shell`、相关 Split/Side/Bottom 测试全绿；
4. fresh GCC Debug 构建成功；
5. architecture boundary 和 `git diff --check` 通过；
6. 全量 CTest 只允许账本中已记录且与本任务无关的既有失败；
7. 独立任务审查得到规格合规和代码质量双通过；
8. 没有未裁定的 Critical/Important；
9. 任务代码已经按中文标题和中文正文提交；
10. 任务 9R 完成后才恢复任务 10 至任务 15。
