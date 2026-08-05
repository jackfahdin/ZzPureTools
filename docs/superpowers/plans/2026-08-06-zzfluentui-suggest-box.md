# ZzFluentUI 搜索建议框实施计划

**目标：** 提供一个基于标准 `QLineEdit` 输入语义、可按文本过滤本地展示建议并携带稳定键与调用方载荷的 `ZzSuggestBox`，在 Light、Dark、HighContrast、LTR、RTL 和高 DPI 下保持完整 Fluent 外观，同时不复制输入、popup、键盘、焦点或无障碍状态机。

**架构：** `ZzSuggestBox` 继承 `QLineEdit`，输入法、光标、选择、撤销、校验、快捷键和 EditableText 无障碍语义全部由 Qt 提供。私有实现只拥有一个轻量 `QAbstractListModel`、一个 `QCompleter` 和一个预构造 `QListView`；建议数据只有模型中的一份值语义快照。`QCompleter` 负责过滤、popup 生命周期和 Up/Down/Enter/Escape，`ZzFluentItemDelegate` 与应用级 `ZzFluentStyle` 负责展示。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

## 1. 范围与前置结论

- 本批继续总体设计阶段 10，只实现本地建议集合的同步过滤与选择，不实现网络检索、数据库搜索、异步任务、命令执行或业务历史记录。
- 该组件有独立价值：标准 `QCompleter` 提供机制但不提供稳定键、图标、调用方载荷、集合管理和统一 Fluent popup 装配，因此新增 `ZzSuggestBox`，而不是给普通 `QLineEdit` 增加全局行为。
- `ZzSuggestBox` 是输入控件，不是业务搜索服务。调用方先把允许展示的值语义快照转换为 `ZzSuggestion`；组件不得 include 或调用 repository、database、network、domain、service 或 application 模块。
- 单选组合框继续使用标准 `QComboBox`；多选 tag/chip、远程自动补全、分组建议、富交互命令面板和历史持久化属于后续独立批次。
- popup 由 `QCompleter` 创建、定位和关闭；本批不创建自定义顶层窗口、不修改 window flags、不抓取窗口 pixmap、不实现展开动画。
- 继续复用本机 `/home/zz/Qt/6.11.1/gcc_64` 验证，不下载 Qt，不访问 GitHub CLI、不读取远端 CI、不 push。

## 2. 旧版逐文件代码审计

旧版以下 10 个文件只作为行为与视觉意图参考：

```text
ZzSuggestBox.h
ZzSuggestBox.cpp
private/ZzSuggestBoxPrivate.h
private/ZzSuggestBoxPrivate.cpp
DeveloperComponents/ZzSuggestBoxSearchViewContainer.h
DeveloperComponents/ZzSuggestBoxSearchViewContainer.cpp
DeveloperComponents/ZzSuggestModel.h
DeveloperComponents/ZzSuggestModel.cpp
DeveloperComponents/ZzSuggestDelegate.h
DeveloperComponents/ZzSuggestDelegate.cpp
```

明确不迁移的实现如下：

