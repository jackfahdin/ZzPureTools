# 多窗口工作区实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度；每个任务验证后立即创建中文 commit。

**目标：** 为 ZzPureToolsFrame 增加同进程多窗口工作区，使同一页面 widget 能在窗口间安全迁移、撕出、回收和持久化，同时保持窗口配置独立且不把业务逻辑放入 UI 库。

**架构：** `ZzFluentUI` 维护页面身份、应用级短期拖放令牌和跨 `ZzSplitWorkspace` 事务；`ZzPureTools` 维护 Window/Shell 登记、窗口配置、来源栈、关闭协议和多窗口拓扑。`ZzPureApplication` 继续独占顶层窗口，应用通过工厂装配业务表面，Example 仅展示公共接口。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、`ZzCore::ZzResult`、CMakePresets、QDataStream Qt_6_8、SHA-256、现有 ZzLog/benchmark 门禁。

---

## 文件结构

### `ZzFluentUI` 公共值与工作区接口

- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzWorkspacePageId.h` 与 `foundation/src/ZzWorkspacePageId.cpp`：轻量 UUID 页面身份，不使用 Pimpl，避免热路径堆分配。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzEmptyGroupPolicy.h`：空组策略枚举。
- 修改 `ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`、`widgets/src/ZzSplitWorkspace.cpp`：页面身份查询、跨实例事务、空组策略、活动页和撕出信号。
- 创建 `ZzFluentUI/widgets/src/private/ZzWorkspaceTransferRegistryPrivate.h/.cpp`：应用级一次性拖放令牌注册表。
- 创建 `ZzFluentUI/widgets/src/private/ZzWorkspaceCrossTransferTransactionPrivate.h/.cpp`：双工作区提交、审计和回滚。
- 修改 `ZzSplitWorkspacePrivate.h/.cpp`、`ZzTabWidgetPrivate.h/.cpp`、`ZzTabBarPrivate.h/.cpp`、`ZzTabWidget.cpp`、`ZzTabBar.cpp`：私有直接转移原语、令牌 MIME、拖放路由和元数据同步。
- 创建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzTabContextMenuProvider.h`：同步菜单扩展回调。

### `ZzPureTools` 窗口协调与拓扑

- 创建 `ZzWorkspaceWindowConfiguration.h`：解析后的窗口配置、可选字段 Patch、关闭策略枚举（值类型使用 header-only，避免无意义的动态分配和编译单元）。
- 创建 `ZzWorkspaceWindowCreateOptions.h`：配置来源、来源窗口、显示和激活选项（header-only）。
- 创建 `ZzWorkspaceWindowHandle.h`：非拥有 Window/Shell 观察句柄（header-only，仅 `isValid()` 为内联函数）。
- 创建 `ZzWorkspaceWindowFactory.h`：窗口装配工厂和页面恢复回调。
- 创建 `ZzWorkspaceWindowCoordinator.h/.cpp` 与 `private/ZzWorkspaceWindowCoordinatorPrivate.h/.cpp`：窗口登记、撕出、配置、关闭回收和活动页聚合。
- 创建 `private/ZzWorkspaceTopologyStatePrivate.h/.cpp` 与 `private/ZzWorkspaceTopologyCodecPrivate.h/.cpp`：有界纯值拓扑和 `ZZWT` schema v2 编解码。
- 修改 `ZzPureApplication.h/.cpp`、`ZzPureApplicationPrivate.h/.cpp`：协调器所有权和延迟显示窗口创建。

### 测试、Example、性能和文档

- 创建 `ZzWorkspacePageIdTest.cpp`、`ZzWorkspaceCrossTransferTest.cpp`、`ZzWorkspaceTransferRegistryPrivateTest.cpp`、`ZzWorkspaceWindowCoordinatorTest.cpp`、`ZzWorkspaceTopologyCodecPrivateTest.cpp`。
- 修改 `ZzSplitWorkspaceTest.cpp`、`ZzTabControlsTest.cpp`、`ZzMultiWindowIsolationTest.cpp` 和相应 CMakeLists。
- 修改 `examples/ZzPureToolsExample/ZzExampleWindowShell*`、`main.cpp` 及 smoke test，展示真实撕出、回收与独立配置。
- 修改 `benchmarks/ZzWorkspaceComponentsBenchmark.cpp`、`benchmarks/CMakeLists.txt`，新增跨工作区迁移时延和对象增长指标。
- 修改 `docs/development/WORKSPACE_API_ZH.md`、`FRAMEWORK_INTEGRATION_ZH.md`、`BUILDING_ZH.md` 和 `README.md`。

## P0-A：跨工作区页面迁移

### 任务 1：增加稳定页面身份和空组策略

**文件：**
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzWorkspacePageId.h`
- 创建：`ZzFluentUI/foundation/src/ZzWorkspacePageId.cpp`
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzEmptyGroupPolicy.h`
- 创建：`ZzFluentUI/tests/ZzWorkspacePageIdTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [ ] **步骤 1：编写失败的页面身份值测试**

测试默认 id 无效、`create()` 连续生成唯一值、字符串往返、非法/空字符串拒绝、`qHash` 与相等语义一致，并验证元类型可被 `QSignalSpy` 保存：

```cpp
void createsStableRoundTrippableIds()
{
    const auto first = ZzFluentUI::ZzWorkspacePageId::create();
    const auto second = ZzFluentUI::ZzWorkspacePageId::create();
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(first != second);
    QCOMPARE(
        ZzFluentUI::ZzWorkspacePageId::fromString(first.toString()),
        first);
    QVERIFY(!ZzFluentUI::ZzWorkspacePageId::fromString({}).isValid());
}
```

