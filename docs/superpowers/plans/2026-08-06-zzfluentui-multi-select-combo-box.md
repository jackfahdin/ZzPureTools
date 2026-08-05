# ZzFluentUI 多选组合框实施计划

**目标：** 提供一个以稳定 key 管理本地值语义选项、支持鼠标和键盘多选、在关闭面板中展示选择摘要的 `ZzMultiSelectComboBox`，完整复用现有 Fluent 组合框、popup 与 item delegate 外观，并消除旧版固定选择位图、多状态源、Qt 内部 popup 操作和交互期动画风险。

**架构：** `ZzMultiSelectComboBox` 继承 `QComboBox` 以复用公开 popup 定位、焦点、窗口关闭和 ComboBox 无障碍角色。私有 `QAbstractListModel` 唯一保存 `QList<ZzMultiSelectOption>`；`selected` 字段通过 `Qt::CheckStateRole` 暴露，标准 `QListView` 与 `ZzFluentItemDelegate` 展示复选状态。关闭面板使用一个无 frame、只读、不可聚焦的内部 `QLineEdit` 展示派生摘要，使 `QComboBox::currentText()` 和标准无障碍 Value 保持同步，但不得成为第二份选择真值。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

## 1. 范围与前置结论

- 本批继续总体设计阶段 10，只实现本地平面选项的多选组合框；tag/chip 编辑、分组树、远程检索、异步分页、用户自定义输入、业务历史和持久化留给后续独立批次。
- 标准单选继续使用 `QComboBox`，搜索建议继续使用 `ZzSuggestBox`。调用方必须显式选择 `ZzMultiSelectComboBox`，不得让应用级 style 改变全部 `QComboBox` 的单选语义。
- 组件只接收展示快照，不 include 或调用 repository、database、network、domain、service、presenter 或 application 模块。
- 模型归组件所有，不接受外部 model/view/delegate 替换。该约束换取唯一 key、选择一致性、批量信号和安装 ABI 的可证明边界；需要外部动态模型时应另立基于代理模型的计划。
- popup 继续由 `QComboBox` 创建、定位和关闭；本批不查找 Qt 私有 container、不修改 window flags/layout/mask、不模拟父窗口鼠标事件、不全局修改 `Qt::UI_AnimateCombo`。
- 继续复用本机 `/home/zz/Qt/6.11.1/gcc_64`，不下载 Qt，不访问 GitHub CLI、不读取远端 CI、不 push。

## 2. 旧版逐行代码审计

旧版以下六个文件只用于核对产品意图：

```text
ZzMultiSelectComboBox.h
ZzMultiSelectComboBox.cpp
private/ZzMultiSelectComboBoxPrivate.h
private/ZzMultiSelectComboBoxPrivate.cpp
DeveloperComponents/ZzComboBoxView.h
DeveloperComponents/ZzComboBoxView.cpp
```

明确不迁移的实现如下：

- 公开头依赖 `ZzProperty.h` 宏并暴露 `BorderRadius`，把统一主题尺寸变成每实例状态；getter 返回内部 `QStringList` 引用，ABI 和生命周期契约不清晰。
- 构造阶段固定高度 35，为每实例创建 `ZzComboBoxStyle`、两层 `ZzScrollBar`、自定义 view 和主题单例连接；析构又手工删除 `style()`，混合 QObject 与 style 所有权。
- `_itemSelection` 固定 resize 为 32，所有字符串/index setter 和点击槽都先按 model row 写入、后调用扩容函数；第 33 行开始发生 `QVector<bool>` 越界写。
- 初始代码无条件把 `_itemSelection[0]` 设为 true，即使模型为空；后续 model insert/remove/reset 没有同步缩容、迁移或清理选择。
- 同一选择同时存在于 `_itemSelection`、`QItemSelectionModel`、逗号拼接 `_currentText` 和 split 后 `_selectedTextList` 四处，任何通知遗漏都会产生分叉。
- 选中文本先用 `","` 拼接再 split；选项本身包含逗号时会被拆成多个不存在的值，重复 text 也无法作为稳定身份。
- 所有 setter 按展示 text 或临时 row 匹配，没有稳定 key 和调用方 payload；同名项、排序、插删和本地化后无法可靠恢复选择。
- `_refreshCurrentIndexs()` 每次都遍历所有行并逐行调用 selection model；即使只有一项变化也执行 O(n) 选择写入，并可能触发额外信号。
- 自定义 view 在 mouse press 发信号后直接 ignore，不调用标准 view 输入；没有 Space/Enter、Up/Down、Home/End、Escape、类型搜索和辅助技术选择契约。
- `paintEvent()` 使用固定坐标、字体图标、手工主题色和宽度百分比绘制；忽略 style option、placeholder、字体度量、icon、DPI、RTL、高对比度与系统无障碍 palette。
- 展开和关闭分别为 container 高度、view 位置、箭头旋转和底部标记创建多条 `DeleteWhenStopped` 动画；快速点击会并发创建对象，旧回调可覆盖新状态。
- popup 通过 `findChild<QFrame *>()` 猜测 Qt 内部对象，清空并重装内部 layout；多处 `takeAt(0)` 丢弃 `QLayoutItem` 而不 delete。
- 关闭路径在检查 container 是否为空前先调用 `container->height()`；Qt 内部结构变化或替换 view 时可直接崩溃。
- 关闭时向 `parentWidget()` 发送坐标为 -1 的伪鼠标事件；无 parent 时接收者为空，多窗口焦点和输入语义也被篡改。
- 通过 cursor 是否位于 item、`underMouse()`、`_isFirstPopup` 和 `_isAllowHidePopup` 猜测关闭原因，不能可靠覆盖 Escape、Alt+Up、Tab、窗口失活、外部点击和辅助技术。

