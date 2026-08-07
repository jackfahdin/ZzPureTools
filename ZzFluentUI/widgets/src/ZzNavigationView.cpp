#include <ZzFluentUI/ZzNavigationView.h>

#include <QtGui/QHelpEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QToolTip>

#include "private/ZzNavigationViewPrivate.h"

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
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_Hover, true);
    connect(
        this,
        &QListView::clicked,
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
    d_ptr->setCompactPresentation(compact);
    viewport()->update();
    updateGeometry();
    Q_EMIT compactChanged(compact);
}

void ZzNavigationView::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr
        && (event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Space)) {
        d_ptr->activateIndex(currentIndex());
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

bool ZzNavigationView::viewportEvent(QEvent *event)
{
    if (d_ptr->compact && event != nullptr
        && event->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const QModelIndex index = indexAt(helpEvent->pos());
        QString tooltip = index.data(Qt::ToolTipRole).toString();
        if (tooltip.isEmpty()) {
            tooltip = index.data(Qt::DisplayRole).toString();
        }
        if (index.isValid() && !tooltip.isEmpty()) {
            QToolTip::showText(
                helpEvent->globalPos(),
                tooltip,
                viewport(),
                visualRect(index));
            event->accept();
            return true;
        }
    }
    return QListView::viewportEvent(event);
}

} // namespace ZzFluentUI