- [ ] **步骤 2：运行测试确认目标不存在**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspacePageIdTest --parallel 2
```

预期：构建失败，提示目标或 `ZzWorkspacePageId.h` 不存在。

- [ ] **步骤 3：实现无堆分配值对象和枚举**

`ZzWorkspacePageId` 直接保存 `QUuid value_`，公开默认构造、`create()`、`fromString(QStringView)`、`isValid()`、`toString()`、`operator==` 和 `qHash`。`fromString` 只接受带或不带花括号的规范 UUID；无效输入返回默认值。`ZzEmptyGroupPolicy` 固定为：

```cpp
enum class ZzEmptyGroupPolicy : std::uint8_t
{
    Keep,
    Remove,
    RemoveUnlessLast
};
```

全部公开声明添加简体中文 Doxygen，使用 `ZZ_FLUENT_FOUNDATION_EXPORT` 和 `Q_DECLARE_METATYPE`。

- [ ] **步骤 4：注册源码并运行测试**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --target ZzWorkspacePageIdTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.workspace-page-id$' --output-on-failure
```

预期：1/1 通过，无 `-Werror` 警告。

- [ ] **步骤 5：提交**

```bash
git add ZzFluentUI/foundation ZzFluentUI/tests/ZzWorkspacePageIdTest.cpp ZzFluentUI/CMakeLists.txt ZzFluentUI/tests/CMakeLists.txt
git diff --cached --check
git commit -m "feat(工作区): 增加稳定页面身份" -m "新增无堆分配的页面 UUID 值和空组策略，为跨工作区拖放、审计与持久化提供稳定标识。"
```

### 任务 2：建立工作区页面登记和跨实例中心转移事务

**文件：**
- 创建：`ZzFluentUI/widgets/src/private/ZzWorkspaceCrossTransferTransactionPrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzWorkspaceCrossTransferTransactionPrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzWorkspaceCrossTransferTest.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 修改：`ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.cpp`
- 修改：`ZzFluentUI/widgets/src/ZzTabWidget.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [ ] **步骤 1：编写失败的公共事务测试**

覆盖 `pageId/pageForId`、同一页面指针迁移、全部标签元数据、`pageLayoutKey` 和页面 id 跟随、目标键冲突预检、非法组/索引/线程拒绝、来源/目标当前页和活动组更新：

```cpp
const auto result = source.transferTabToWorkspace(
    sourceGroup, 0, &target, targetGroup, -1,
    ZzFluentUI::ZzWorkspaceDropZone::Center);
QVERIFY(result);
QCOMPARE(target.pageForId(pageId), page);
QCOMPARE(target.pageLayoutKey(page), QStringLiteral("terminal/session-1"));
QVERIFY(!source.pageId(page).isValid());
QCOMPARE(targetTabs->widget(targetTabs->count() - 1), page);
```

再增加一个故障注入用例：目标 `currentChanged` 槽销毁暂存参与者，断言失败后来源树、页序、元数据、布局键和活动组与事务前快照一致；第三方强行接管页面时断言不抢回页面并返回 `InvalidState`。

- [ ] **步骤 2：运行测试确认公共 API 缺失**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest --parallel 2
```

预期：编译失败，提示 `transferTabToWorkspace`、`pageId` 和 `pageForId` 未定义。

- [ ] **步骤 3：实现页面登记和不可重入保护**

在 `ZzSplitWorkspacePrivate` 增加 `QHash<QWidget *, ZzWorkspacePageId>`、反向映射、销毁连接和 `transactionDepth`。`pageId()` 对当前工作区真实拥有的页面按需创建 id；页面移出或销毁后同步清除。以下调用在事务中返回 `InvalidState` 或保持现有 bool API 返回 false：`splitGroup`、`removeEmptyGroup`、`transferTab`、`moveTabToDropZone`、`restoreLayout` 和新的跨实例 API。

- [ ] **步骤 4：实现中心转移事务**

`ZzWorkspaceCrossTransferTransactionPrivate` 预检两个工作区、组、插槽、页面 id 和布局键唯一性；捕获双方树、页面顺序、当前页、活动组、元数据和键映射；通过 `ZzTabWidgetPrivate::transferToDirect()` 移动同一 widget；提交后目标接管 id/key，来源移除登记。公开签名固定为：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> transferTabToWorkspace(
    const ZzTabGroupId &sourceGroup,
    int sourceIndex,
    ZzSplitWorkspace *targetWorkspace,
    const ZzTabGroupId &targetGroup,
    int targetIndex = -1,
    ZzWorkspaceDropZone zone = ZzWorkspaceDropZone::Center);
```

同时新增 `pageForId(const ZzWorkspacePageId &) const`，未知 id 返回 nullptr；`pageId(const QWidget *) const` 对不属于本工作区的页面返回无效 id。新增精确签名 `void tabTransferCommitted(ZzSplitWorkspace *sourceWorkspace, const ZzTabGroupId &sourceGroup, const ZzTabGroupId &targetGroup, QWidget *page, const ZzWorkspacePageId &pageId, ZzWorkspaceDropZone zone)`，只在双工作区审计通过后发一次。`ZzTabWidget::transferTabTo` 检测到两个不同工作区宿主时委托该 API；事务内部只调用私有直接原语，防止递归。

- [ ] **步骤 5：运行定向回归**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.(workspace-cross-transfer|split-workspace|tab-controls)$' --output-on-failure
```

预期：跨实例测试和既有复杂回滚测试全部通过。

- [ ] **步骤 6：提交**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests/ZzWorkspaceCrossTransferTest.cpp ZzFluentUI/tests/CMakeLists.txt ZzFluentUI/CMakeLists.txt
git diff --cached --check
git commit -m "feat(工作区): 支持跨实例页面转移" -m "新增页面身份登记和双工作区中心转移事务，同步迁移标签元数据与布局键，并对失败执行双边回滚。"
```

### 任务 3：将拖放令牌提升为应用级私有注册表