- 公开头依赖旧 `ZzDef.h`、宏生成属性和字体图标枚举，接口无法作为独立安装包边界；`SuggestData` 还通过隐藏宏生成字段访问器，所有权和复制成本不可见。
- 建议项为逐项分配的 `QObject`，再由 `QVector<ZzSuggestion *>` 保存，每条短文本都承担堆分配、meta object 和 parent-child 成本；删除又依赖 `deleteLater()`，短期内模型与真实存活对象可能不一致。
- key 每项使用带字符串转换和多次 `remove()` 的 UUID 生成；批量装载没有 `reserve()`，重复扩容并且没有重复 key 契约。
- `removeSuggestion(int)` 只检查 `index >= count`，负索引仍会越界；按 key 删除在 `foreach` 复制容器时继续修改原容器，没有命中后退出。
- `clearSuggestion()` 一边遍历复制一边 `removeOne()`，形成 O(n^2)；模型没有同步 reset，popup 可能继续持有已安排延迟销毁的裸指针。
- 私有模型 `data()` 对所有 role 永远返回空值，delegate 只能把 index 强制转回具体模型并读取内部裸指针，破坏标准 model/view role 契约和代理模型兼容性。
- `clearSearchNode()` 清空向量时没有 `beginResetModel()/endResetModel()`；`setSearchSuggestion()` 对空结果直接返回，旧结果不会被清除。
- 每次文本编辑都线性扫描所有 `QObject`，构造新的指针向量并完整 reset model；该做法没有问题本身的复杂度上界、排序契约或大集合测试。
- 构造时固定 `280 x 35`，每项固定 40px，并在调用 `setFixedSize()` 时同步覆盖内部输入框高度，字体增长、DPI、触控密度和布局约束均被绕过。
- 输入框使用两个主题专用 `QAction` 和两份图标，主题切换时反复 remove/add；两个 action 的 triggered lambda 实际为空，增加无功能对象和可访问噪声。
- popup 被创建为 `window()` 的普通子 widget，通过 `mapTo(window())` 手工定位，只考虑单个祖先坐标；嵌套滚动、顶层移动、多屏边界、RTL 和窗口外裁切都没有可靠处理。
- 展开时分别新建 size、view position 动画，关闭时再新建两组动画；快速输入会并发创建 `DeleteWhenStopped` 对象，布尔标志不能取消过期动画。
- 动画过程中把 view 从 layout 移除，再在回调中重新加入；lambda 捕获裸成员，控件销毁、popup 被关闭或新动画覆盖后存在生命周期竞态和布局抖动。
- close 路径只等待 position 动画完成才清空模型、隐藏 popup；size 动画和 position 动画的完成顺序没有统一状态机。
- focus-out 仅通过自定义 `wmFocusOut` 信号和 `underMouse()` 判断，缺少 Tab、窗口失活、键盘 Escape、popup 焦点转移和辅助技术操作语义。
- 只连接鼠标 `clicked`，没有 Up/Down、PageUp/PageDown、Enter、Escape、Home/End 和键盘 search；也没有标准 completer 的 current completion 行为。
- 自绘 delegate 用固定基线坐标绘制文本和字体图标，不使用 `QStyle`、字体度量、elide、palette group、decoration role、disabled、RTL 或高对比度。
- delegate 通过 `dynamic_cast` 加 `const_cast` 假定具体模型，未检查转换和行范围；任何代理模型、外部 model 或失效 index 都可能解引用空指针。
- popup container 使用 object name 和 QSS 设透明背景，每个实例保存主题枚举并单独连接主题单例；paint 中再次访问全局主题并忽略 `QPaintEvent` 脏区。

可保留的产品意图只有：可清除文本输入、contains/case-sensitive 过滤、最多若干可见项、可选图标、稳定 key、调用方载荷和选择信号。新实现全部建立在 Qt 公开契约上。

## 3. 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzSuggestBox.h`：

```cpp
namespace ZzFluentUI {

struct ZZ_FLUENT_UI_EXPORT ZzSuggestion final
{
    QString key;
    QString text;
    QIcon icon;
    QVariant data;
};

class ZZ_FLUENT_UI_EXPORT ZzSuggestBox final : public QLineEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSuggestBox)
    Q_PROPERTY(Qt::CaseSensitivity caseSensitivity
               READ caseSensitivity WRITE setCaseSensitivity
               NOTIFY caseSensitivityChanged)
    Q_PROPERTY(Qt::MatchFlag filterMode
               READ filterMode WRITE setFilterMode
               NOTIFY filterModeChanged)
    Q_PROPERTY(int maximumVisibleItems
               READ maximumVisibleItems WRITE setMaximumVisibleItems
               NOTIFY maximumVisibleItemsChanged)
    Q_PROPERTY(int suggestionCount
               READ suggestionCount NOTIFY suggestionsChanged)

public:
    explicit ZzSuggestBox(QWidget *parent = nullptr);
    ~ZzSuggestBox() override;

    void setSuggestions(QList<ZzSuggestion> suggestions);
    [[nodiscard]] QList<ZzSuggestion> suggestions() const;
    [[nodiscard]] int suggestionCount() const noexcept;

    [[nodiscard]] QString addSuggestion(
        QString text,
        QVariant data = {},
        QIcon icon = {});
    [[nodiscard]] QString addSuggestion(ZzSuggestion suggestion);
    [[nodiscard]] bool removeSuggestion(const QString &key);
    [[nodiscard]] bool removeSuggestionAt(int index);
    void clearSuggestions();

    void setCaseSensitivity(Qt::CaseSensitivity sensitivity);
    [[nodiscard]] Qt::CaseSensitivity caseSensitivity() const noexcept;
    void setFilterMode(Qt::MatchFlag mode);
    [[nodiscard]] Qt::MatchFlag filterMode() const noexcept;
    void setMaximumVisibleItems(int count);
    [[nodiscard]] int maximumVisibleItems() const noexcept;

    void showSuggestions();
    void hideSuggestions();
    [[nodiscard]] bool isSuggestionPopupVisible() const noexcept;

Q_SIGNALS:
    void suggestionActivated(const ZzSuggestion &suggestion);
    void suggestionHighlighted(const ZzSuggestion &suggestion);
    void suggestionsChanged();
    void caseSensitivityChanged(Qt::CaseSensitivity sensitivity);
    void filterModeChanged(Qt::MatchFlag mode);
    void maximumVisibleItemsChanged(int count);
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzSuggestion)
```

