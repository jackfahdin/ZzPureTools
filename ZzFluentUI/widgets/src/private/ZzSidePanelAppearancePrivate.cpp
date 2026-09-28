#include "ZzSidePanelAppearancePrivate.h"

#include <QtCore/QThread>
#include <QtWidgets/QLayout>
#include <QtWidgets/QTreeView>

#include <ZzFluentUI/ZzFluentItemDelegate.h>

namespace ZzFluentUI {

namespace {
constexpr int zzSidePanelContentMargin = 12;
constexpr int zzSidePanelContentSpacing = 8;
constexpr int zzSidePanelTreeIndentation = 16;
} // namespace

void ZzSidePanelAppearancePrivate::applyTreeView(QTreeView *view)
{
    if (view == nullptr) {
        return;
    }
    Q_ASSERT(view->thread() == QThread::currentThread());
    if (view->thread() != QThread::currentThread()) {
        return;
    }
    view->setFrameShape(QFrame::NoFrame);
    view->setBackgroundRole(QPalette::Window);
    view->viewport()->setBackgroundRole(QPalette::Window);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setUniformRowHeights(true);
    view->setIndentation(zzSidePanelTreeIndentation);
    view->setMouseTracking(true);
    view->viewport()->setMouseTracking(true);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *delegate = qobject_cast<ZzFluentItemDelegate *>(view->itemDelegate());
    if (delegate == nullptr) {
        delegate = new ZzFluentItemDelegate(view);
        view->setItemDelegate(delegate);
    }
    delegate->setDensity(ZzItemDensity::Standard);
}

void ZzSidePanelAppearancePrivate::applyFormLayout(QLayout *layout)
{
    if (layout == nullptr) {
        return;
    }
    Q_ASSERT(layout->thread() == QThread::currentThread());
    if (layout->thread() != QThread::currentThread()) {
        return;
    }
    layout->setContentsMargins(
        zzSidePanelContentMargin, zzSidePanelContentMargin,
        zzSidePanelContentMargin, zzSidePanelContentMargin);
    layout->setSpacing(zzSidePanelContentSpacing);
}

} // namespace ZzFluentUI