**文件：**
- 创建：`ZzFluentUI/widgets/src/private/ZzWorkspaceTransferRegistryPrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzWorkspaceTransferRegistryPrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzWorkspaceTransferRegistryPrivateTest.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabBarPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabBarPrivate.cpp`
- 修改：`ZzFluentUI/widgets/src/ZzTabBar.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [ ] **步骤 1：编写注册表安全测试**

测试使用注入的 `std::chrono::steady_clock::time_point`，不执行 5 秒 sleep。覆盖 publish/inspect/consume、一次性消费、5 秒边界、重放、伪造、来源页面移走、来源工作区销毁、目标跨线程、4096 字节载荷上限和应用销毁清理。验证 1000 次发布/失效后没有 `QTimer` 或 QObject 增长。

- [ ] **步骤 2：运行测试确认私有类型不存在**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest --parallel 2
```

预期：构建失败，提示私有注册表头不存在。

- [ ] **步骤 3：实现注册表和纯令牌 MIME**

注册表作为 `QCoreApplication` 的 GUI 线程子对象按需创建，记录 `QPointer<ZzTabWidget>`、可选来源工作区、组、来源索引、页面 id、`QPointer<QWidget>` 和 deadline。`inspect()` 不消费，`consume()` 原子移除记录。`ZzTabMimeData` 只保存随机 128-bit token 的版本化 `QByteArray`，删除 `source/page` 裸指针成员；MIME 格式升级为 `application/x-zz-workspace-transfer-v2`。

- [ ] **步骤 4：接入 TabBar 和 SplitWorkspace 拖放**

`ZzTabBarPrivate::startDrag()` 发布一次令牌；TabBar 中心 drop 与工作区 overlay 都先 `inspect()` 再在真正提交点 `consume()`。取消、离开、来源析构和页面迁移立即失效令牌。删除 `ZzSplitWorkspacePrivate::dragTokens`、`ensureDragToken()` 和实例私有查询逻辑。

- [ ] **步骤 5：运行安全及旧拖放测试**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.(workspace-transfer-registry-private|split-workspace|tab-controls)$' --output-on-failure
```

预期：伪造、过期和重放全部拒绝；独立 `ZzTabWidget` 间拖放仍可用。

- [ ] **步骤 6：提交**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests/ZzWorkspaceTransferRegistryPrivateTest.cpp ZzFluentUI/tests/CMakeLists.txt ZzFluentUI/CMakeLists.txt
git diff --cached --check
git commit -m "feat(拖放): 增加应用级一次性令牌" -m "移除 MIME 中的 QObject 指针，以应用级私有注册表支持跨工作区验证、过期清理和重放拒绝。"
```

### 任务 4：完成跨窗口五区拖放和撕出意图

**文件：**
- 修改：`ZzFluentUI/widgets/src/private/ZzWorkspaceCrossTransferTransactionPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzWorkspaceCrossTransferTransactionPrivate.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzWorkspaceCrossTransferTest.cpp`
- 修改：`ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp`

- [ ] **步骤 1：编写失败的五区与撕出测试**

用两个顶层 QWidget 分别承载工作区，依次将页面拖到目标 `Center/Left/Top/Right/Bottom`，断言中心不增组、边缘只增一组、RTL 左右仍按物理方向、页面指针/id/key 不变。通过调用现有 TabBar `tearOffRequested` 验证工作区提升信号参数：来源组、索引、页面 id、全局坐标和非空推荐尺寸。

- [ ] **步骤 2：运行测试确认边缘跨实例失败**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.workspace-cross-transfer$' --output-on-failure
```

预期：中心通过，跨实例边缘 drop 和 `tabTearOffRequested` 断言失败。

- [ ] **步骤 3：实现边缘事务和工作区级信号**

边缘转移先在目标树创建临时组，再调用中心原语；目标达到 64 组或 16 层时在修改来源前返回 `CapacityExceeded` 对应的 `InvalidState`。提交后按 `groupAdded → layoutChanged → activeGroupChanged → tabTransferCommitted/tabDropCommitted` 顺序发信号。`prepareTabs()` 把每个组的 `ZzTabWidget::tearOffRequested` 映射为工作区信号。

- [ ] **步骤 4：运行完整工作区拖放回归**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest ZzSplitWorkspaceTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.(workspace-cross-transfer|split-workspace)$' --output-on-failure
```

预期：五区、RTL、容量、令牌和回滚用例全部通过。

- [ ] **步骤 5：提交**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests/ZzWorkspaceCrossTransferTest.cpp ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp
git diff --cached --check
git commit -m "feat(工作区): 完成跨窗口五区拖放" -m "将跨实例中心迁移扩展到四边分屏，并提升包含页面身份和推荐几何的工作区撕出意图。"
```

## P0-B：窗口协调、撕出和回收

### 任务 5：定义窗口配置、创建选项和观察句柄

**文件：**
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowConfiguration.h`
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCreateOptions.h`
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowHandle.h`
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowFactory.h`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp`

- [ ] **步骤 1：编写失败的值对象契约测试**

测试默认配置、字段级 Patch、来源枚举、空/有效句柄、复制移动和元类型。配置字段固定为标题、图标、`ZzWorkspaceTitleMode`、`ZzWindowClosePolicy`、置顶、最小/最大尺寸和初始几何；Patch 对每项使用 `std::optional`。

- [ ] **步骤 2：运行测试确认头文件缺失**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspacePublicApiTest --parallel 2
```

预期：新增 include 无法找到。

- [ ] **步骤 3：实现值对象并冻结工厂签名**

```cpp
enum class ZzWindowClosePolicy : std::uint8_t
{
    Allow,
    Deny,
    Delegate
};

enum class ZzWorkspaceConfigurationSource : std::uint8_t
{
    CoordinatorDefaults,
    SourceWindow,
    Explicit
};

enum class ZzApplicationWindowVisibility : std::uint8_t
{
    Visible,
    Deferred
};

