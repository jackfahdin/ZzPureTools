#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <ZzFluentUI/ZzIconAssets.h>

#include <QtCore/QThread>
#include <QtGui/QFontDatabase>
#include <QtGui/QGuiApplication>
#include <QtGui/QIconEngine>
#include <QtGui/QPainter>
#include <QtGui/QPalette>

namespace ZzFluentUI {
namespace {

class ZzSegoeIconEngine final : public QIconEngine
{
public:
    ZzSegoeIconEngine(ZzSegoeIcon glyph, QColor color, bool gallery = false, int pixelSize = 25, int canvasSize = 30)
        : glyph_(glyph), color_(color), gallery_(gallery), pixelSize_(pixelSize), canvasSize_(canvasSize) {}

    QIconEngine *clone() const override { return new ZzSegoeIconEngine(*this); }
    QString iconName() override
    {
        if (gallery_) return (color_.isValid() ? QStringLiteral("zz-segoe-gallery-fixed:") : QStringLiteral("zz-segoe-gallery:"))
            + QString::number(static_cast<uint>(glyph_), 16) + QLatin1Char(':') + QString::number(pixelSize_)
            + QLatin1Char(':') + QString::number(canvasSize_);
        return (color_.isValid() ? QStringLiteral("zz-segoe-fixed:")
                                : QStringLiteral("zz-segoe-foreground:"))
            + QString::number(static_cast<uint>(glyph_), 16);
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        if (rect.isEmpty()) return;
        if (gallery_) {
            const auto pixmap = scaledPixmap(rect.size(), mode, state, painter->device()->devicePixelRatioF());
            const QSizeF size = pixmap.deviceIndependentSize();
            const QPointF origin = QRectF(rect).center() - QPointF(size.width() / 2, size.height() / 2);
            painter->drawPixmap(origin, pixmap);
            return;
        }
        const auto palette = QGuiApplication::palette();
        const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
        const auto role = mode == QIcon::Selected ? QPalette::HighlightedText : QPalette::WindowText;
        painter->save();
        painter->setPen(color_.isValid() ? color_ : palette.color(group, role));
        painter->setFont(ZzSegoeIconFont::font(qMin(rect.width(), rect.height())));
        painter->drawText(rect, Qt::AlignCenter, QString(QChar(static_cast<char16_t>(glyph_))));
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        if (size.isEmpty() || scale <= 0) return {};
        if (gallery_) {
            const int canvas = canvasSize_;
            QPixmap original(QSize(canvas, canvas) * scale);
            original.setDevicePixelRatio(scale);
            original.fill(Qt::transparent);
            QPainter painter(&original);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
            painter.setFont(ZzSegoeIconFont::font(pixelSize_));
            painter.setPen(color_.isValid() ? color_ : QGuiApplication::palette().color(
                mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::WindowText));
            painter.drawText(QRect(0, 0, canvas, canvas), Qt::AlignCenter, QString(QChar(static_cast<char16_t>(glyph_))));
            painter.end();
            auto result = original.scaled(size * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            result.setDevicePixelRatio(scale);
            return result;
        }
        // Qt 6.8+ 传入逻辑尺寸；只在这里换算一次物理尺寸。
        QPixmap result(size * scale);
        result.setDevicePixelRatio(scale);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return result;
    }

private:
    ZzSegoeIcon glyph_;
    QColor color_;
    bool gallery_ = false;
    int pixelSize_ = 0;
    int canvasSize_ = 0;
};

} // namespace

bool ZzSegoeIconFont::ensureRegistered()
{
    Q_ASSERT(qGuiApp && QThread::currentThread() == qGuiApp->thread());
    if (!qGuiApp || QThread::currentThread() != qGuiApp->thread()) return false;
    static const bool registered = [] {
        if (!ZzIconAssets::ensureInitialized()) return false;
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/zzfluent/fonts/SegoeFluentIcons.ttf"));
        return id >= 0 && QFontDatabase::applicationFontFamilies(id).contains(familyName());
    }();
    return registered;
}

QString ZzSegoeIconFont::familyName() { return QStringLiteral("Segoe Fluent Icons"); }

QFont ZzSegoeIconFont::font(int pixelSize)
{
    QFont result;
    if (!ensureRegistered()) return result;
    result.setFamily(familyName());
    result.setPixelSize(qMax(1, pixelSize));
    result.setWeight(QFont::Normal);
    result.setStyleStrategy(QFont::NoFontMerging);
    return result;
}

QIcon ZzSegoeIconFont::icon(ZzSegoeIcon glyph, const QColor &color)
{
    if (!ensureRegistered()) return {};
    return QIcon(new ZzSegoeIconEngine(glyph, color));
}

QIcon ZzSegoeIconFont::galleryIcon(ZzSegoeIcon glyph, int pixelSize, int canvasSize)
{
    if (!ensureRegistered()) return {};
    return QIcon(new ZzSegoeIconEngine(glyph, {}, true, pixelSize > 0 ? pixelSize : 25,
        canvasSize > 0 ? canvasSize : pixelSize > 0 ? pixelSize : 30));
}

bool ZzSegoeIconFont::usesForegroundColor(const QIcon &icon)
{
    return icon.name().startsWith(QStringLiteral("zz-segoe-foreground:"));
}

QIcon ZzSegoeIconFont::withForegroundColor(const QIcon &source, const QColor &color)
{
    const bool gallery = source.name().startsWith(QStringLiteral("zz-segoe-gallery:"));
    if (!color.isValid() || (!usesForegroundColor(source) && !gallery)) return source;
    // QIcon 不公开 engine；由私有 engine 名称携带字形身份，复制 QIcon 后仍可恢复。
    bool valid = false;
    const uint glyph = source.name().section(QLatin1Char(':'), 1, 1).toUInt(&valid, 16);
    if (!valid || glyph > 0xffffU) return source;
    if (gallery) return QIcon(new ZzSegoeIconEngine(static_cast<ZzSegoeIcon>(glyph), color, true,
        source.name().section(QLatin1Char(':'), 2, 2).toInt(), source.name().section(QLatin1Char(':'), 3, 3).toInt()));
    return icon(static_cast<ZzSegoeIcon>(glyph), color);
}

} // namespace ZzFluentUI