接口约束：

- `ZzSuggestion` 是可复制、隐式共享成员组成的展示快照，不是 `QObject`，不持有业务对象裸指针。
- 空 key 在插入时生成不带花括号的 UUID；重复 key 也生成新 key，模型内始终唯一。调用者通过 `addSuggestion()` 返回值或 `suggestions()` 获得最终 key。
- text、icon 和 data 保持调用方值；允许相同 text，因为选择必须按 completion index 取回对应 key/data，而不是按文本反查。
- `setSuggestions()` 只发一次 model reset 和一次 `suggestionsChanged()`；单项增删使用 insert/remove rows，不全量 reset。
- `removeSuggestionAt()` 对负数和越界返回 false；删除不存在 key 返回 false；无变化时不发信号。
- `Qt::MatchStartsWith`、`Qt::MatchContains`、`Qt::MatchEndsWith` 是唯一有效 filter mode；其他 flag 不改变当前值。
- `maximumVisibleItems` 收敛到 `[1, 100]`；默认 8。默认 case-insensitive、contains，与旧版用户可见意图一致。
- `showSuggestions()` 使用当前 text 作为 completion prefix 并调用 `QCompleter::complete()`；空集合不显示。`hideSuggestions()` 只隐藏 completer popup，不修改输入和集合。
- 基类 `setCompleter()` 在 `ZzSuggestBox` 静态类型上设为 private，防止调用方替换内部 completer 后破坏集合和信号契约；输入法、validator、clear button、placeholder、selection、undo/redo 和 text signals 继续直接使用 `QLineEdit` API。
- 所有公开类、结构、字段和方法使用简体中文 Doxygen；传统单层命名空间，不使用链式命名空间。

## 4. 私有实现

新增四文件结构：

```text
widgets/include/ZzFluentUI/ZzSuggestBox.h
widgets/src/ZzSuggestBox.cpp
widgets/src/private/ZzSuggestBoxPrivate.h
widgets/src/private/ZzSuggestBoxPrivate.cpp
```

### 4.1 值模型

- `ZzSuggestBoxPrivate.cpp` 内定义 `ZzSuggestionListModel final : public QAbstractListModel`；它是 private translation-unit 类型，不泄漏到安装接口。
- 模型唯一保存 `QList<ZzSuggestion>`，不为每项创建 QObject、QAction、QStandardItem 或 widget。
- `rowCount()` 对有效 parent 返回 0；`data()` 先检查 index、row 和 column，再映射 `DisplayRole/EditRole -> text`、`DecorationRole -> icon`、private key role -> key、private payload role -> data。
- completion 激活和高亮从 completion model index 的上述 role 直接构造 `ZzSuggestion`，不得按文本扫描源集合；代理模型会转发 role，因此重复文本也能返回正确载荷。
- `setSuggestions()` 先在栈上规范化 key 并 `reserve()`，再一次 reset；add/remove 分别使用 `beginInsertRows/endInsertRows` 和 `beginRemoveRows/endRemoveRows`。
- key 查找允许 O(n)，因为它只发生在显式集合变更，不在 paint、键入过滤或每帧路径；必要时以后可在不改变公开 API 的前提下加入索引表。

### 4.2 QCompleter 装配

