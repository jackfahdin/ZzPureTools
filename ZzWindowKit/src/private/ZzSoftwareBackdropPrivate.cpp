#include "ZzSoftwareBackdropPrivate.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QtCore/QChildEvent>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

#include "ZzSoftwareBackdrop.h"

namespace ZzWindowKit {

namespace {

constexpr int kTextureCellExtent = 64;
constexpr int kTextureAlpha = 16;

class ZzSoftwareBackdropLayer final : public QWidget
{
public:
    explicit ZzSoftwareBackdropLayer(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("zzSoftwareBackdropLayer"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        hide();
    }

    void setTexture(QPixmap texture)
    {
        texture_ = std::move(texture);
        update();
    }

    void clearTexture()
    {
        texture_ = {};
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (texture_.isNull()) {
            return;
        }
        QPainter painter(this);
        painter.drawTiledPixmap(rect(), texture_);
    }

private:
    QPixmap texture_;
};

[[nodiscard]] ZzSoftwareBackdropLayer *zzLayer(
    const QPointer<QWidget> &layer)
{
    return static_cast<ZzSoftwareBackdropLayer *>(layer.data());
}

[[nodiscard]] QColor zzMaterialAccent(QColor base)
{
    const auto light = base.lightnessF() >= 0.5;
    base = light ? base.darker(108) : base.lighter(112);
    base.setAlpha(kTextureAlpha);
    return base;
}

} // namespace

ZzSoftwareBackdropPrivate::ZzSoftwareBackdropPrivate(
    ZzSoftwareBackdrop *q)
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

ZzSoftwareBackdropPrivate::~ZzSoftwareBackdropPrivate()
{
    detach();
}

bool ZzSoftwareBackdropPrivate::attach(QWidget *host)
{
    if (host_ != nullptr || host == nullptr
        || QThread::currentThread() != q_ptr->thread()
        || host->thread() != q_ptr->thread() || !host->isWindow()) {
        return false;
    }

    auto *layer = new ZzSoftwareBackdropLayer(host);
    layer_ = layer;
    host_ = host;
    host->installEventFilter(q_ptr);
    hostDestroyedConnection_ = QObject::connect(
        host,
        &QObject::destroyed,
        q_ptr,
        [this] {
            QObject::disconnect(hostDestroyedConnection_);
            host_.clear();
            layer_.clear();
            enabled_ = false;
        });
    layer->setGeometry(host->rect());
    layer->lower();
    return true;
}

void ZzSoftwareBackdropPrivate::detach()
{
    QObject::disconnect(hostDestroyedConnection_);
    if (host_ != nullptr) {
        host_->removeEventFilter(q_ptr);
    }
    if (auto *layer = zzLayer(layer_); layer != nullptr) {
        layer->hide();
        layer->clearTexture();
        delete layer;
    }
    host_.clear();
    layer_.clear();
    enabled_ = false;
}

bool ZzSoftwareBackdropPrivate::setEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return host_ != nullptr;
    }
    if (host_ == nullptr || layer_ == nullptr) {
        return false;
    }

    enabled_ = enabled;
    if (enabled_) {
        rebuild();
        showLayer();
    } else {
        hideLayer();
        if (auto *layer = zzLayer(layer_); layer != nullptr) {
            layer->clearTexture();
        }
    }
    return true;
}

bool ZzSoftwareBackdropPrivate::isEnabled() const noexcept
{
    return enabled_;
}

std::size_t ZzSoftwareBackdropPrivate::rebuildCount() const noexcept
{
    return rebuildCount_;
}

bool ZzSoftwareBackdropPrivate::eventFilter(
    QObject *watched,
    QEvent *event)
{
    if (watched != host_ || event == nullptr) {
        return false;
    }

    switch (event->type()) {
    case QEvent::Destroy:
        QObject::disconnect(hostDestroyedConnection_);
        host_.clear();
        layer_.clear();
        enabled_ = false;
        return false;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
    case QEvent::StyleChange:
    case QEvent::ScreenChangeInternal:
    case QEvent::DevicePixelRatioChange:
    case QEvent::ContentsRectChange:
        invalidate();
        break;
    case QEvent::Resize:
        if (auto *layer = zzLayer(layer_); layer != nullptr) {
            layer->setGeometry(host_->rect());
        }
        break;
    case QEvent::ChildAdded:
        if (auto *layer = zzLayer(layer_); layer != nullptr) {
            layer->lower();
        }
        break;
    case QEvent::Show:
        if (enabled_) {
            showLayer();
        }
        break;
    case QEvent::Hide:
        hideLayer();
        break;
    default:
        break;
    }
    return false;
}

void ZzSoftwareBackdropPrivate::rebuild()
{
    if (!enabled_ || host_ == nullptr || layer_ == nullptr) {
        return;
    }

    auto *layer = zzLayer(layer_);
    if (layer == nullptr) {
        return;
    }
    const auto base = host_->palette().color(QPalette::Window);
    const auto dpr = host_->devicePixelRatioF();
    const auto textureExtent = std::max(
        1,
        static_cast<int>(std::ceil(
            static_cast<qreal>(kTextureCellExtent) * dpr)));
    auto image = QImage(
        QSize(textureExtent, textureExtent),
        QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(base.rgba());

    QPainter painter(&image);
    const auto accent = zzMaterialAccent(base);
    painter.setPen(QPen(accent, 1.0));
    painter.drawLine(QPoint(0, 0), QPoint(textureExtent, textureExtent));
    painter.drawLine(
        QPoint(-textureExtent / 2, 0),
        QPoint(textureExtent / 2, textureExtent));
    painter.drawLine(
        QPoint(textureExtent / 2, 0),
        QPoint(textureExtent + textureExtent / 2, textureExtent));
    painter.end();

    layer->setGeometry(host_->rect());
    layer->setTexture(QPixmap::fromImage(std::move(image)));
    layer->lower();
    ++rebuildCount_;
}

void ZzSoftwareBackdropPrivate::invalidate()
{
    if (enabled_) {
        rebuild();
    }
}

void ZzSoftwareBackdropPrivate::showLayer()
{
    if (auto *layer = zzLayer(layer_); layer != nullptr) {
        layer->setGeometry(host_->rect());
        layer->lower();
        layer->show();
    }
}

void ZzSoftwareBackdropPrivate::hideLayer()
{
    if (auto *layer = zzLayer(layer_); layer != nullptr) {
        layer->hide();
    }
}

} // namespace ZzWindowKit