可保留的产品意图只有：多项选择、关闭面板摘要、popup 内复选状态、disabled item、批量程序化选择和选择变化信号。新实现全部建立在 Qt 公开 API 上。

## 3. 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzMultiSelectComboBox.h`：

```cpp
namespace ZzFluentUI {

struct ZZ_FLUENT_UI_EXPORT ZzMultiSelectOption final
{
    QString key;
    QString text;
    QIcon icon;
    QVariant data;
    bool enabled = true;
    bool selected = false;

    friend bool operator==(
        const ZzMultiSelectOption &,
        const ZzMultiSelectOption &) = default;
};

class ZZ_FLUENT_UI_EXPORT ZzMultiSelectComboBox final : public QComboBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzMultiSelectComboBox)
    Q_PROPERTY(int optionCount READ optionCount NOTIFY optionsChanged)
    Q_PROPERTY(int selectionCount READ selectionCount
                   NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedText READ selectedText
                   NOTIFY selectionChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText
                   WRITE setPlaceholderText)

public:
    enum ZzDataRole {
        KeyRole = Qt::UserRole + 1
    };
    Q_ENUM(ZzDataRole)

    explicit ZzMultiSelectComboBox(QWidget *parent = nullptr);
    ~ZzMultiSelectComboBox() override;

    void setOptions(QList<ZzMultiSelectOption> options);
    [[nodiscard]] QList<ZzMultiSelectOption> options() const;
    [[nodiscard]] int optionCount() const noexcept;

    [[nodiscard]] QString addOption(
        QString text,
        QVariant data = {},
        QIcon icon = {},
        bool selected = false);
    [[nodiscard]] QString addOption(ZzMultiSelectOption option);
    [[nodiscard]] bool removeOption(const QString &key);
    [[nodiscard]] bool removeOptionAt(int index);
    void clearOptions();

    [[nodiscard]] bool setOptionSelected(
        const QString &key,
        bool selected);
    [[nodiscard]] bool setOptionSelectedAt(int index, bool selected);
    void setSelectedKeys(QStringList keys);
    void setSelectedIndexes(QList<int> indexes);
    void selectAll();
    void clearSelection();

    [[nodiscard]] QList<ZzMultiSelectOption> selectedOptions() const;
    [[nodiscard]] QStringList selectedKeys() const;
    [[nodiscard]] QList<int> selectedIndexes() const;
    [[nodiscard]] int selectionCount() const noexcept;
    [[nodiscard]] QString selectedText() const;

    void setPlaceholderText(const QString &text);

Q_SIGNALS:
    void optionsChanged();
    void selectionChanged();
    void optionToggled(
        const ZzMultiSelectOption &option,
        bool selected);
};

} // namespace ZzFluentUI
```

公开契约：