struct ZzWorkspaceWindowConfiguration final
{
    QString title;
    QIcon icon;
    ZzWorkspaceTitleMode titleMode = ZzWorkspaceTitleMode::Application;
    ZzWindowClosePolicy closePolicy = ZzWindowClosePolicy::Allow;
    bool alwaysOnTop = false;
    QSize minimumSize;
    QSize maximumSize;
    QRect initialGeometry;
};

struct ZzWorkspaceWindowConfigurationPatch final
{
    std::optional<QString> title;
    std::optional<QIcon> icon;
    std::optional<ZzWorkspaceTitleMode> titleMode;
    std::optional<ZzWindowClosePolicy> closePolicy;
    std::optional<bool> alwaysOnTop;
    std::optional<QSize> minimumSize;
    std::optional<QSize> maximumSize;
    std::optional<QRect> initialGeometry;
};

struct ZzWorkspaceWindowCreateOptions final
{
    ZzWorkspaceConfigurationSource configurationSource =
        ZzWorkspaceConfigurationSource::CoordinatorDefaults;
    QPointer<ZzApplicationWindow> sourceWindow;
    ZzWorkspaceWindowConfigurationPatch configuration;
    ZzApplicationWindowVisibility visibility =
        ZzApplicationWindowVisibility::Visible;
    bool activate = true;
};

struct ZzWorkspaceWindowHandle final
{
    QPointer<ZzApplicationWindow> window;
    QPointer<ZzWorkspaceShell> shell;
    [[nodiscard]] bool isValid() const noexcept;
};

using ZzWorkspaceWindowFactory = std::function<
    ZzCore::ZzResult<ZzWorkspaceWindowHandle>(
        const ZzWorkspaceWindowCreateOptions &)>;

using ZzWorkspacePageResolver = std::function<
    ZzCore::ZzResult<std::unique_ptr<QWidget>>(QStringView)>;
```

公共头显式包含 `QIcon`、`QPointer`、`QRect`、`QSize`、`QStringView`、`<functional>`、`<memory>` 和 `<optional>`，并前向声明 `ZzApplicationWindow`、`ZzWorkspaceShell`、`QWidget`。所有值类型在工厂登记和 `applyConfiguration` 前校验非负尺寸、min/max 关系、窗口与 Shell 同线程且 Shell 的 `workspaceWidget()` 属于 Window；无效值返回 `InvalidArgument`。`ZzWorkspaceWindowHandle` 只保存两个 `QPointer` 观察值，不删除对象。轻量值不使用 Pimpl，协调器对象仍使用四文件 Pimpl。公共头不能把 `ZzFluentUI` 作为 `ZzPureTools` 的链接接口泄漏给消费者。任务 5 不创建上述值类型的 `.cpp` 文件；只有包含实质逻辑的协调器和编解码器使用 `.cpp`。

- [ ] **步骤 4：构建公共头测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspacePublicApiTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\.workspace-public-api$' --output-on-failure
git add ZzPureTools/widgets ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp ZzPureTools/CMakeLists.txt
git diff --cached --check
git commit -m "feat(窗口): 定义多窗口配置契约" -m "新增解析配置、字段 Patch、创建选项、观察句柄和应用装配工厂类型，不改变现有窗口所有权。"
```

### 任务 6：增加延迟显示窗口和协调器登记骨架

**文件：**
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- 创建：`ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- 创建：`ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzPureApplication.h`
- 修改：`ZzPureTools/widgets/src/ZzPureApplication.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/CMakeLists.txt`
- 修改：`ZzPureTools/tests/ZzApplicationBuilderTest.cpp`

- [ ] **步骤 1：编写失败的应用与登记测试**

覆盖无参 `createWindow()` 仍显示、新 `createWindow(Deferred)` 已被应用拥有但不可见、协调器每个应用唯一、首窗显式登记、重复 Window/Shell、跨线程和错误宿主拒绝、对象销毁自动注销、主窗口只能有一个。

- [ ] **步骤 2：运行测试确认协调器 API 缺失**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzApplicationBuilderTest --parallel 2
```

预期：`workspaceWindowCoordinator()`、`registerWindow()` 和 `ZzApplicationWindowVisibility` 未定义。

- [ ] **步骤 3：实现延迟显示但保持单一所有权**

在 `ZzPureApplicationPrivate::adoptWindow` 增加可见性参数；`Visible` 保持现有 `show()`，`Deferred` 只接管并连接关闭协议。无参公共重载委托 `Visible`。`ZzPureApplicationPrivate` 的 `coordinator` 成员声明在 `windows` 之后，使 C++ 逆序析构时协调器先于窗口释放；`beginShutdown()` 先把协调器设为 shutdown、断开工作区信号，再清理窗口。

公共接口固定为：

```cpp
[[nodiscard]] ZzCore::ZzResult<ZzApplicationWindow *> createWindow(
    ZzApplicationWindowVisibility visibility);
[[nodiscard]] ZzWorkspaceWindowCoordinator *
workspaceWindowCoordinator() const noexcept;
```

无参 `createWindow()` 保持原签名并等价于 `createWindow(Visible)`；`Deferred` 窗口在返回前已经完成 WindowKit、导航和应用 `ZzWindowSetupCallback` 装配，但不调用 `show()`。

- [ ] **步骤 4：实现协调器登记表**

协调器构造函数私有并由 `ZzPureApplicationPrivate` 独占。`registerWindow(handle, configuration, primary)` 验证 GUI 线程、Shell 的 `workspaceWidget()` 属于 Window、唯一绑定和配置；连接两端 destroyed 信号并安装事件过滤器。`unregisterWindow()` 只清理记录。本任务只提交登记、配置快照和生命周期连接，协调器的 `Q_OBJECT` 头加入 `zz_pure_tools_moc_headers`；撕出、关闭和拓扑方法在后续任务首次加入，避免公共 API 先暴露不可用入口。

