# ZzFluentUI 滚轮选择控件实施计划

**状态：** 已完成代码级设计，等待按本文连续实现与本机验证。

**目标：** 提供一个无自定义计时器、无惯性状态机、绘制复杂度只与可见行数相关的 `ZzRoller`，以及一个复用固定 popup、支持确定与回滚事务的 `ZzRollerPicker`。两者必须保留 Fluent 外观，同时直接复用 Qt 公开输入、焦点和无障碍语义，消除旧版空集合死循环、负索引越界、零高度除法、全量绘制和 popup 对象抖动。

**架构：** `ZzRoller` 继承 `QSpinBox`，把标准 value/range 唯一映射为逻辑 item index，并自绘固定奇数行；`ZzRollerPicker` 继承 `QPushButton`，拥有一次构造并重复使用的私有 `Qt::Popup`，popup 内仅放置若干 `ZzRoller` 和标准 `QDialogButtonBox`。两个公开类分别采用四文件 Pimpl，公开头不暴露 popup、layout、button box 或平台实现类型。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

## 1. 范围与前置结论

- 本批继续总体设计阶段 10，只实现本地、同步、平面字符串列的滚轮和多列确认式选择器；日期合法性联动、时区、远程数据、虚拟化模型、业务校验与持久化留给应用 presenter 或后续专用控件。
- `ZzRoller` 的 item 只保存 `QStringList` 值快照。需要稳定业务 key/payload 或十万级远程模型时，不得把业务对象指针塞入 UI；应另立 model/view 型控件计划。
- `ZzRollerPicker` 的列由 `ZzRollerColumn` 值类型描述，组件只输出 index/text 快照和用户提交意图，不 include 或调用 repository、database、network、domain、service、presenter 或 application 模块。
- 不迁移旧版像素 scroll offset、惯性动画、长按 timer、字体图标按钮、主题单例连接、手绘 popup 按钮、全控件 pixmap 抓取或 Windows DWM 辅助代码。
- 继续复用本机 `/home/zz/Qt/6.11.1/gcc_64`，不下载 Qt，不访问 GitHub CLI、不读取远端 CI、不 push。

## 2. 旧版逐行代码审计

旧版以下十个文件只用于核对产品意图：

```text
ZzRoller.h
ZzRoller.cpp
private/ZzRollerPrivate.h
private/ZzRollerPrivate.cpp
ZzRollerPicker.h
ZzRollerPicker.cpp
private/ZzRollerPickerPrivate.h
private/ZzRollerPickerPrivate.cpp
DeveloperComponents/ZzRollerPickerContainer.h
DeveloperComponents/ZzRollerPickerContainer.cpp
```

### 2.1 ZzRoller 风险

- `ZzRoller.h:13-20` 依赖属性宏并把圆角、容器模式等主题实现细节暴露为实例状态；`ItemList` getter 返回内部引用，ABI 与跨线程生命周期不清晰。
- `ZzRoller.cpp:19-27` 用两组固定尺寸和固定字体像素构造控件，不尊重字体、DPI、style size hint、布局约束或高对比度。
- `ZzRoller.cpp:35-68` 每实例常驻一个 `QPropertyAnimation`；finished 回调在空列表时以 `size * itemHeight == 0` 作为两个 while 的步长，GUI 线程永久循环。
- `ZzRoller.cpp:52-56`、`162-172` 和多个滚动路径直接除以或乘以 `_pItemHeight`；setter 接受 0 或负值，随后发生除零、错误索引或反向几何。
- `ZzRoller.cpp:70-87` 每实例再常驻两个 `QTimer` 实现长按；即使用户从不交互，100 个 Roller 也固定产生 300 个动画/计时对象。
- `ZzRoller.cpp:108-115` 只检查 `currentIndex >= count`，未检查负数；`setCurrentIndex(-1)` 会被接受，随后 `itemList[-1]` 越界。
- `ZzRoller.cpp:118-138` 替换 item list 或 item height 时不规范化 current index、scroll offset 和 animation target；列表缩短后状态可以长期指向不存在的行。
- `ZzRoller.cpp:147-153` 允许 0、负数和偶数可见行，中心行计算与点击映射不再唯一。
- `ZzRoller.cpp:181-239` wheel、箭头和点击都用固定 `120` 触发动画，不处理 pixelDelta、natural scrolling、键盘、Home/End、Page 键、焦点或 disabled 状态。
- `ZzRoller.cpp:213-215` 点击路径以未校验 item height 做除法；控件边界外 release 也可生成无界 jump count。
- `ZzRoller.cpp:260` 起的 paint 对全部 item 做 O(n) 遍历；循环模式还为每个 item 运行 while 归一化，空列表、极端 offset 和无效高度均可能停滞。
- 旧 paint 使用手写主题色、固定边距与中心判断，未通过 `QStyleOption`、palette、字体度量、elide、RTL、focus state 或标准 SpinBox accessible value。
- 私有类以 QObject 承载属性和 animation target，但公开类析构没有明确释放原始 `d_ptr`；对象所有权依赖宏实现，无法从接口证明。