- `ZzMultiSelectOption` 是隐式共享成员组成的值语义快照，不持有业务对象裸指针；全部字段和公开方法补充简体中文 Doxygen。
- 空 key 和重复 key 在写入时规范化为无花括号 UUID。相同 text 与包含逗号的 text 均合法，身份和选择只使用 key/row。
- `setOptions()` 对完全相同集合不发信号；有效替换最多发一次 `optionsChanged()` 和一次 `selectionChanged()`。add/remove/clear 同理，无变化不发信号。
- 负 index、`index == count` 和不存在 key 返回 false；bulk index 去重并忽略越界，bulk key 去重并忽略未知 key。
- 程序化 API 可以保留或设置 disabled option 的选择；`selectAll()` 和用户交互只选择 enabled option，disabled 已选项仍可由 clear/bulk API取消。
- `selectedOptions/Keys/Indexes` 按模型顺序返回，不按调用参数顺序返回。`selectedText` 直接以 `", "` 连接真实 text，不通过 split 反推选择。
- `optionToggled` 只表示用户通过鼠标或 Space/Enter 切换一行；程序化批量变化只发一次 `selectionChanged`，避免把初始化误当用户意图。
- `QComboBox` 的外部 model/view/delegate、editable/current-index 和逐项 mutation API 在派生类静态类型上设为 private，防止绕过 key 与信号契约；`maxVisibleItems`、size policy、font、palette、tooltip 和 accessible name 等展示 API继续可用。
- 默认 maximum visible items 为 8，不设置固定宽高；尺寸由 `ZzFluentStyle::CT_ComboBox`、字体和外部 layout 决定。

## 4. 私有实现

新增四文件结构：

```text
widgets/include/ZzFluentUI/ZzMultiSelectComboBox.h
widgets/src/ZzMultiSelectComboBox.cpp
widgets/src/private/ZzMultiSelectComboBoxPrivate.h
widgets/src/private/ZzMultiSelectComboBoxPrivate.cpp
```

### 4.1 唯一值模型

- private `.cpp` 定义 `ZzMultiSelectOptionModel final : public QAbstractListModel`，唯一保存 `QList<ZzMultiSelectOption>`；Pimpl 只保存非拥有的抽象 model/view/delegate 指针。
- `rowCount()` 对有效 parent 返回 0；`data()` 完整检查 index/row/column，映射 `DisplayRole/EditRole -> text`、`DecorationRole -> icon`、`UserRole -> data`、`KeyRole -> key`、`CheckStateRole -> Checked/Unchecked`。
- `flags()` 对 enabled 行返回 enabled/selectable/user-checkable，对 disabled 行不返回 enabled 和 user-checkable；model 不创建 item QObject 或 widget。
- `setData(CheckStateRole)` 只接受 Checked/Unchecked，对实际变化发单行 `dataChanged`；selection 批量方法在一次 O(n) 扫描后按变化范围发有限 `dataChanged`，不得逐行触发公开信号。
- reset 前先在栈上 `reserve()` 并规范化 key；insert/remove 使用 `beginInsertRows/endInsertRows` 和 `beginRemoveRows/endRemoveRows`。
- 选择 API 查询允许 O(n)，因为只发生在显式集合/选择操作；paint、sizeHint、hover 和 popup 滚动只访问当前 index 的 role。

### 4.2 关闭面板摘要

- 构造时调用一次 `QComboBox::setEditable(true)` 并取得 Qt 管理的 `QLineEdit`；line edit 设置 read-only、NoFocus、无 frame，不接受业务编辑或 completer。
- 每次选择实际变化后从模型顺序生成一次 `selectedText` 缓存，写入 line edit 并把 cursor 固定到起始端；绘制只消费缓存，不扫描模型。
- 无选择时 line edit 文本为空并展示同步后的 placeholder。公开 `setPlaceholderText()` 同时更新 QComboBox 与 line edit。
- QComboBox `currentIndex` 始终恢复为 -1，不能作为选择真值；内部 line edit 只保存派生展示文本，使标准 `currentText()` 与 QAccessible Value 可读。
- model reset、insert/remove 和选择变化后更新摘要；文本或 icon 随 options reset 更新。不存在外部 model mutation，因此不需要猜测第三方通知顺序。

### 4.3 popup 与输入

- 构造一次标准 `QListView` 和 `ZzFluentItemDelegate`；使用 `NoSelection`、`ScrollPerPixel`、`uniformItemSizes=true`、Standard 40px density 和 text elide。
- 已选状态来自 `CheckStateRole`，键盘 current index 只负责 focus ring；不得把 `QItemSelectionModel` 当第二份多选状态。
- 为保留多选 popup，只把公开控件自身作为固定 event filter 安装到 view 和 viewport。`showPopup()` 前 remove/install 同一 filter，利用 Qt 公开的“最后安装先执行”规则，不创建 helper QObject。
- viewport 左键 press 只更新 current index，release 只切换当前 enabled 行并消费事件；双击不得切换两次。滚动条、wheel 和 hover 继续交给标准 view。
- popup 内提前消费 Space、Enter、Return 的 ShortcutOverride，并在 press 切换 current enabled 行、release 结束短生命周期关闭保护；Up/Down/Home/End/PageUp/PageDown 和类型搜索交给 `QListView`；Escape、Tab、窗口失活和外部点击沿 QComboBox popup 关闭路径处理。
- 关闭状态下 Space、Enter、Return、F4、Alt+Down、Up/Down 打开 popup；普通字符不修改只读摘要。combo 本体 wheel 不改变选择，避免滚轮静默改业务值。
- line edit 上的鼠标 press 转为 combo 的 `showPopup()`；它本身不可聚焦、不可选择或编辑摘要。
- `hidePopup()` 先执行 QComboBox 标准关闭，再恢复 `currentIndex == -1` 和模型派生摘要；不检查 cursor 全局位置，不查找 QFrame，不修改 popup layout/window flags，不创建 timer/animation/QSS/动态属性。