登记骨架公开方法固定为：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> registerWindow(
    const ZzWorkspaceWindowHandle &handle,
    const ZzWorkspaceWindowConfiguration &configuration,
    bool primary = false);
[[nodiscard]] ZzCore::ZzResult<void> unregisterWindow(
    ZzApplicationWindow *window);
[[nodiscard]] ZzCore::ZzResult<ZzWorkspaceWindowConfiguration>
configuration(ZzApplicationWindow *window) const;
```

- [ ] **步骤 5：运行应用和窗口生命周期回归**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzApplicationBuilderTest ZzMultiWindowIsolationTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\.(workspace-window-coordinator|application-builder|multi-window)' --output-on-failure
```

预期：既有窗口 close token 和 queued erase 语义不变。

- [ ] **步骤 6：提交**

```bash
git add ZzPureTools/widgets ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp ZzPureTools/tests/ZzApplicationBuilderTest.cpp ZzPureTools/tests/CMakeLists.txt ZzPureTools/CMakeLists.txt
git diff --cached --check
git commit -m "feat(窗口): 建立工作区窗口协调器" -m "增加延迟显示窗口创建和显式 Window/Shell 登记，保持 ZzPureApplication 的唯一窗口所有权。"
```

### 任务 7：实现窗口创建、配置继承和撕出事务

**文件：**
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- 修改：`ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`

- [ ] **步骤 1：编写失败的创建、继承和撕出测试**

测试工厂未设置、工厂失败、无效 handle、默认配置、来源窗口快照、显式字段优先级，以及继承后修改来源不影响新窗口。撕出断言新窗口在迁移提交前不可见，提交后同一页面指针位于新 Shell；工厂或迁移失败时暂存窗口关闭、来源状态不变。

- [ ] **步骤 2：运行测试确认 create/tearOff 尚不支持**

```bash
ctest --preset linux-gcc-debug -R '^puretools\.workspace-window-coordinator$' --output-on-failure
```

预期：测试编译阶段报告 `createWindow(options)` 与 `tearOff(...)` 尚未提供。

本任务加入并冻结以下公共入口：

```cpp
void setWindowFactory(ZzWorkspaceWindowFactory factory);
[[nodiscard]] ZzCore::ZzResult<void> applyConfiguration(
    ZzApplicationWindow *window,
    const ZzWorkspaceWindowConfigurationPatch &patch);
[[nodiscard]] ZzCore::ZzResult<ZzWorkspaceWindowHandle> createWindow(
    const ZzWorkspaceWindowCreateOptions &options = {});
[[nodiscard]] ZzCore::ZzResult<void> tearOff(
    ZzFluentUI::ZzSplitWorkspace *sourceWorkspace,
    const ZzFluentUI::ZzTabGroupId &sourceGroup,
    int sourceIndex,
    const ZzWorkspaceWindowCreateOptions &options = {});
```

- [ ] **步骤 3：实现字段合并和独立配置应用**

按“显式 Patch > 来源配置快照 > 协调器默认值”生成完整配置。`applyConfiguration()` 同步 Window 标题/图标/尺寸、Shell 标题模式和 `setAlwaysOnTop()`；失败时把已应用字段恢复为旧配置，其他窗口完全不变。

- [ ] **步骤 4：实现撕出事务**

调用工厂时强制 `Deferred`；验证返回 handle 后登记暂存窗口，迁移到其工作区首组，应用配置，提交后才 `show()`/`raise()`/`activateWindow()`。连接每个已登记工作区的 `tabTearOffRequested` 到该流程。撕出默认几何以鼠标点和推荐尺寸计算，并通过当前屏幕 `availableGeometry()` 收敛。

- [ ] **步骤 5：运行定向测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzWorkspaceCrossTransferTest --parallel 2
ctest --preset linux-gcc-debug -R '^(puretools\.workspace-window-coordinator|fluent\.workspace-cross-transfer)$' --output-on-failure
git add ZzPureTools/widgets ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp
git diff --cached --check
git commit -m "feat(窗口): 支持配置继承与标签撕出" -m "按字段解析窗口配置，通过不可见暂存窗口迁移同一页面，事务成功后再显示并保持窗口相互独立。"
```

### 任务 8：实现关闭策略、来源栈和事务回收

**文件：**
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- 修改：`ZzPureTools/tests/ZzMultiWindowIsolationTest.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`（增加协调器对 `ZzLog::ZzLog` 的私有链接）

- [ ] **步骤 1：编写失败的关闭矩阵测试**

覆盖 `Allow`、`Deny`、`Delegate`、重复 Delegate 只发一次、`approveDelegatedClose`、A→B→C 依次回收、拖回来源时截断栈、原组不存在回活动组、来源窗口已销毁回主窗/首个存活窗、无目标发 `orphanedPages` 且窗口保持、第二个页面迁移失败时第一个回滚、`beginShutdown()` 不回收。

本任务加入并冻结以下入口：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> closeWindow(
    ZzApplicationWindow *window);
[[nodiscard]] ZzCore::ZzResult<void> approveDelegatedClose(
    ZzApplicationWindow *window);
```