- private 构造阶段一次性创建 model、`QCompleter`、`QListView` 和 `ZzFluentItemDelegate`，全部以 QObject parent 管理；Pimpl 只保存非拥有指针。
- completer 使用 `PopupCompletion`、`CaseInsensitivelySortedModel` 之外的默认未排序模式、`MatchContains`、最大 8 项，并以 `Qt::EditRole` 作为 completion role。
- popup 使用 `SingleSelection`、`NoEditTriggers`、`uniformItemSizes=true`、`ScrollPerPixel`、右侧省略和应用级 style；不设置 stylesheet、object name、window flags 或 fixed size。
- popup item 高度由既有 `ZzFluentItemDelegate` 的 Standard 40px 密度提供；文本、图标、selection、hover、focus、disabled、palette 和 RTL 走标准 role/style。
- popup viewport、滚动条和 window 自动继承应用 `ZzFluentStyle`；不为每实例创建 `QProxyStyle`，不手工绘制 popup frame。
- `QLineEdit::setCompleter()` 只在构造时调用一次；Qt 的 event filter 负责键盘导航、选择写回、焦点转移、popup 定位和多屏约束。
- completer 的 QModelIndex overload `activated`/`highlighted` 分别转发公开信号；连接以公开对象为 context，控件销毁时自动断开。
- 构造默认启用 clear button，不设置固定宽高；尺寸继续由 `ZzFluentStyle::CT_LineEdit`、字体和外部 layout 决定。

### 4.3 生命周期与性能

- 每实例固定创建一套 model/completer/popup/delegate，建议集合变化、键入、主题切换和 popup 开关均不得继续创建 QObject。
- 不创建 `QPropertyAnimation`、`QTimer`、QSS、theme singleton connection、pixmap cache 或动态属性。
- Pimpl 的一次堆分配只发生在构造阶段，不进入输入、过滤、layout、paint 或激活热路径；其 ABI 隔离收益高于不可测的单次构造成本。
- 不覆写 `paintEvent()`、`keyPressEvent()`、`focusInEvent()`、`focusOutEvent()`、`inputMethodEvent()` 或 `eventFilter()`，确保 Qt 原生输入和辅助功能状态机保持唯一。

## 5. 自动测试

新增 `ZzFluentUI/tests/ZzSuggestBoxTest.cpp` 和 CTest `fluent.suggest-box`：

- 验证默认 case-insensitive contains、8 个可见项、clear button、StrongFocus/输入法和最小 32px Fluent line-edit 尺寸。
- 验证 set/add/remove/removeAt/clear、自动 key、调用方 key、重复 key 规范化、重复 text、图标和任意 QVariant data；无变化不发 `suggestionsChanged()`。
- 通过公开 `QCompleter::completionModel()` 验证 starts-with/contains/ends-with、case-sensitive/case-insensitive、空 prefix、无命中和最大可见项。
- 验证模型 DisplayRole、EditRole、DecorationRole、invalid index、有效 parent rowCount 以及 set/reset/insert/remove 后 persistent index 行为。
- 显示真实控件和 popup，覆盖 Down/Up、Enter、Escape、鼠标点击、clear action、焦点切换、validator、IME query、undo/redo；不调用自定义事件处理器。
- 两个相同 text、不同 key/data 的建议分别激活时，`suggestionActivated` 必须返回对应 index 的完整快照；highlighted 同理。
- 覆盖 LTR/RTL、disabled、只读、长文本、图标、空文本、高对比度和主题热切换；popup 不得改变调用方 window flags 或移动宿主。
- `QAccessible` 主控件保持 EditableText role、name、value、focusable/focused/disabled/read-only 状态；popup list/item 使用 Qt 标准角色，不注册自定义 accessible interface。
- 预构造 100 个控件、每个 20 条建议，执行 1000 轮 text、case、filter mode、maximum visible items、add/remove 和方向切换，恢复初值并处理 deferred delete 后 QObject、animation 和 timer 数量不增长。

扩展既有质量边界：

- 公开头编译测试必须单独 include `ZzSuggestBox.h`，安装导出和 shared/static consumer 均可链接 `ZzSuggestion` signal。
- 架构审计继续禁止 UI 包含业务和基础设施词缀依赖、Qt Private、QWindowKit 或第三方实现头。
- `ZzFluentStyleTest` 不增加 SuggestBox 专用状态源；输入 panel 和 item delegate 的已有测试继续作为绘制底座。

## 6. 画廊与安装消费

- 在 `examples/ZzFluentControlsGallery` 的输入区域加入真实 `ZzSuggestBox`，展示 icon、重复文本、稳定 key/data、contains 过滤和 disabled 项；只使用本地展示数据。
- 画廊把 `suggestionActivated` 连接到展示 label，证明 UI 意图输出；不得从组件内部访问业务 service 或 model。
- `tests/InstallConsumer/Gui/main.cpp` 从安装后的 `<ZzFluentUI/ZzSuggestBox.h>` 创建控件，插入两条建议，验证 key 唯一、过滤结果、载荷和 style；这同时验证 public MOC、metatype 和 shared/static ABI。
- 不在安装接口暴露 private model、delegate、popup container 或 completion role 常量。

