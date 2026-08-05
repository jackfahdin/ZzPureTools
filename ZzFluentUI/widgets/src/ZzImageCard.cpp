#include <ZzFluentUI/ZzImageCard.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QSizePolicy>

#include "private/ZzImageCardPrivate.h"

namespace ZzFluentUI {

namespace {

/** @brief 把任意枚举值收敛为三个受支持的图片策略。 */
Qt::AspectRatioMode zzNormalizedAspectRatioMode(Qt::AspectRatioMode mode)
{
    switch (mode) {
    case Qt::IgnoreAspectRatio:
    case Qt::KeepAspectRatio:
    case Qt::KeepAspectRatioByExpanding:
        return mode;
    }
    return Qt::KeepAspectRatioByExpanding;
}

} // namespace

ZzImageCard::ZzImageCard(QWidget *parent)
    : QAbstractButton(parent)
    , d_ptr(std::make_unique<ZzImageCardPrivate>(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

ZzImageCard::ZzImageCard(
    const QString &text,
    const QString &description,
    QWidget *parent)
    : ZzImageCard(parent)
{
    setText(text);
    setDescription(description);
}

ZzImageCard::~ZzImageCard() = default;

QPixmap ZzImageCard::pixmap() const
{
    return d_ptr->pixmap;
}

void ZzImageCard::setPixmap(QPixmap pixmap)
{
    if (d_ptr->pixmap.cacheKey() == pixmap.cacheKey()
        && d_ptr->pixmap.size() == pixmap.size()
        && d_ptr->pixmap.devicePixelRatioF()
            == pixmap.devicePixelRatioF()) {
        return;
    }
    d_ptr->pixmap = std::move(pixmap);
    update();
    Q_EMIT pixmapChanged(d_ptr->pixmap);
}

QString ZzImageCard::description() const
{
    return d_ptr->description;
}

void ZzImageCard::setDescription(QString description)
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

Qt::AspectRatioMode ZzImageCard::aspectRatioMode() const noexcept
{
    return d_ptr->aspectRatioMode;
}

void ZzImageCard::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    const Qt::AspectRatioMode normalized =
        zzNormalizedAspectRatioMode(mode);
    if (d_ptr->aspectRatioMode == normalized) {
        return;
    }
    d_ptr->aspectRatioMode = normalized;
    update();
    Q_EMIT aspectRatioModeChanged(normalized);
}

QSize ZzImageCard::sizeHint() const
{
    return QSize(320, 260);
}

QSize ZzImageCard::minimumSizeHint() const
{
    return QSize(200, 180);
}

void ZzImageCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    d_ptr->paint(&painter);
}

void ZzImageCard::keyPressEvent(QKeyEvent *event)
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

void ZzImageCard::changeEvent(QEvent *event)
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
    case QEvent::DevicePixelRatioChange:
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