### 2.2 ZzRollerPicker 风险

- `ZzRollerPicker.cpp:18-33` 构造时创建顶层 popup 和 layout，却把 Roller 的 parent 设为 picker 后再插入 popup layout，窗口父子关系与可见生命周期混杂。
- `ZzRollerPicker.cpp:45-64` 每列固定 7 行、35px 和 90px，添加列后直接固定 picker 宽度；长文本、DPI、RTL 和窄屏没有可用约束。
- `ZzRollerPicker.cpp:67-180` 所有按列访问 API 只检查 `index >= count`，负列号可进入 `QList[-1]`；批量 index 同样把负 item index 传给 Roller。
- `ZzRollerPicker.cpp:114-125` 与 `159-170` 对短输入执行部分写入后直接 return，调用方得到非原子的半更新状态；没有单次批量信号契约。
- `ZzRollerPicker.cpp:194-223` 按每列当前数据手绘按钮内容和分隔线，绕过标准 QPushButton label、focus、mnemonic、disabled、RTL 和无障碍 Value。
- `ZzRollerPickerPrivate.cpp:14-20` popup 以硬编码全局偏移定位，未使用 `QScreen::availableGeometry()`；靠近任意屏幕边缘时都可能越界。
- `ZzRollerPickerPrivate.cpp:23-31` 确定只发信号，取消槽为空；提交、取消、外部点击和窗口失活的事务语义分散在 container flag 与 hideEvent 中。
- `ZzRollerPickerContainer.cpp:30-50` 每次打开先抓取完整 popup pixmap，再分配一条 `DeleteWhenStopped` 动画；高 DPR、多列或频繁开关会增加显存、分配和并行动画风险。
- `ZzRollerPickerContainer.cpp:80-95` 用手写矩形判断确定/取消，缺少标准按钮的键盘、焦点、助记符、无障碍 Action 和平台翻译语义。
- `ZzRollerPickerContainer.cpp:106-123` 引入 Windows 专用 DWM popup helper；hideEvent 依赖 `_isOverButtonClicked` 猜测关闭原因，异常关闭和重入路径难以证明。
- `ZzRollerPickerContainer.cpp:126-180` 全部 popup、阴影、分隔和字体图标由 paintEvent 手绘，依赖旧主题单例和私有图标字体。
- `ZzRollerPickerContainer.cpp:183-202` 快照数组与 Roller 列结构分离；popup 打开后动态增删列会让恢复循环读取不存在的 `_historyIndexList[i]`。

可保留的产品意图只有：固定可见行的滚轮、可选循环、多列组合、打开时保存快照、确定提交、取消或外部关闭回滚。新实现全部建立在 Qt 公共 API 上。

## 3. ZzRoller 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzRoller.h`：

