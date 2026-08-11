#include <ZzFluentUI/ZzPasswordBox.h>

#include <QtCore/QEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QResizeEvent>

#include "private/ZzPasswordBoxPrivate.h"

namespace ZzFluentUI {

ZzPasswordBox::ZzPasswordBox(QWidget *parent)
    : QLineEdit(parent)
    , d_ptr(std::make_unique<ZzPasswordBoxPrivate>(this))
{
    qRegisterMetaType<ZzPasswordRevealMode>();
}

ZzPasswordBox::~ZzPasswordBox() = default;

ZzPasswordRevealMode ZzPasswordBox::revealMode() const noexcept
{
    return d_ptr->revealMode;
}

void ZzPasswordBox::setRevealMode(ZzPasswordRevealMode mode)
{
    d_ptr->setRevealMode(mode);
}

bool ZzPasswordBox::isPasswordVisible() const noexcept
{
    return d_ptr->isPasswordVisible();
}

bool ZzPasswordBox::event(QEvent *event)
{
    if (event != nullptr
        && (event->type() == QEvent::WindowDeactivate
            || event->type() == QEvent::Hide)) {
        d_ptr->endPeek();
    }
    const bool handled = QLineEdit::event(event);
    if (event != nullptr
        && event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->refreshPresentation();
    }
    return handled;
}

void ZzPasswordBox::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    d_ptr->syncButtonGeometry();
}

void ZzPasswordBox::changeEvent(QEvent *event)
{
    QLineEdit::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::EnabledChange:
        if (!isEnabled()) {
            d_ptr->endPeek();
        }
        d_ptr->refreshPresentation();
        break;
    case QEvent::FontChange:
    case QEvent::LanguageChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        d_ptr->refreshTheme();
        break;
    default:
        break;
    }
}

void ZzPasswordBox::focusOutEvent(QFocusEvent *event)
{
    d_ptr->endPeek();
    QLineEdit::focusOutEvent(event);
}

} // namespace ZzFluentUI