- [ ] **步骤 2：运行测试确认系统关闭仍绕过策略**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzMultiWindowIsolationTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\.(workspace-window-coordinator|multi-window)' --output-on-failure
```

预期：Deny/Delegate、来源栈和批量回滚用例失败。

- [ ] **步骤 3：实现来源栈和关闭事件过滤**

监听 `tabTransferCommitted`，每页保存 `{windowId, groupId, index}` 栈；转入栈中已有窗口时截断已返回路径。系统 `QEvent::Close` 按策略处理：Deny ignore；Delegate ignore 并设置 pending；Allow 同步执行回收。批准接口只消费一次 pending token，不能绕过 Deny。

- [ ] **步骤 4：实现批量回收事务**

先计算全部目标并预检键冲突/容量，再按页迁移；失败时逆序搬回原窗口、组和索引。无目标时只发 `orphanedPages`，不删页。全部成功后设置一次内部 close bypass，让既有 `ZzApplicationWindow::closeAccepted`/应用 queued erase 完成销毁。

协调器私有实现使用现有 `ZzLog::writeText` 写入 `window.create`、`window.tear_off`、`page.transfer`、`page.reclaim`、`layout.restore` 五类事件；事件正文只包含稳定 id、阶段、耗时和结果。日志未初始化时不阻塞事务，也不创建临时 sink；`ZzPureTools` 对 `ZzLog::ZzLog` 只做 PRIVATE 直接链接，不能从公共接口泄漏。

- [ ] **步骤 5：运行测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzMultiWindowIsolationTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\.(workspace-window-coordinator|multi-window)' --output-on-failure
git add ZzPureTools/widgets ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp ZzPureTools/tests/ZzMultiWindowIsolationTest.cpp
git diff --cached --check
git commit -m "feat(窗口): 增加关闭策略与页面回收" -m "实现 Allow/Deny/Delegate 关闭协议、页面来源栈和多页原子回收，应用退出时跳过普通回收。"
```

## P1：组生命周期与活动页聚合

### 任务 9：完成空组策略和窗口级活动状态

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 修改：`ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`

- [ ] **步骤 1：编写失败的策略与聚合测试**

关闭/转移最后一页后分别验证 `Keep`、`Remove`、`RemoveUnlessLast`；最后一个组始终保留。切换组、标签、修改/attention 状态时验证 `activePageChanged` 和 `pageActivityChanged` 只在真实变化时发一次，协调器信号携带正确窗口和页面 id。

- [ ] **步骤 2：运行测试确认普通关页不会自动处理空组**

```bash
cmake --build --preset linux-gcc-debug --target ZzSplitWorkspaceTest ZzWorkspaceWindowCoordinatorTest --parallel 2
ctest --preset linux-gcc-debug -R '^(fluent\.split-workspace|puretools\.workspace-window-coordinator)$' --output-on-failure
```

预期：策略和窗口级聚合断言失败。

- [ ] **步骤 3：实现策略和连接重绑定**

`prepareTabs()` 观察页移除、currentChanged、modified 和 attention 信号；空组仅在当前事务提交后处理，最后一组不删除。活动组变化时断开旧组连接并绑定新组，页面销毁时先发空活动页再清理 id。

- [ ] **步骤 4：实现协调器聚合并回归**

协调器把每个已登记工作区的活动/状态信号加上窗口身份后转发；注销窗口时断开全部连接。运行：

```bash
ctest --preset linux-gcc-debug -R '^(fluent\.split-workspace|puretools\.workspace-window-coordinator)$' --output-on-failure
```

预期：全部通过，无重复信号和延迟对象增长。

- [ ] **步骤 5：提交**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp ZzPureTools/widgets ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp
git diff --cached --check
git commit -m "feat(工作区): 聚合空组与活动页面状态" -m "增加可配置空组回收，并把活动页、修改和注意状态可靠提升到窗口协调器。"
```

## P2：菜单、视觉聚合与拓扑持久化

### 任务 10：增加标签菜单 provider 和 attention 组聚合

**文件：**
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzTabContextMenuProvider.h`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzTabWidget.h`
- 修改：`ZzFluentUI/widgets/src/ZzTabWidget.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.cpp`
- 修改：`ZzFluentUI/widgets/src/ZzTabBar.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzTabControlsTest.cpp`
- 修改：`ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`

- [ ] **步骤 1：编写失败的菜单与 attention 测试**

测试 provider 收到同一个临时 `QMenu &`、正确 index/page，可追加动作、清空内建动作且不取得菜单所有权；provider 为空时内建“新建/关闭其他/关闭右侧”保持。测试 `attentionGroupIds()` 稳定树序、`groupAttentionChanged(id, bool)` 幂等，以及跨工作区迁移后来源和目标聚合更新。

- [ ] **步骤 2：运行测试确认 provider 和组查询缺失**

```bash
cmake --build --preset linux-gcc-debug --target ZzTabControlsTest ZzSplitWorkspaceTest --parallel 2
```

预期：新 API 编译失败。

- [ ] **步骤 3：实现同步菜单扩展和组状态**

```cpp
using ZzTabContextMenuProvider = std::function<void(
    QMenu &menu, int index, QWidget *page)>;
```

将菜单构建从 TabBar 移到 TabWidget：先添加内建动作，再同步调用 provider，最后仅在菜单非空时 popup。工作区按各组标签元数据计算 attention，不创建每页 QObject，也不轮询；Tab 默认视觉继续由现有 Fluent 标签绘制消费 attention 状态。

- [ ] **步骤 4：运行测试并提交**

```bash
ctest --preset linux-gcc-debug -R '^fluent\.(tab-controls|split-workspace)$' --output-on-failure
git add ZzFluentUI/widgets ZzFluentUI/tests/ZzTabControlsTest.cpp ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp ZzFluentUI/CMakeLists.txt
git diff --cached --check
git commit -m "feat(标签): 增加菜单扩展与注意状态聚合" -m "允许应用同步扩展标签菜单，并提供工作区级 attention 组查询和变化信号。"
```

### 任务 11：实现有界多窗口拓扑状态与编解码器

**文件：**
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceTopologyStatePrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceTopologyStatePrivate.cpp`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceTopologyCodecPrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceTopologyCodecPrivate.cpp`
- 创建：`ZzPureTools/tests/ZzWorkspaceTopologyCodecPrivateTest.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/CMakeLists.txt`

- [ ] **步骤 1：编写失败的纯值 codec 测试**

构造双窗口、五组、多个页面及来源栈状态，验证稳定编码、往返相等和 SHA-256。逐项篡改 magic、schema、流版本、长度、摘要、窗口/页面计数、UUID、重复 windowId、重复 pageId/layoutKey、负几何、超长字符串和截断 payload，断言完整拒绝且无 QWidget 创建。

- [ ] **步骤 2：运行测试确认 codec 文件不存在**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTopologyCodecPrivateTest --parallel 2
```

