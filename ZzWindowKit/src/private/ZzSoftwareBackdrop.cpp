#include "ZzSoftwareBackdrop.h"

#include <utility>

#include "ZzSoftwareBackdropPrivate.h"

namespace ZzWindowKit {

ZzSoftwareBackdrop::ZzSoftwareBackdrop(QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<ZzSoftwareBackdropPrivate>(this))
{
}

ZzSoftwareBackdrop::~ZzSoftwareBackdrop()
{
    detach();
}

bool ZzSoftwareBackdrop::attach(QWidget *host)
{
    return d_ptr->attach(host);
}

void ZzSoftwareBackdrop::detach()
{
    d_ptr->detach();
}

bool ZzSoftwareBackdrop::setEnabled(bool enabled)
{
    return d_ptr->setEnabled(enabled);
}

bool ZzSoftwareBackdrop::isEnabled() const noexcept
{
    return d_ptr->isEnabled();
}

std::size_t ZzSoftwareBackdrop::rebuildCount() const noexcept
{
    return d_ptr->rebuildCount();
}

bool ZzSoftwareBackdrop::eventFilter(QObject *watched, QEvent *event)
{
    if (d_ptr->eventFilter(watched, event)) {
        return true;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzWindowKit
