#include <ZzFluentUI/ZzSplitButton.h>

#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QtMath>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include "private/ZzSplitButtonPrivate.h"

namespace ZzFluentUI {

ZzSplitButton::ZzSplitButton(QWidget *parent)
    : QPushButton(parent)
    , d_ptr(std::make_unique<ZzSplitButtonPrivate>(this))
{
}

ZzSplitButton::ZzSplitButton(
    const QString &text,
    QWidget *parent)
    : QPushButton(text, parent)
    , d_ptr(std::make_unique<ZzSplitButtonPrivate>(this))
{
}

ZzSplitButton::~ZzSplitButton() = default;

ZzButtonAppearance ZzSplitButton::appearance() const noexcept
{
    return d_ptr->appearance;
}

void ZzSplitButton::setAppearance(ZzButtonAppearance appearance)
{
    d_ptr->setAppearance(appearance);
}

QMenu *ZzSplitButton::menu() const noexcept
{
    return d_ptr->menu.data();
}

void ZzSplitButton::setMenu(QMenu *menu)
{
    d_ptr->setMenu(menu);
}

QSize ZzSplitButton::sizeHint() const
{
    QSize result = QPushButton::sizeHint();
    const auto snapshot = d_ptr->theme.snapshot();
    result.rwidth() += qCeil(
        snapshot->metric(ZzMetricToken::SplitButtonMenuExtent));
    result.setHeight(std::max(
        result.height(),
        qCeil(snapshot->metric(ZzMetricToken::ControlHeight))));
    return result;
}

void ZzSplitButton::showMenu()
{
    d_ptr->showMenu();
}

void ZzSplitButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    d_ptr->paint(&painter);
}

bool ZzSplitButton::hitButton(const QPoint &position) const
{
    return d_ptr->regions().main.contains(position)
        && QPushButton::hitButton(position);
}

void ZzSplitButton::mouseMoveEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }
    d_ptr->updateHover(event->position().toPoint());
    if (d_ptr->menuPressed) {
        d_ptr->menuArmed = d_ptr->regions().menu.contains(
            event->position().toPoint());
        update();
        event->accept();
        return;
    }
    QPushButton::mouseMoveEvent(event);
}

void ZzSplitButton::mousePressEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }
    const QPoint position = event->position().toPoint();
    d_ptr->updateHover(position);
    if (event->button() == Qt::LeftButton
        && isEnabled()
        && d_ptr->regions().menu.contains(position)) {
        setDown(false);
        d_ptr->menuPressed = true;
        d_ptr->menuArmed = true;
        update();
        event->accept();
        return;
    }
    d_ptr->menuPressed = false;
    d_ptr->menuArmed = false;
    QPushButton::mousePressEvent(event);
}

void ZzSplitButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }
    if (event->button() == Qt::LeftButton && d_ptr->menuPressed) {
        const bool activate = isEnabled()
            && d_ptr->regions().menu.contains(
                event->position().toPoint());
        d_ptr->menuPressed = false;
        d_ptr->menuArmed = false;
        d_ptr->updateHover(event->position().toPoint());
        event->accept();
        if (activate) {
            d_ptr->showMenu();
        }
        update();
        return;
    }
    QPushButton::mouseReleaseEvent(event);
    d_ptr->updateHover(event->position().toPoint());
}

void ZzSplitButton::leaveEvent(QEvent *event)
{
    d_ptr->mainHovered = false;
    d_ptr->menuHovered = false;
    if (d_ptr->menuPressed) {
        d_ptr->menuArmed = false;
    }
    update();
    QPushButton::leaveEvent(event);
}

void ZzSplitButton::keyPressEvent(QKeyEvent *event)
{
    if (event == nullptr) {
        return;
    }
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (event->key() == Qt::Key_Down
        && (modifiers == Qt::NoModifier
            || modifiers == Qt::AltModifier)) {
        d_ptr->showMenu();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Enter
         || event->key() == Qt::Key_Return)
        && modifiers == Qt::NoModifier) {
        click();
        event->accept();
        return;
    }
    QPushButton::keyPressEvent(event);
}

void ZzSplitButton::changeEvent(QEvent *event)
{
    QPushButton::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::EnabledChange:
        if (!isEnabled()) {
            d_ptr->mainHovered = false;
            d_ptr->menuHovered = false;
            d_ptr->menuPressed = false;
            d_ptr->menuArmed = false;
        }
        d_ptr->refreshTheme();
        break;
    case QEvent::DevicePixelRatioChange:
    case QEvent::FontChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        d_ptr->refreshTheme();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