预期：构建失败，提示私有头不存在。

- [ ] **步骤 3：实现 `ZZWT` schema v2**

使用 `QDataStream::Qt_6_8`、magic `ZZWT`、schema `2`、SHA-256。硬上限固定为 32 窗口、全拓扑 4096 页面、每工作区 64 组/16 层、字符串 256 UTF-16 code unit、单个 `ZZSW` 1 MiB、总 payload 16 MiB、来源栈深度 32。先解码到无 QObject 的 DTO，完成所有交叉引用检查后返回成功。

- [ ] **步骤 4：实现 `ZZSW` v1 包装读取**

检测到 `ZZSW` magic 时保留原字节作为一个默认窗口的 workspaceState，窗口配置使用协调器默认值，页面归属由其 layoutKey 在恢复阶段解析；不得修改现有 `ZzSplitWorkspace::saveLayout/restoreLayout` 格式。

- [ ] **步骤 5：运行 codec 测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTopologyCodecPrivateTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\.workspace-topology-codec-private$' --output-on-failure
git add ZzPureTools/widgets/src/private/ZzWorkspaceTopology* ZzPureTools/tests/ZzWorkspaceTopologyCodecPrivateTest.cpp ZzPureTools/tests/CMakeLists.txt ZzPureTools/CMakeLists.txt
git diff --cached --check
git commit -m "feat(布局): 增加多窗口拓扑编解码" -m "新增有界 ZZWT v2 状态、摘要与交叉引用校验，并兼容读取现有 ZZSW v1 单窗口布局。"
```

### 任务 12：连接拓扑保存恢复和跨屏几何收敛

**文件：**
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp`

- [ ] **步骤 1：编写失败的拓扑集成测试**

保存两个窗口、不同配置/几何/分屏/活动组/页面顺序和来源栈；在空协调器中用 page resolver 返回无父页面，验证完整恢复。增加 resolver 失败、重复页面、返回有父对象、第二窗工厂失败、已有业务页面拒绝恢复、缺失屏幕回主屏和完全离屏几何收敛用例。

本任务加入并冻结以下入口：

```cpp
[[nodiscard]] ZzCore::ZzResult<QByteArray> saveTopology() const;
[[nodiscard]] ZzCore::ZzResult<void> restoreTopology(
    const QByteArray &state,
    const ZzWorkspacePageResolver &pageResolver);
```

- [ ] **步骤 2：运行测试确认拓扑入口尚未提供**

```bash
ctest --preset linux-gcc-debug -R '^puretools\.workspace-window-coordinator$' --output-on-failure
```

预期：测试编译阶段报告 `saveTopology()` 与 `restoreTopology()` 尚未提供。

- [ ] **步骤 3：实现保存和离屏恢复事务**

保存时按窗口登记顺序生成 DTO，每窗口嵌入现有 `splitWorkspace()->saveLayout()`，页面只持久化非空且全局唯一的 layoutKey。恢复要求当前无业务页面：完整解码 → 工厂创建全部 Deferred 窗口 → resolver 创建全部页面 → 恢复每棵树 → 应用配置/几何 → 一次提交显示。任一步失败关闭全部暂存窗口，原空拓扑不变。

- [ ] **步骤 4：实现屏幕映射**

按 `QScreen::name()` 选择目标屏幕；缺失时用主屏。使用逻辑坐标与 `availableGeometry()` 限制窗口，至少保留完整标题栏和 160×120 内容区可见；不恢复历史物理 DPI 像素。

- [ ] **步骤 5：运行测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzWorkspacePublicApiTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\.(workspace-window-coordinator|workspace-public-api)$' --output-on-failure
git add ZzPureTools/widgets ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp
git diff --cached --check
git commit -m "feat(布局): 支持多窗口拓扑保存恢复" -m "连接窗口工厂、页面解析器、工作区布局和跨屏几何收敛，以离屏事务恢复完整窗口拓扑。"
```

### 任务 13：在 Example 中串联多窗口公共能力

**文件：**
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShell.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShell.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/main.cpp`
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`

- [ ] **步骤 1：编写失败的 Example smoke 场景**

场景创建终端标签，调用协调器撕出，验证窗口数从 1 到 2、两个 Shell 独立、页面指针不变；修改第二窗标题/关闭策略不影响首窗；关闭第二窗后页面回首窗；触发无目标 orphaned 路径时不销毁页面。

- [ ] **步骤 2：运行 smoke test 确认 Example 未注册协调器**

```bash
cmake --build --preset linux-gcc-debug --target ZzExampleWorkspaceSmokeTest --parallel 2
ctest --preset linux-gcc-debug -R '^example\.workspace-smoke$' --output-on-failure
```

预期：窗口工厂缺失或初始 Window/Shell 未登记。

- [ ] **步骤 3：公开 Shell 观察接口并注册工厂**

给 `ZzExampleWindowShell` 增加 `workspaceShell() const noexcept`。Builder 成功后将首窗 handle 注册为 primary；工厂使用 `application.createWindow(Deferred)`，再通过 `ZzExampleWindowShell::attachedTo()` 取得 Shell 并返回 handle。终端页面继续由 Example 创建，协调器不读取 session model。

- [ ] **步骤 4：运行 Example 定向验证并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2
ctest --preset linux-gcc-debug -R '^example\.workspace-smoke$' --output-on-failure
git add examples/ZzPureToolsExample
git diff --cached --check
git commit -m "feat(示例): 串联多窗口撕出与回收" -m "Example 只通过公共协调器登记窗口工厂，演示页面撕出、独立配置和关闭回收，不承载库内事务逻辑。"
```