```cpp
namespace ZzFluentUI {

class ZZ_FLUENT_UI_EXPORT ZzRoller final : public QSpinBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRoller)
    Q_PROPERTY(QStringList items READ items WRITE setItems
                   NOTIFY itemsChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex
                   WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText
                   NOTIFY currentTextChanged)
    Q_PROPERTY(int itemHeight READ itemHeight WRITE setItemHeight
                   NOTIFY itemHeightChanged)
    Q_PROPERTY(int visibleItemCount READ visibleItemCount
                   WRITE setVisibleItemCount NOTIFY visibleItemCountChanged)

public:
    explicit ZzRoller(QWidget *parent = nullptr);
    ~ZzRoller() override;

    void setItems(QStringList items);
    [[nodiscard]] QStringList items() const;
    [[nodiscard]] int itemCount() const noexcept;
    void addItem(QString text);
    [[nodiscard]] bool insertItem(int index, QString text);
    [[nodiscard]] bool removeItem(int index);
    [[nodiscard]] bool setItemText(int index, QString text);
    void clearItems();
    [[nodiscard]] QString itemText(int index) const;

    void setCurrentIndex(int index);
    [[nodiscard]] int currentIndex() const noexcept;
    [[nodiscard]] bool setCurrentText(const QString &text);
    [[nodiscard]] QString currentText() const;

    void setItemHeight(int height);
    [[nodiscard]] int itemHeight() const noexcept;
    void setVisibleItemCount(int count);
    [[nodiscard]] int visibleItemCount() const noexcept;

Q_SIGNALS:
    void itemsChanged();
    void currentIndexChanged(int index);
    void currentTextChanged(const QString &text);
    void itemHeightChanged(int height);
    void visibleItemCountChanged(int count);
    void activated(int index, const QString &text);

protected:
    [[nodiscard]] QString textFromValue(int value) const override;
    [[nodiscard]] int valueFromText(const QString &text) const override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    friend class ZzRollerPrivate;
    std::unique_ptr<ZzRollerPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

全部类型、字段和公开/复杂方法补充简体中文 Doxygen。`QSpinBox::wrapping` 继续作为公开循环开关，调用方使用标准 `setWrapping(bool)`；不再定义重复的 `isEnableLoop` 状态。

## 4. ZzRoller 状态与输入契约

### 4.1 index/value 唯一真值

- `QSpinBox::value()` 与 `currentIndex()` 始终相等。空集合时 range/value/current index 均为 `[-1, -1] / -1`；非空集合 range 为 `[0, count - 1]`。
- 内部 `QLineEdit` 只用于保留 QSpinBox 标准文本值和无障碍语义，设置 read-only、NoFocus、透明且不可见；业务输入不经过 editor，控件本体自绘全部可见行。
- `textFromValue()` 只在 index 有效时返回对应 item；无效值返回空。`valueFromText()` 返回第一个完全相同文本的 index，未知文本保持当前 index。
- 负 index、`index == count` 和更大 index 的 `setCurrentIndex()` 无副作用；空集合只接受内部规范值 -1。`itemText()` 对无效 index 返回空值。
- 重复文本合法，index 是唯一身份；`setCurrentText()` 选择首个完全匹配项并返回 true，找不到时返回 false。
- `setItems()` 保留旧 index 并收敛到新集合尾部，不按 text 猜测身份；从空变为非空时选择 0，从非空变为空时选择 -1。完全相同列表不发信号。
- insert/remove/setText/clear 在一次变更后最多发一次 `itemsChanged()`；index 实际变化发一次 `currentIndexChanged()`，当前文本实际变化发一次 `currentTextChanged()`。程序化操作不发 `activated()`。
- 插入位置合法范围为 `[0, count]`，删除/改写合法范围为 `[0, count)`；无效调用返回 false 且不发信号。

### 4.2 尺寸、绘制和循环

- `itemHeight` 默认 36，并收敛到 `[24, 96]`；`visibleItemCount` 默认 5，收敛到 `[3, 9]` 内最近的奇数，偶数向上取奇数但不得超过 9。
- 控件垂直 size policy 固定，size hint 高度严格为 `itemHeight * visibleItemCount`；宽度由最长文本字体测量缓存、style frame margin 与最小 96px 共同决定，不调用 `setFixedWidth()`。
- item 集合或字体变化时 O(n) 重建一次最长文本宽度缓存；paint、size hint、hover、wheel 和 mouse move 不扫描全集。
- paint 先通过 `QStyleOptionSpinBox` 与当前 style 绘制标准无按钮 SpinBox frame，再只遍历 `[-visible/2, visible/2]` 固定偏移；非循环越界行留空，循环行用无除零的正模映射。
- 中心行使用 palette Highlight/HighlightedText，其他行按距离降低 palette Text 透明度；disabled、focus、hover、RTL、高对比度和 DPR 均从 style option、palette、字体度量与逻辑坐标派生。
- 每行文本在行矩形内居中并 `ElideRight`，任何文本不能进入 frame 边距或覆盖相邻行。空集合只绘制空 frame，不访问 item。
- 不维护像素 scroll offset，不实现惯性或连续动画，不创建 `QTimer`、`QPropertyAnimation`、QSS、动态属性、逐项 QObject 或 pixmap cache。

### 4.3 标准输入

- Up/Down 单步，PageUp/PageDown 以 `visibleItemCount` 步进，Home/End 到首尾；只有值实际变化才发一次 `activated(index, text)`。
- `wrapping == false` 时越界步进停在首尾；`wrapping == true` 时使用正模循环，多步输入也只产生一个最终变化和一个用户意图信号。
- wheel 累积 angleDelta 到标准 120 单位，并支持 pixelDelta 的离散方向；尊重 `QWheelEvent::inverted()`，禁用或空集合时无副作用。
- 左键点击非中心可见行时离散跳转对应步数；按住拖动每跨越一个 `itemHeight` 更新一次 index，release 最多发一次最终 `activated`。拖动不创建 timer/animation。
- Escape、Enter 和 Return 由 Roller 忽略并向父级传播，使 Picker popup 能处理取消/确定；Tab/Backtab 保持 Qt 标准焦点遍历。
- 鼠标 hover 只保存一个行 offset 并触发局部 update；leave 清理 hover。右键、中键和边界外 release 不改变 index。

## 5. ZzRollerPicker 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzRollerPicker.h`：