### 4.4 生命周期与性能

- 每实例固定创建一个 model、Qt editable line edit、一个 list view 和一个 delegate；首次 popup 可能由 Qt 延迟创建 container，但预热后选择切换不增加对象。
- Pimpl 一次堆分配只在构造阶段发生。摘要 cache 只在选择变化时重建，不在 paint、layout、hover 或滚动热路径分配。
- 不创建 `QPropertyAnimation`、`QTimer`、QProxyStyle、QStandardItem、QAction、QSS、pixmap cache 或主题单例连接。
- 所有 GUI 状态只在 GUI 线程访问；Qt 事件边界不传播异常。

## 5. 自动测试

新增 `ZzFluentUI/tests/ZzMultiSelectComboBoxTest.cpp` 与 CTest `fluent.multi-select-combo-box`：

- 验证默认 option/selection count、8 个最大可见项、placeholder、StrongFocus、只读内部 editor 和最小 32px Fluent 尺寸。
- 验证 set/add/remove/removeAt/clear、空 key、重复 key、重复 text、逗号 text、icon、payload、enabled、selected 和 40 项以上集合；覆盖旧版第 33 行越界条件。
- 验证 setSelectedKeys/indexes 的去重、未知 key、负数、等于 count、顺序收敛、disabled 项、selectAll/clear 和无变化信号。
- 使用 `QAbstractItemModelTester` 验证 reset/insert/remove、有效 parent rowCount、invalid index、Display/Edit/Decoration/User/Key/CheckState role 与 flags。
- 验证批量操作最多一次 `selectionChanged`，用户鼠标/Space/Enter 每次产生一次 `optionToggled`；程序化操作不产生 user intent。
- 显示真实 popup，验证点击两项后 popup 仍打开、复选状态更新、Escape/Tab/外部点击关闭；键盘 Up/Down/Home/End/Page 键不直接改变选择。
- 验证 line edit 鼠标打开、combo wheel 不改变选择、currentIndex 保持 -1、selectedText/currentText/placeholder 同步，长文本从起始端显示。
- 覆盖 Light/Dark/HighContrast、LTR/RTL、disabled combo、disabled row、重复文本与图标。
- `QAccessible` 主控件保持 ComboBox role、name、派生 summary value、focusable/disabled 状态；popup row 使用标准 ListItem/checkable/checked/disabled 状态，不注册自定义 accessible interface。
- 预构造 100 个控件、每个 20 条 option，预热 popup 后执行 1000 轮单项/bulk/selectAll/clear/direction/enabled/placeholder 切换，恢复初值并处理 deferred delete；QObject、animation 和 timer 数量不增长。

扩展既有质量边界：

- 公开头逐头编译、shared/static public MOC、metatype 与 fresh install consumer 必须包含新类型。
- 架构审计继续禁止 UI include 业务/基础设施词缀、Qt Private、QWindowKit 或第三方实现头。
- 标准单选 `QComboBox` 测试继续通过，证明新组件没有改变应用级 style 或 Qt 默认单选语义。

## 6. 画廊与安装消费

- 在 `examples/ZzFluentControlsGallery` 输入区域加入真实 `ZzMultiSelectComboBox`，展示图标、重复 text、包含逗号 text、disabled item、预选项和稳定 key/data。
- 画廊把 `optionToggled`/`selectionChanged` 连接到纯展示 label，证明 UI 只输出意图和可查询快照，不访问业务服务。
- `tests/InstallConsumer/Gui/main.cpp` 从安装后的 `<ZzFluentUI/ZzMultiSelectComboBox.h>` 创建控件，验证 key 唯一、bulk 选择、summary、payload、model roles 和 style。
- 不在安装接口暴露 private model、event filter helper、popup container 或内部 editor 所有权。

