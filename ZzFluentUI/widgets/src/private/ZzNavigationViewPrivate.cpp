#include "ZzNavigationViewPrivate.h"

#include <QtWidgets/QStyledItemDelegate>

#include <ZzFluentUI/ZzNavigationView.h>

namespace ZzFluentUI {

namespace {

/** @brief 紧凑模式只清空复制 option 的展示文本。 */
class ZzNavigationViewDelegate final : public QStyledItemDelegate
{
public:
    /** @brief 绑定导航视图并由其 QObject parent 管理生命周期。 */
    explicit ZzNavigationViewDelegate(ZzNavigationView *view)
        : QStyledItemDelegate(view)
        , view_(view)
    {
    }

    /** @brief 保留平台 delegate 绘制，仅在紧凑模式隐藏文本。 */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        QStyleOptionViewItem adjusted = option;
        if (view_ != nullptr && view_->isCompact()) {
            adjusted.text.clear();
            adjusted.decorationPosition = QStyleOptionViewItem::Top;
            adjusted.decorationAlignment = Qt::AlignCenter;
        }
        QStyledItemDelegate::paint(painter, adjusted, index);
    }

private:
    ZzNavigationView *const view_;
};

} // namespace

ZzNavigationViewPrivate::ZzNavigationViewPrivate(
    ZzNavigationView *publicObject) noexcept
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->setItemDelegate(new ZzNavigationViewDelegate(q_ptr));
}

void ZzNavigationViewPrivate::activateIndex(
    const QModelIndex &index)
{
    if (!index.isValid()
        || !index.flags().testFlag(Qt::ItemIsEnabled)) {
        return;
    }
    Q_EMIT q_ptr->navigationRequested(index);
}

} // namespace ZzFluentUI