```cpp
namespace ZzFluentUI {

struct ZZ_FLUENT_UI_EXPORT ZzRollerColumn final
{
    QString key;
    QStringList items;
    int currentIndex = 0;
    bool wrapping = true;
    int minimumWidth = 96;

    friend bool operator==(
        const ZzRollerColumn &,
        const ZzRollerColumn &) = default;
};

class ZZ_FLUENT_UI_EXPORT ZzRollerPicker final : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRollerPicker)
    Q_PROPERTY(int columnCount READ columnCount NOTIFY columnsChanged)
    Q_PROPERTY(QString currentText READ currentText
                   NOTIFY currentSelectionChanged)
    Q_PROPERTY(bool popupVisible READ isPopupVisible
                   NOTIFY popupVisibleChanged)

public:
    explicit ZzRollerPicker(QWidget *parent = nullptr);
    ~ZzRollerPicker() override;

    void setColumns(QList<ZzRollerColumn> columns);
    [[nodiscard]] QList<ZzRollerColumn> columns() const;
    [[nodiscard]] int columnCount() const noexcept;
    [[nodiscard]] QString addColumn(ZzRollerColumn column);
    [[nodiscard]] bool insertColumn(int index, ZzRollerColumn column);
    [[nodiscard]] bool removeColumn(const QString &key);
    [[nodiscard]] bool removeColumnAt(int index);
    void clearColumns();
    [[nodiscard]] bool setColumnItems(int column, QStringList items);

    [[nodiscard]] bool setCurrentIndex(int column, int index);
    void setCurrentIndexes(const QList<int> &indexes);
    [[nodiscard]] int currentIndex(int column) const noexcept;
    [[nodiscard]] QList<int> currentIndexes() const;
    [[nodiscard]] bool setCurrentText(int column, const QString &text);
    [[nodiscard]] QString currentText(int column) const;
    [[nodiscard]] QStringList currentTexts() const;
    [[nodiscard]] QString currentText() const;

    void showPopup();
    void acceptPopup();
    void cancelPopup();
    [[nodiscard]] bool isPopupVisible() const noexcept;

Q_SIGNALS:
    void columnsChanged();
    void currentSelectionChanged(
        const QList<int> &indexes,
        const QStringList &texts);
    void selectionActivated(
        int column,
        int index,
        const QString &text);
    void selectionAccepted(
        const QList<int> &indexes,
        const QStringList &texts);
    void selectionCanceled();
    void popupVisibleChanged(bool visible);

private:
    friend class ZzRollerPickerPrivate;
    std::unique_ptr<ZzRollerPickerPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

`ZzRollerColumn` 注册 metatype，全部成员只包含隐式共享值，不保存 QObject 或业务对象裸指针。继承的 `QPushButton::text()` 返回控件派生的组合摘要；公开静态类型上隐藏 `setText()`，防止第二份展示状态。

## 6. Picker 列与事务契约

### 6.1 列规范化

- `setColumns/add/insert` 在写入边界规范化：空 key 或重复 key 替换为无花括号 UUID；`minimumWidth` 收敛到 `[64, 512]`。
- 空 item 列的 current index 规范为 -1；非空列的 current index 收敛到 `[0, count - 1]`。重复文本合法且以 index 区分。
- `setColumns()` 对规范化后完全相同的集合无副作用；结构或 item 变化最多发一次 `columnsChanged()`，派生 selection 变化最多发一次 `currentSelectionChanged()`。
- insert 合法位置为 `[0, count]`；所有列访问严格检查 `column >= 0 && column < count`，item index 同时检查上下界。无效单项 setter 返回 false。
- `setCurrentIndexes()` 是原子批量操作：先计算所有目标，再一次应用；短列表只覆盖给出的前缀，超长尾部忽略，负数或越界 item index 保持对应列原值，最多发一次 selection 变化信号。
- current indexes/texts 总按列顺序返回；空 item 列对应 `-1` 和空字符串。组合 `currentText()` 只连接非空文本，默认分隔符为 `" / "`，不通过 split 反推状态。
- 结构性 mutation 在 popup 可见时先执行一次 cancel/回滚并关闭 popup，再修改列，确保 snapshot 长度永远与 popup Roller 结构一致。

### 6.2 popup 生命周期

- 构造 Picker 时一次创建私有 `QFrame(Qt::Popup)`、一个水平 Roller layout、一个标准 `QDialogButtonBox(Ok | Cancel)` 和对应 `ZzRoller` 列；后续 show/hide 不分配 popup、animation、timer 或 pixmap。
- popup 打开前把当前 index list 保存为唯一快照；重复 `showPopup()` 不覆盖快照。用户滚动直接修改 Roller 唯一状态、刷新按钮摘要并发 selection change/activated。
- 确定按钮、popup 内 Enter 或公开 `acceptPopup()` 提交当前值，清除快照、隐藏 popup，并只发一次 `selectionAccepted()`。
- 取消按钮、Escape、外部点击、窗口失活或公开 `cancelPopup()` 先原子恢复快照，再隐藏 popup；若草稿实际变化，恢复过程发一次 `currentSelectionChanged()`，随后发一次 `selectionCanceled()`。
- 未发生草稿变化的取消不发 current selection change；popup 自身 hideEvent 负责兜底外部关闭，内部 closing guard 防止 accept/cancel/hide 重入和双信号。
- 程序化单项/批量 setter 在 popup 打开期间属于当前草稿，取消会恢复打开时快照；调用方需要永久提交时调用 `acceptPopup()`。
- popup 只在至少存在一列时打开；空列不创建临时占位对象。Picker disabled 时 clicked、show、accept 和 cancel 不产生用户意图。

### 6.3 定位、键盘与无障碍

- popup size hint 由列 minimum width、Roller 可见高度、layout margin、spacing 和 button box 组成；不使用 picker 固定宽度。
- 通过 `windowHandle()->screen()` 或 `QGuiApplication::screenAt()` 取得目标 `QScreen::availableGeometry()`；LTR 优先左对齐、RTL 优先右对齐，空间不足时从下方翻转至上方，再把最终矩形完整 clamp 到可用区域。
- popup 不 include Qt Private、QPA、native event 或平台原生头；不设置 `NoDropShadowWindowHint`，平台标准 popup 阴影和关闭行为保留。
- 标准 Ok/Cancel 按钮使用 Qt 平台文本、图标、焦点、Enter/Escape 与 Accessible Action；不绘制字体图标，不硬编码翻译文本。
- Roller 保持标准 SpinBox role/value/focusable/disabled 状态；Picker 保持 PushButton role/name/derived text/pressed action，popup 子项和按钮使用 Qt 标准 accessible interface，不注册自定义工厂。
- Tab/Backtab 在各列和按钮间遍历；打开后焦点进入第一列，空列跳过；关闭后焦点返回 Picker。RTL 只改变列视觉顺序和 popup 对齐，不改变公开 columns/indexes 的逻辑顺序。

## 7. 私有结构与性能边界

新增八个实现文件：

```text
widgets/include/ZzFluentUI/ZzRoller.h
widgets/src/ZzRoller.cpp
widgets/src/private/ZzRollerPrivate.h
widgets/src/private/ZzRollerPrivate.cpp
widgets/include/ZzFluentUI/ZzRollerPicker.h
widgets/src/ZzRollerPicker.cpp
widgets/src/private/ZzRollerPickerPrivate.h
widgets/src/private/ZzRollerPickerPrivate.cpp
```

- `ZzRollerPrivate` 只保存 items、尺寸、最长文本宽度、wheel remainder、hover/drag 短状态和非拥有 `q_ptr`；不继承 QObject。
- `ZzRollerPickerPrivate` 保存规范化 columns、popup/layout/button box、与列一一对应的 `QList<ZzRoller *>`、打开快照和关闭 guard；不继承 QObject。
- private `.cpp` 内可定义 `ZzRollerPickerPopup final : public QFrame` 处理标准 panel 绘制与 hide 兜底；类型不安装、不导出、不进入公开头。
- Pimpl 只在构造发生一次堆分配。结构变更允许按列创建/销毁 Roller；show/hide、选择、paint、wheel、key 和 pointer move 不改变 QObject descendant 数量。
- 100 个独立 Roller 的固定基础设施不包含 animation/timer；100 个 Picker 的 popup 和标准按钮为构造期固定成本。基准分别记录两类对象，避免把 popup 预热延迟成本混入帧计时。

## 8. 自动测试

新增 `ZzFluentUI/tests/ZzRollerControlsTest.cpp` 与 CTest `fluent.roller-controls`：

- 验证 Roller 默认空状态、range/value/index/text、36px、5 行、Preferred/Fixed size policy、read-only 隐藏 editor、NoButtons、StrongFocus 和最小 96px 宽。
- 验证 set/add/insert/remove/setText/clear、重复文本、空文本、40 项以上集合、负 index、等于 count、列表缩短和信号次数。
- 验证 itemHeight 的 24/96 clamp、visible count 的 3/9 奇数 clamp、字体改变后的 size hint、长文本 elide、空列表绘制和只绘制固定可见行。
- 验证 non-wrapping/wrapping 的 Up/Down、Page、Home/End、wheel angle/pixel/natural、点击行、拖动、disabled 和空列表；程序化 setter 不发 activated，用户输入实际变化只发一次。
- 验证 Light/Dark/HighContrast、LTR/RTL、focus/hover/disabled 与标准 `QAccessible::SpinBox`、name、value、focusable/disabled 状态。
- 验证 Picker 的 column key 规范化、宽度 clamp、空列、重复文本、负 column/index、短/长 bulk、原子信号与按列顺序查询。
- 显示真实 popup，验证重复 show 不覆盖快照、多个 Roller 草稿、确定/Enter 提交、取消/Escape/外部点击回滚、无变化取消、focus 返回和 visible signal 次数。
- popup 打开后执行程序化草稿并回滚；结构 mutation 先回滚关闭；动态增删列后再次打开/取消不得越界。
- 在模拟多屏可用区域的实际单屏边界上验证下方、上方与水平 clamp；几何完全位于目标 `availableGeometry()`。
- 验证标准 Ok/Cancel button、icons、focus policy、accessible names/actions，以及 Picker 的 PushButton role 和派生 summary text。
- 构造 100 个 Roller 和 20 个 Picker，预热 popup 后执行 1000 轮选择、循环、方向、enabled、show/accept/cancel；恢复初值并处理 deferred delete，QObject、animation 和 timer 数量不增长。

扩展既有质量边界：

- 公开头逐头编译、shared/static public MOC、metatype 与 fresh install consumer 必须包含两个新类和 `ZzRollerColumn`。
- 架构审计继续禁止 UI include 业务/基础设施词缀、Qt Private、QWindowKit 或第三方实现头。
- 标准 `QSpinBox` 与 `QPushButton` 测试继续通过，证明新组件没有修改全局标准输入语义。

## 9. 画廊与安装消费

- 在 `examples/ZzFluentControlsGallery` 输入区域加入一个单列 `ZzRoller`，展示空/非循环边界与循环切换；再加入小时、分钟、AM/PM 三列 `ZzRollerPicker`。
- 画廊使用现有图标和展示 label 反映 `activated/currentSelectionChanged/selectionAccepted/selectionCanceled`，不出现解释功能、快捷键或实现方式的可见说明文字。
- `tests/InstallConsumer/Gui/main.cpp` 从安装后的两个公开头创建控件，验证空集合安全、循环、重复 text、key 规范化、批量 index、popup 确定/回滚和 metatype。
- `tests/InstallConsumer/CMakeLists.txt` 把两个新头加入独立安装头编译清单；不暴露 private popup、layout、button box、snapshot 或平台 helper。

## 10. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造 100 个 Roller、每个 10000 条 item，当前 index 分散；10 帧预热、120 帧正式渲染到预分配 `QImage`。
- 每帧只对一个 Roller 执行一次用户等价离散步进并渲染全部控件；计时区不创建控件、item、timer、animation、popup、layout 或图片。
- 当前活动 Linux 参考环境离屏渲染 P95 `<= 16.7 ms`；普通环境只记录数据。CTest 明确 `QT_QPA_PLATFORM=offscreen`，结果只表述为 `QImage` 离屏绘制，不宣称 compositor 帧时间。
- 1000 轮 items/index/wrapping/direction/enabled/font 状态变化后恢复初值，验证 descendants/animations/timers 与预热后一致。
- 单个 10000 项 Roller 连续执行 1000 次 paint，证明运行时间不随 item 总数线性增长；用 20 项对照组计算宽松比值门禁，避免绑定机器绝对时间。
- 20 个三列 Picker 预热 popup，执行 1000 次 show/change/cancel 与 show/change/accept，记录总时间并锁定首次本机 reference 的约两倍门限；对象计数保持稳定。

## 11. 视觉基线

扩展 `ZzFluentScreenshotTest`，新增固定尺寸 `roller-controls` surface：

- 覆盖空 Roller、首项/中项/末项、循环、非循环、3/5/9 行、长文本、重复文本、hover、focus 和 disabled。
- 覆盖三列 Picker 的关闭摘要、popup open、当前中心行、空列、不同 column width、Ok/Cancel、下方/上方展开、LTR 和 RTL。
- 建立 Light、Dark、HighContrast x DPR 1.0、1.25、1.5、2.0 共 12 张基线。
- Roller 与按钮文字纳入文字遮罩；frame、center indicator、focus、hover、disabled、popup surface、separator 和按钮图标参加严格像素比较。
- 人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认文本非空、中心行唯一、无裁切/重叠/错误 RTL、popup 不越屏且关闭控件保持可辨识。

## 12. 跨平台静态检查

- Windows MSVC、Windows Qt SDK MinGW 和 macOS 只使用 Qt Widgets/Core/Gui 公共 API、标准 C++20 与组件 private header；本批不增加平台分支。
- 正模计算避免负余数差异；尺寸乘法先限制输入再计算，避免编译器相关 overflow。wheel delta 使用 Qt 整数类型，不依赖触控板平台原生事件。
- popup 定位只用逻辑像素、`QScreen::availableGeometry()` 和 QWidget/global mapping；不使用 Win32、Cocoa、X11、Wayland、DWM、QPA 或 Qt private symbols。
- Doxygen、源文件和 UI 字符串使用 UTF-8；现有 MSVC `/utf-8`、严格 warnings 和 visibility 设置必须继续覆盖新增目标。
- preset matrix 继续保留 Windows MSVC shared/static、Windows Qt SDK MinGW shared/static、macOS arm64/x86_64 shared/static；本批只做源代码、CMake、公开 ABI、依赖和条件编译静态审计，不宣称原生编译或真机验证。

## 13. 提交拆分与验收

每个逻辑批次验证后立即提交，标题中文简述，正文使用中文详细说明：

```text
文档：规划Fluent滚轮选择控件批次

