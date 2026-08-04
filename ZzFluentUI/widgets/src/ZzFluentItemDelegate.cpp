#include <ZzFluentUI/ZzFluentItemDelegate.h>

#include <QtWidgets/QAbstractItemView>

#include "private/ZzFluentItemDelegatePrivate.h"

namespace ZzFluentUI {

ZzFluentItemDelegate::ZzFluentItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , d_ptr(std::make_unique<ZzFluentItemDelegatePrivate>(this))
{
}

ZzFluentItemDelegate::~ZzFluentItemDelegate() = default;

void ZzFluentItemDelegate::setDensity(ZzItemDensity density)
{
    if (d_ptr->density == density) {
        return;
    }
    d_ptr->density = density;
    if (auto *view = qobject_cast<QAbstractItemView *>(parent())) {
        view->doItemsLayout();
        view->viewport()->update();
    }
}

ZzItemDensity ZzFluentItemDelegate::density() const noexcept
{
    return d_ptr->density;
}

void ZzFluentItemDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    d_ptr->paint(painter, option, index);
}

QSize ZzFluentItemDelegate::sizeHint(
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    QSize result = QStyledItemDelegate::sizeHint(option, index);
    result.setHeight(
        d_ptr->density == ZzItemDensity::Compact ? 32 : 40);
    return result;
}

} // namespace ZzFluentUI
