#include "ZzPushButton.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include "ZzApplication.h"
#include "ZzFluentPainter.h"
#include "ZzTheme.h"
#include "private/ZzPushButtonPrivate.h"

ZzPushButtonPrivate::ZzPushButtonPrivate(ZzPushButton* q)
    : q_ptr(q)
{
}

ZzPushButton::ZzPushButton(QWidget* parent)
    : QPushButton(parent)
    , d_ptr(new ZzPushButtonPrivate(this))
{
    initialize();
}

ZzPushButton::ZzPushButton(const QString& text, QWidget* parent)
    : ZzPushButton(parent)
{
    setText(text);
}

ZzPushButton::~ZzPushButton()
{
    delete d_ptr;
}

void ZzPushButton::setButtonStyle(ZzButtonStyle style)
{
    if (d_ptr->buttonStyle == style) {
        return;
    }

    d_ptr->buttonStyle = style;
    update();
}

ZzPushButton::ZzButtonStyle ZzPushButton::buttonStyle() const noexcept
{
    return d_ptr->buttonStyle;
}

void ZzPushButton::initialize()
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setMinimumHeight(ZzApplication::instance().theme() != nullptr
        ? ZzApplication::instance().theme()->metrics().controlHeight
        : 32);

    setAttribute(Qt::WA_Hover, true);
    setFlat(true);
    setStyleSheet(QStringLiteral("border: none; background: transparent;"));

    bindTheme();
}

void ZzPushButton::bindTheme()
{
    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        return;
    }

    connect(theme, &ZzTheme::colorsChanged, this, [this]() {
        update();
    });
    connect(theme, &ZzTheme::metricsChanged, this, [this]() {
        setMinimumHeight(ZzApplication::instance().theme()->metrics().controlHeight);
        update();
    });
}

ZzControlState ZzPushButton::controlState() const
{
    if (!isEnabled()) {
        return ZzControlState::Disabled;
    }
    if (d_ptr->pressed) {
        return ZzControlState::Pressed;
    }
    if (d_ptr->hovered) {
        return ZzControlState::Hovered;
    }
    return ZzControlState::Normal;
}

void ZzPushButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    ZzTheme* theme = ZzApplication::instance().theme();
    if (theme == nullptr) {
        QPushButton::paintEvent(event);
        return;
    }

    QPainter painter(this);
    const QRectF rect = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
    const ZzControlState state = controlState();

    if (d_ptr->buttonStyle == ZzButtonStyle::Accent) {
        ZzFluentPainter::drawAccentButton(painter, rect, theme->colors(), theme->metrics(), state);
        const QColor textColor = state == ZzControlState::Disabled ? theme->colors().textDisabled : Qt::white;
        ZzFluentPainter::drawButtonText(painter, rect, text(), textColor);
        return;
    }

    ZzFluentPainter::drawControlSurface(painter, rect, theme->colors(), theme->metrics(), state);
    ZzFluentPainter::drawButtonText(painter, rect, text(), theme->textColor(state));
}

void ZzPushButton::enterEvent(QEnterEvent* event)
{
    d_ptr->hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void ZzPushButton::leaveEvent(QEvent* event)
{
    d_ptr->hovered = false;
    d_ptr->pressed = false;
    update();
    QPushButton::leaveEvent(event);
}

void ZzPushButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        d_ptr->pressed = true;
        update();
    }
    QPushButton::mousePressEvent(event);
}

void ZzPushButton::mouseReleaseEvent(QMouseEvent* event)
{
    d_ptr->pressed = false;
    update();
    QPushButton::mouseReleaseEvent(event);
}
