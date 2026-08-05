#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTypeInfo>
#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QLineEdit>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/** @brief 保存一条可由搜索建议框展示和返回的值语义快照。 */
struct ZzSuggestion final
{
    /** @brief 调用方提供或由控件生成的唯一稳定键。 */
    QString key;

    /** @brief 在建议 popup 中展示并写回输入框的文本。 */
    QString text;

    /** @brief 可为空的建议装饰图标。 */
    QIcon icon;

    /** @brief 由调用方解释的值语义载荷，不得保存业务对象裸指针。 */
    QVariant data;

    /** @brief 是否允许用户高亮并激活该建议。 */
    bool enabled = true;
};

class ZzSuggestBoxPrivate;

/**
 * @brief 复用 QLineEdit 与 QCompleter 原生语义的 Fluent 搜索建议框。
 *
 * 输入法、光标、选择、撤销、校验、焦点和可访问输入语义由
 * QLineEdit 提供；过滤、popup 定位和键盘导航由 QCompleter 提供。
 */
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
    /**
     * @brief 创建默认不区分大小写、包含匹配的搜索建议框。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzSuggestBox(QWidget *parent = nullptr);

    /** @brief 销毁私有装配状态和由 Qt 管理的 completer 对象树。 */
    ~ZzSuggestBox() override;

    /**
     * @brief 一次性替换全部展示建议并规范化空键或重复键。
     * @param suggestions 新的值语义建议快照。
     */
    void setSuggestions(QList<ZzSuggestion> suggestions);

    /**
     * @brief 返回包含最终唯一键的全部建议副本。
     * @return 当前建议集合，顺序与 popup 一致。
     */
    [[nodiscard]] QList<ZzSuggestion> suggestions() const;

    /**
     * @brief 返回当前建议数量。
     * @return 模型中的建议行数。
     */
    [[nodiscard]] int suggestionCount() const noexcept;

    /**
     * @brief 追加一条自动生成唯一键的建议。
     * @param text 展示并写回输入框的文本。
     * @param payload 由调用方解释的值语义载荷。
     * @param icon 可为空的装饰图标。
     * @return 实际写入模型的唯一键。
     */
    [[nodiscard]] QString addSuggestion(
        QString text,
        QVariant payload = {},
        QIcon icon = {});

    /**
     * @brief 追加建议并在需要时替换空键或重复键。
     * @param suggestion 待追加的值语义快照。
     * @return 实际写入模型的唯一键。
     */
    [[nodiscard]] QString addSuggestion(ZzSuggestion suggestion);

    /**
     * @brief 按唯一键删除一条建议。
     * @param key 要删除的稳定键。
     * @return 找到并删除时返回 true，否则返回 false。
     */
    [[nodiscard]] bool removeSuggestion(const QString &key);

    /**
     * @brief 按当前展示顺序删除一条建议。
     * @param index 从零开始的行号。
     * @return 行号有效并完成删除时返回 true。
     */
    [[nodiscard]] bool removeSuggestionAt(int index);

    /** @brief 清空全部建议；集合已空时不发变化信号。 */
    void clearSuggestions();

    /**
     * @brief 设置文本过滤的大小写规则。
     * @param sensitivity Qt 标准大小写规则。
     */
    void setCaseSensitivity(Qt::CaseSensitivity sensitivity);

    /**
     * @brief 返回当前文本过滤的大小写规则。
     * @return 当前 QCompleter 大小写设置。
     */
    [[nodiscard]] Qt::CaseSensitivity caseSensitivity() const noexcept;

    /**
     * @brief 设置 starts-with、contains 或 ends-with 过滤模式。
     * @param mode 支持的单一 Qt 匹配标志；其他值保持原设置。
     */
    void setFilterMode(Qt::MatchFlag mode);

    /**
     * @brief 返回当前单一文本过滤模式。
     * @return 当前 QCompleter 匹配标志。
     */
    [[nodiscard]] Qt::MatchFlag filterMode() const noexcept;

    /**
     * @brief 设置 popup 最多同时显示的建议数量。
     * @param count 自动收敛到 1 至 100。
     */
    void setMaximumVisibleItems(int count);

    /**
     * @brief 返回 popup 最多同时显示的建议数量。
     * @return 1 至 100 范围内的数量。
     */
    [[nodiscard]] int maximumVisibleItems() const noexcept;

    /** @brief 使用当前输入文本主动打开匹配建议 popup。 */
    void showSuggestions();

    /** @brief 隐藏建议 popup，不改变输入文本或建议集合。 */
    void hideSuggestions();

    /**
     * @brief 返回建议 popup 当前是否可见。
     * @return popup 可见时返回 true。
     */
    [[nodiscard]] bool isSuggestionPopupVisible() const noexcept;

Q_SIGNALS:
    /**
     * @brief 用户通过鼠标或键盘激活一条建议后发出。
     * @param suggestion 与被激活模型行对应的完整值快照。
     */
    void suggestionActivated(const ZzSuggestion &suggestion);

    /**
     * @brief popup 当前高亮行变化后发出。
     * @param suggestion 与高亮模型行对应的完整值快照。
     */
    void suggestionHighlighted(const ZzSuggestion &suggestion);

    /** @brief 建议集合发生有效 reset、insert、remove 或 clear 后发出。 */
    void suggestionsChanged();

    /**
     * @brief 大小写规则实际变化后发出。
     * @param sensitivity 新的大小写规则。
     */
    void caseSensitivityChanged(Qt::CaseSensitivity sensitivity);

    /**
     * @brief 文本过滤模式实际变化后发出。
     * @param mode 新的单一匹配标志。
     */
    void filterModeChanged(Qt::MatchFlag mode);

    /**
     * @brief popup 最大可见项数量实际变化后发出。
     * @param count 收敛后的新数量。
     */
    void maximumVisibleItemsChanged(int count);

private:
    using QLineEdit::setCompleter;

    std::unique_ptr<ZzSuggestBoxPrivate> d_ptr;
};

} // namespace ZzFluentUI

Q_DECLARE_TYPEINFO(ZzFluentUI::ZzSuggestion, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzFluentUI::ZzSuggestion)
