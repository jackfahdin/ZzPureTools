#pragma once

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/Qt>

#include <ZzFluentUI/ZzSuggestBox.h>

QT_BEGIN_NAMESPACE
class QCompleter;
class QModelIndex;
class QListView;
QT_END_NAMESPACE

namespace ZzFluentUI {

class ZzFluentItemDelegate;
class ZzSuggestionListModel;

/** @brief 管理搜索建议值模型与 Qt completer 的一次性装配。 */
class ZzSuggestBoxPrivate final
{
public:
    /**
     * @brief 创建值模型、completer、popup 和 Fluent delegate。
     * @param q 非空、非拥有的公开搜索建议框。
     */
    explicit ZzSuggestBoxPrivate(ZzSuggestBox *q);

    /** @brief 一次性替换建议集合并在可见时刷新 popup。 */
    void setSuggestions(QList<ZzSuggestion> suggestions);

    /** @brief 返回当前建议集合副本。 */
    [[nodiscard]] QList<ZzSuggestion> suggestions() const;

    /** @brief 返回模型当前行数。 */
    [[nodiscard]] int suggestionCount() const noexcept;

    /** @brief 追加建议并返回规范化后的唯一键。 */
    [[nodiscard]] QString addSuggestion(ZzSuggestion suggestion);

    /** @brief 按唯一键删除建议。 */
    [[nodiscard]] bool removeSuggestion(const QString &key);

    /** @brief 按行号删除建议。 */
    [[nodiscard]] bool removeSuggestionAt(int index);

    /** @brief 清空全部建议并关闭 popup。 */
    [[nodiscard]] bool clearSuggestions();

    /** @brief 把 completion model index 转换为公开值快照。 */
    [[nodiscard]] static ZzSuggestion suggestionFromIndex(
        const QModelIndex &index);

    /** @brief 返回是否为 QCompleter 支持的单一过滤标志。 */
    [[nodiscard]] static bool isSupportedFilterMode(
        Qt::MatchFlag mode) noexcept;

    /** @brief 在建议 popup 可见时按当前输入刷新匹配结果。 */
    void refreshVisiblePopup();

    ZzSuggestBox *const q_ptr;
    ZzSuggestionListModel *model = nullptr;
    QCompleter *completer = nullptr;
    QListView *popup = nullptr;
    ZzFluentItemDelegate *delegate = nullptr;
};

} // namespace ZzFluentUI
