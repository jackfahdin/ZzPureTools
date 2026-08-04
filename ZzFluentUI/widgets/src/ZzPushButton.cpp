#include <ZzFluentUI/ZzPushButton.h>

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
    update();
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
