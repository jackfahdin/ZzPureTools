#include <ZzFluentUI/ZzActionCard.h>

#include <algorithm>
#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QSizePolicy>

#include "private/ZzActionCardPrivate.h"

namespace ZzFluentUI {

ZzActionCard::ZzActionCard(QWidget *parent)
    : QAbstractButton(parent)
    , d_ptr(std::make_unique<ZzActionCardPrivate>(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

ZzActionCard::ZzActionCard(
    const QString &text,
    const QString &description,
    QWidget *parent)
    : ZzActionCard(parent)
{
    setText(text);
    setDescription(description);
}

ZzActionCard::~ZzActionCard() = default;

QString ZzActionCard::description() const
{
    return d_ptr->description;
}

void ZzActionCard::setDescription(QString description)
{
    if (d_ptr->description == description) {
        return;
    }
    const bool updateAccessibleDescription =
        accessibleDescription().isEmpty()
        || accessibleDescription() == d_ptr->description;
    d_ptr->description = std::move(description);
    if (updateAccessibleDescription) {
        setAccessibleDescription(d_ptr->description);
    }
    updateGeometry();
    update();
    Q_EMIT descriptionChanged(d_ptr->description);
}

bool ZzActionCard::isTrailingIndicatorVisible() const noexcept
{
    return d_ptr->trailingIndicatorVisible;
}

void ZzActionCard::setTrailingIndicatorVisible(bool visible)
{
    if (d_ptr->trailingIndicatorVisible == visible) {
        return;
    }
    d_ptr->trailingIndicatorVisible = visible;
    updateGeometry();
    update();
    Q_EMIT trailingIndicatorVisibleChanged(visible);
}

QSize ZzActionCard::sizeHint() const
{
    QFont titleFont = font();
    titleFont.setWeight(QFont::DemiBold);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descriptionMetrics(font());
    int width = std::max(
        titleMetrics.horizontalAdvance(text()),
        descriptionMetrics.horizontalAdvance(d_ptr->description));
    width += 24;
    if (!icon().isNull()) {
        width += std::max(iconSize().width(), 16) + 10;
    }
    if (d_ptr->trailingIndicatorVisible) {
        width += 26;
    }
    return QSize(std::clamp(width, 240, 480), 80);
}

QSize ZzActionCard::minimumSizeHint() const
{
    return QSize(180, 64);
}

void ZzActionCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    d_ptr->paint(&painter);
}

void ZzActionCard::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr
        && !event->isAutoRepeat()
        && (event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Return)) {
        click();
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void ZzActionCard::changeEvent(QEvent *event)
{
    QAbstractButton::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
        updateGeometry();
        update();
        break;
    case QEvent::EnabledChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::PaletteChange:
        update();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
