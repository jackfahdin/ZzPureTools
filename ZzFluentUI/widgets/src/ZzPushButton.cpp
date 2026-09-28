#include <ZzFluentUI/ZzPushButton.h>

#include <QtCore/QEvent>

#include <QtWidgets/QStylePainter>

#include "private/ZzPushButtonPrivate.h"

namespace ZzFluentUI {

ZzPushButton::ZzPushButton(QWidget *parent)
    : QPushButton(parent)
    , d_ptr(std::make_unique<ZzPushButtonPrivate>(this))
{
}

ZzPushButton::ZzPushButton(
    const QString &text,
    QWidget *parent)
    : QPushButton(text, parent)
    , d_ptr(std::make_unique<ZzPushButtonPrivate>(this))
{
}

ZzPushButton::~ZzPushButton() = default;

ZzButtonAppearance ZzPushButton::appearance() const noexcept
{
    return d_ptr->appearance;
}

void ZzPushButton::setAppearance(ZzButtonAppearance appearance)
{
    if (d_ptr->appearance == appearance) {
        return;
    }
    d_ptr->appearance = appearance;
    setProperty("accent", appearance == ZzButtonAppearance::Accent);
    update();
}

bool ZzPushButton::event(QEvent *event)
{
    const bool handled = QPushButton::event(event);
    if (d_ptr && event->type() == QEvent::DynamicPropertyChange
        && static_cast<const QDynamicPropertyChangeEvent *>(event)->propertyName() == "accent") {
        if (property("accent").toBool()) {
            setAppearance(ZzButtonAppearance::Accent);
        } else if (appearance() == ZzButtonAppearance::Accent) {
            setAppearance(ZzButtonAppearance::Standard);
        }
    }
    return handled;
}

void ZzPushButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStyleOptionButton option;
    d_ptr->initStyleOption(&option);
    QStylePainter painter(this);
    painter.drawControl(QStyle::CE_PushButton, option);
}

} // namespace ZzFluentUI