记录旧版空集合死循环、负索引、除零、全量绘制和popup事务风险。
固定新公开API、QSpinBox索引契约、可见行绘制边界和标准popup事务。
```

```text
控件：实现Fluent滚轮选择控件

新增ZzRoller与ZzRollerPicker四文件Pimpl实现并接入公开安装目标。
复用Qt标准输入与无障碍语义，移除自定义动画、计时器和平台私有依赖。
```

```text
测试：接入滚轮选择控件质量与安装消费

覆盖空集合、索引、循环、输入、popup提交回滚、动态列和无障碍契约。
接入画廊、公开头、metatype、fresh install与package relocation验证。
```

```text
性能：锁定滚轮选择控件性能预算

加入100个Roller固定可见行绘制、万项复杂度和Picker重复事务基准。
记录离屏参考数据并锁定宽松门限与对象稳定性预算。
```

```text
测试：补齐滚轮选择控件多主题视觉基线

新增Light、Dark、HighContrast四档DPR截图与文字遮罩。
验证中心行、焦点、禁用、popup按钮、边界定位与RTL视觉结构。
```

```text
文档：记录滚轮选择控件批次交付结果

记录Linux GCC、Clang、sanitizer、clang-tidy、截图、性能和安装消费结果。
明确Windows与macOS仅完成源码静态审计，远端CI继续暂缓。
```

本机环境：

```bash
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
```

每个代码提交运行对应 target 与 CTest；最终运行 GCC Release shared/static、Clang ASan+UBSan、shared/static `ZzClangTidy`、public headers、fresh install consumer、四档截图、reference benchmark 与画廊 smoke。

## 14. 完成定义

- `ZzRoller` 与 `ZzRollerPicker` 安装接口完整，全部自定义类型使用 Zz 前缀和简体中文 Doxygen。
- QSpinBox value 是 Roller 唯一 index 真值；空集合、负索引、零高度、重复文本和循环边界均有确定且无未定义行为的契约。
- paint 只访问固定可见行，全集扫描只发生在 item mutation 后的宽度缓存更新；每实例没有 animation 或 timer。
- Picker popup 一次构造并复用，标准确定/取消、Enter/Escape、外部关闭、快照回滚和动态列具有单一事务路径。
- Light、Dark、HighContrast x 四档 DPR 视觉基线通过；100 个 Roller 离屏绘制 P95 满足 16.7ms，万项对照比和 Picker 事务满足锁定门限。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows MSVC、Qt SDK MinGW 与 macOS 待验证状态如实记录。