## 7. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造 100 个 `ZzSuggestBox`，每个包含 20 条值语义建议；10 帧预热、120 帧正式渲染，记录 P50/P95/max。
- 每帧只改变一个控件的 text 或 completion prefix，并把 100 个控件渲染到预分配 `QImage`；计时区不创建控件、模型、delegate、suggestion item 或图片。
- 当前活动 Linux 参考发布环境绝对 P95 `<= 16.7 ms`；普通环境只记录数值。
- 1000 轮状态切换覆盖 text、case、filter mode、visible count、direction 和一条建议增删；恢复初始状态后 descendants、animations、timers 必须相同。
- 另以单个 10000 条建议集合测量 100 次 prefix 变化和 completion rowCount 查询，记录总耗时并确保每次只返回匹配行，不创建每结果 QObject；参考机绝对总耗时门限在首次测量后以宽松稳定值写入交付记录，不把未测猜测写成门禁。
- paint 路径不得遍历建议集合；激活不得按 text 反查；过滤复杂度由 Qt `QCompleter` completion model 承担。

## 8. 视觉基线

扩展 `ZzFluentScreenshotTest`，增加固定尺寸 `suggest-box` surface：

- 覆盖 empty/placeholder、typed、clear button、focus、disabled、read-only、icon、long text、LTR、RTL 和一个打开的建议 popup。
- popup 只通过 `box.completer()->popup()` 公共接口获得并合成到固定画布，不匹配 Qt 私有类名、不改变 window flags。
- 建立 Light、Dark、HighContrast x DPR 1.0、1.25、1.5、2.0 共 12 张基线。
- line edit 文字和 popup item 文字纳入显式文字遮罩；input surface、focus stroke、clear icon、popup surface、selection/hover、decoration icon 和滚动条参加严格比较。
- 更新后人工检查 DPR 1.0 三主题和 DPR 2.0 Light，确认画面非空，无裁切、重叠、错误 RTL、双 frame 或不可读状态。

## 9. 跨平台静态检查

- Windows MSVC、Windows Qt SDK MinGW 和 macOS 只使用 Qt Widgets 公共 API、标准 C++20 和组件 private header；不新增平台分支。
- UUID 使用 Qt Core `QUuid`，尺寸使用逻辑像素，popup 定位完全委托平台插件和 `QCompleter`。
- 运行 preset matrix、gate script contract、public headers、架构和 Fluent 边界审计；本机结果不得表述为 Windows/macOS 已编译或真机通过。

## 10. 提交顺序

```text
文档：规划Fluent搜索建议框批次

审计旧版建议对象、过滤模型、popup定位和动画生命周期问题。
确定复用QLineEdit、QCompleter与值语义列表模型的公开契约。
```

```text
控件：实现Fluent搜索建议框

新增ZzSuggestion与ZzSuggestBox四文件Pimpl实现。
保留Qt原生输入、过滤、popup、键盘和无障碍状态机。
```

```text
测试：接入搜索建议框质量与安装消费

补齐集合、过滤、键鼠、载荷、无障碍、对象稳定性和性能测试。
接入画廊、安装消费者、公开头与架构门禁。
```

```text
测试：补齐搜索建议框多主题视觉基线

新增三主题、四档DPR的输入与建议popup参考图。
验证focus、disabled、read-only、icon、long text和RTL状态。
```

```text
文档：记录搜索建议框批次交付结果

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

每个代码提交运行对应 target 与 CTest；最终运行 GCC Release shared/static、Clang ASan+UBSan、shared/static `ZzClangTidy`、public headers、fresh install consumer、四档截图、benchmark 与画廊 smoke。

## 12. 完成定义

- `ZzSuggestBox` 安装接口完整，所有自定义类型使用 `Zz` 前缀并包含简体中文 Doxygen。
- 组件只有一份建议值集合；输入、completion、popup、焦点、键盘、IME 和无障碍状态由 Qt 管理。
- 重复文本可按 index 返回正确 key/icon/data；增删/reset 的模型通知完整，负索引和重复 key 安全。
- 每实例对象在构造后稳定，没有动画、timer、QSS、动态属性、手工 popup window 或逐建议 QObject。
- Light、Dark、HighContrast x 四档 DPR 视觉基线通过；参考机渲染 P95 满足 16.7ms。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