## 7. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造 100 个多选组合框、每个 20 条 option；先打开/关闭一次 popup 完成 Qt 延迟对象预热，10 帧预热、120 帧正式渲染。
- 每帧只切换一个控件的一项选择并渲染到预分配 `QImage`；计时区不创建控件、model、delegate、option、popup、animation、timer 或图片。
- 当前活动 Linux 参考环境渲染 P95 `<= 16.7 ms`；普通环境只记录数据。
- 1000 轮选择、bulk、selectAll/clear、direction、enabled 与 placeholder 变化后，恢复状态并确保 descendants/animations/timers 与预热后一致。
- 单个 10000 项模型执行 100 轮“100 个 key 的 bulk 选择 + selectedKeys 查询”，记录总耗时和最终选择数；首次正式 reference 测量后再设置宽松绝对门限，不写未测猜测。
- paint/sizeHint/popup scroll 不得扫描总模型；一次选择变化最多执行一次 O(n) 摘要/keys 构建。

## 8. 视觉基线

扩展 `ZzFluentScreenshotTest`，新增固定尺寸 `multi-select-combo-box` surface：

- 覆盖 placeholder、单选、多选、包含逗号、重复 text、长摘要、disabled combo、disabled row、icon、popup open、checked/unchecked、keyboard current、LTR 和 RTL。
- popup 只通过 `combo.view()->window()` 与公开 view geometry 合成，不查找 Qt 私有类名、不修改 window flags。
- 建立 Light、Dark、HighContrast x DPR 1.0、1.25、1.5、2.0 共 12 张基线。
- 摘要和 popup item 文本纳入文字遮罩；panel、focus、arrow、check indicator、hover/current、disabled、icon、stroke 和 popup surface 参加严格比较。
- 人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认无双 frame、裁切、重叠、错误 RTL、摘要末端滚动或 popup 遮挡 closed panel。

## 9. 跨平台静态检查

- Windows MSVC、Windows Qt SDK MinGW 和 macOS 只使用 Qt Widgets/Core/Gui 公共 API、标准 C++20 与组件 private header；不增加平台分支。
- event filter 只判断 Qt 公共 event/key/button/type；坐标使用逻辑像素，popup 定位和 screen 约束全部委托平台插件。
- 运行 preset matrix、gate script contract、public headers、完整架构和 Fluent 边界审计；本机结果不得表述为 Windows/macOS 已编译或真机通过。

## 10. 提交顺序

```text
文档：规划Fluent多选组合框批次

审计旧版固定选择位图、多状态源、popup内部操作和动画风险。
确定值模型CheckStateRole、只读摘要与标准QComboBox popup契约。
```

```text
控件：实现Fluent多选组合框

新增ZzMultiSelectOption与ZzMultiSelectComboBox四文件实现。
以私有值模型作为唯一选择真值并保留标准popup关闭语义。
```

```text
测试：接入多选组合框质量与安装消费

补齐集合、选择、键鼠、无障碍、模型、对象稳定性和性能测试。
接入画廊、安装消费者、公开头与架构门禁。
```

```text
测试：补齐多选组合框多主题视觉基线

新增三主题、四档DPR的关闭摘要和复选popup参考图。
验证disabled、icon、long text、keyboard current和RTL状态。
```

```text
文档：记录多选组合框批次交付结果

记录Linux GCC、Clang、sanitizer、clang-tidy、截图、性能和安装消费结果。
明确Windows与macOS仅完成源码静态审计，远端CI继续暂缓。
```

## 11. 本机验证命令

使用现有环境：

```bash
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
```

每个代码提交运行对应 target 与 CTest；最终运行 GCC Release shared/static、Clang ASan+UBSan、shared/static `ZzClangTidy`、public headers、fresh install consumer、四档截图、reference benchmark 与画廊 smoke。

## 12. 完成定义

- `ZzMultiSelectComboBox` 安装接口完整，全部自定义类型使用 Zz 前缀和简体中文 Doxygen。
- 私有 option model 是唯一选择真值；摘要、check role 和用户信号均由它派生，没有固定容量或 text 反向解析。
- 鼠标和键盘可连续切换多项且 popup 保持打开；Escape、Tab、外部点击和窗口失活仍由 Qt 正确关闭。
- 每实例基础设施固定，没有自定义 popup、Qt 内部对象查找、QSS、timer、animation、动态属性或逐项 QObject。
- Light、Dark、HighContrast x 四档 DPR 视觉基线通过；参考机渲染 P95 满足 16.7ms，大集合门限由正式实测确定。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows MSVC、Qt SDK MinGW 与 macOS 待验证状态如实记录。
