#include <ZzFluentUI/ZzNavigationView.h>

#include <QtGui/QKeyEvent>

#include "private/ZzNavigationViewPrivate.h"

#include <ZzFluentUI/ZzFluentItemDelegate.h>

namespace ZzFluentUI {

ZzNavigationView::ZzNavigationView(QWidget *parent)
    : QListView(parent)
    , d_ptr(std::make_unique<ZzNavigationViewPrivate>(this))
{
    setUniformItemSizes(true);
    setLayoutMode(QListView::Batched);
    setBatchSize(64);
    setFixedWidth(240);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(
        this,
        &QListView::activated,
        this,
        [this](const QModelIndex &index) {
            d_ptr->activateIndex(index);
        });
}

ZzNavigationView::~ZzNavigationView() = default;

bool ZzNavigationView::isCompact() const noexcept
{
    return d_ptr->compact;
}

void ZzNavigationView::setCompact(bool compact)
{
    if (d_ptr->compact == compact) {
        return;
    }
    d_ptr->compact = compact;
    setFixedWidth(compact ? 48 : 240);
    if (auto *delegate = qobject_cast<ZzFluentItemDelegate *>(
            itemDelegate())) {
        delegate->setDensity(
            compact
                ? ZzItemDensity::Compact
                : ZzItemDensity::Standard);
    }
    viewport()->update();
    updateGeometry();
    Q_EMIT compactChanged(compact);
}

void ZzNavigationView::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr
        && (event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Return)) {
        d_ptr->activateIndex(currentIndex());
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

} // namespace ZzFluentUI
