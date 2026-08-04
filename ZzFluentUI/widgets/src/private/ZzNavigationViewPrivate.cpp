#include "ZzNavigationViewPrivate.h"

#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzNavigationView.h>

namespace ZzFluentUI {

ZzNavigationViewPrivate::ZzNavigationViewPrivate(
    ZzNavigationView *publicObject) noexcept
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->setItemDelegate(new ZzFluentItemDelegate(q_ptr));
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