### 任务 14：增加性能阈值、文档和全量验证

**文件：**
- 修改：`benchmarks/ZzWorkspaceComponentsBenchmark.cpp`
- 修改：`benchmarks/CMakeLists.txt`（为跨工作区两个指标注册独立门禁）
- 修改：`docs/development/WORKSPACE_API_ZH.md`
- 修改：`docs/development/FRAMEWORK_INTEGRATION_ZH.md`
- 修改：`docs/development/BUILDING_ZH.md`
- 修改：`README.md`
- 修改：`docs/superpowers/plans/2026-09-02-multi-window-workspace.md`（仅追加实际验证证据）

- [ ] **步骤 1：增加迁移性能和对象预算采样**

在现有 workspace benchmark 创建两个工作区、32 个标签，预热后往返迁移 500 次，报告 `workspace-cross-transfer-time`（ms）和 `workspace-cross-transfer-object-growth`（count）。每次迁移验证同一页面、id/key 和总页数，稳定阶段不允许新增 QObject/QTimer/QAbstractAnimation。

- [ ] **步骤 2：运行三轮 benchmark 并冻结阈值**

```bash
cmake --preset linux-gcc-benchmarks -DZZ_BUILD_TESTS=ON -DZZ_BUILD_BENCHMARKS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-benchmarks --target ZzWorkspaceComponentsBenchmark --parallel 2
for round in 1 2 3; do
  ctest --preset linux-gcc-benchmarks -R '^benchmark\.workspace-components$' --output-on-failure
done
```

要求三轮对象增长均为 0；迁移 p95 不高于 4 ms。若实测超过 4 ms，先优化热路径并重跑，不得直接放宽阈值。`benchmarks/CMakeLists.txt` 为 `workspace-cross-transfer-time` 和 `workspace-cross-transfer-object-growth` 分别调用现有 `zz_add_reference_gate`，对象增长使用允许等于零的 `MAX_VALUE 0` deterministic gate；任何正增长仍必须失败。

- [ ] **步骤 3：补充中文集成文档**

文档给出创建协调器、登记首窗、设置工厂、程序化迁移、关闭 Delegate、保存/恢复的可编译示例；明确 GUI 线程、所有权、空 layoutKey、不跨进程和配置只在创建时继承。README 组件表只增加文档链接，不复制长说明。

- [ ] **步骤 4：运行 Linux 全量构建与测试**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug --output-on-failure
```

预期：全量 CTest 通过；若存在与本功能无关的既有失败，保存准确名称和日志，不把它写成通过。

- [ ] **步骤 5：运行静态、安装消费和平台契约检查**

```bash
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
cmake --preset linux-clang-tidy-release
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy --parallel 2
ctest --preset linux-gcc-debug -R '^(architecture\.public-headers|architecture\.complete-audit|install\.consumer)$' --output-on-failure
cmake -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" -P tests/Platform/PresetMatrixContract.cmake
```

Windows MSVC、Windows MinGW 和 macOS 本轮只记录源码/公共头/preset 静态边界；未在对应机器执行时不得宣称原生通过。

- [ ] **步骤 6：记录证据并提交**

```bash
git diff --check
git status --short
git add benchmarks docs README.md
git diff --cached --check
git commit -m "docs(工作区): 完成多窗口验收与性能门禁" -m "记录 Linux 全量、clang-tidy、安装消费和三轮迁移性能证据，并补充多窗口公共 API 集成说明。"
```

## 完成标准

- 同一 QWidget 可在两个工作区五区双向迁移，页面 id、layoutKey 和全部标签元数据不丢失；
- MIME 不含 QObject 指针，伪造、过期、重放和来源销毁令牌全部拒绝；
- 撕出窗口在事务提交前不可见，每个窗口配置、布局、活动页和关闭策略独立；
- 关闭撕出窗口按来源栈回收，多页面任一失败时回滚，无目标不销毁页面；
- `ZZWT` v2 双窗口拓扑可保存恢复，`ZZSW` v1 可作为单窗口导入，恶意输入有界拒绝；
- Example 只调用公共组件；库内没有 SSH、终端或其他业务模型依赖；
- Linux 全量测试、静态检查和性能阈值有新鲜证据，Windows/macOS 验证边界如实记录；
- 未读取、修改、暂存或提交顶层 `multi-window-requirements.md` 和 `temp_image/`。

## 实际验收证据（2026-09-07）

- Linux GCC Debug 在 Qt 6.11.1、GNU 15.2.0 上完成全量构建；提交
  `ef9c715` 的 CTest 为 235/235 通过，总耗时 485.05 秒，其中
  `install.consumer` 与 `platform.package-relocation` 均通过。
- Clang 20 shared/static 全量静态分析在 `9f02735` 上分别为 294/294
  通过；`ef9c715` 新增的协调器生产与测试翻译单元又分别在 shared/static
  编译数据库下完成定向分析，未产生项目级诊断。
- `9f02735` 的三轮 Release/shared/LTO 迁移采样均使用 500 次往返：迁移
  p95 分别为 0.074089、0.072904、0.083369 ms，max 分别为
  0.122809、0.124812、0.131453 ms，对象增长 max 均为 0。耗时门禁
  `p95 <= 4 ms` 与对象门禁 `max <= 0` 均通过，正增长输入仍被拒绝。
- 本次性能采样使用 `QT_QPA_PLATFORM=offscreen`；本机 XCB 环境缺少
  `libxcb-cursor`，因此这些数据是功能性能证据，不替代正式 Xvfb/xcb
  参考基线。
- preset 矩阵、公共头、完整架构、二进制依赖、Linux 打包和发布契约均
  通过。Windows MSVC、Windows MinGW 与 macOS 仅完成 preset、源码、
  公共头和打包契约静态验证，未声明原生构建通过。
- 分支差异不包含顶层 `multi-window-requirements.md` 或 `temp_image/`。
