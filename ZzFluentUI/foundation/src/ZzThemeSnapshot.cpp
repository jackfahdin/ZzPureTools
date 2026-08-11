#include <ZzFluentUI/ZzThemeSnapshot.h>

#include <utility>

#include <QtCore/QThread>
#include <QtCore/QtGlobal>
#include <QtGui/QGuiApplication>

namespace ZzFluentUI {

namespace {

template<typename ZzArray, typename ZzToken>
std::size_t zzCheckedIndex(
    const ZzArray &values,
    ZzToken token) noexcept
{
    const auto index = static_cast<std::size_t>(token);
    if (index >= values.size()) {
        Q_ASSERT(false);
        return 0;
    }
    return index;
}

QFont zzScaledFont(
    const QFont &base,
    qreal scale,
    qreal fallbackPointSize)
{
    QFont result(base);
    if (base.pointSizeF() > 0.0) {
        result.setPointSizeF(base.pointSizeF() * scale);
    } else if (base.pixelSize() > 0) {
        result.setPixelSize(qMax(1, qRound(base.pixelSize() * scale)));
    } else {
        result.setPointSizeF(fallbackPointSize);
    }
    return result;
}

} // namespace

ZzThemeSnapshot::ZzThemeSnapshot(
    const ZzThemePalette &palette,
    const std::array<
        qreal,
        static_cast<std::size_t>(ZzMetricToken::Count)> &metrics,
    std::array<
        QFont,
        static_cast<std::size_t>(ZzTypographyToken::Count)> fonts,
    const std::array<
        int,
        static_cast<std::size_t>(ZzMotionToken::Count)> &durations,
    quint64 revision,
    bool reducedMotion)
    : palette_(palette)
    , metrics_(metrics)
    , fonts_(std::move(fonts))
    , durations_(durations)
    , revision_(revision)
    , reducedMotion_(reducedMotion)
{
}

ZzThemeSnapshot ZzThemeSnapshot::create(
    ZzThemeMode mode,
    QColor accent,
    quint64 revision,
    bool reducedMotion)
{
    Q_ASSERT(QGuiApplication::instance() != nullptr);
    Q_ASSERT(
        QThread::currentThread() == QGuiApplication::instance()->thread());
    if (mode == ZzThemeMode::System) {
        mode = ZzThemeMode::Light;
    }

    const std::array<
        qreal,
        static_cast<std::size_t>(ZzMetricToken::Count)> metrics{
        2.0,
        4.0,
        1.0,
        2.0,
        32.0,
        12.0,
        6.0,
        16.0,
        20.0,
        24.0,
        320.0,
        548.0,
        16.0,
        8.0,
        320.0,
        3.0,
        16.0,
        320.0};
    QFont base = QGuiApplication::font();
    if (base.family().isEmpty()) {
        base.setFamily(QStringLiteral("Sans Serif"));
    }
    QFont caption = zzScaledFont(base, 0.9, 9.0);
    QFont body = zzScaledFont(base, 1.0, 10.0);
    QFont strong(body);
    strong.setWeight(QFont::DemiBold);
    QFont subtitle = zzScaledFont(strong, 1.4, 14.0);
    QFont title = zzScaledFont(strong, 2.0, 20.0);
    const std::array<QFont, 5> fonts{
        caption,
        body,
        strong,
        subtitle,
        title};
    std::array<int, 4> durations{83, 167, 250, 250};
    if (reducedMotion) {
        durations.fill(0);
    }
    return ZzThemeSnapshot(
        ZzThemePalette::create(mode, accent),
        metrics,
        fonts,
        durations,
        revision,
        reducedMotion);
}

QColor ZzThemeSnapshot::color(ZzColorToken token) const noexcept
{
    return palette_.color(token);
}

qreal ZzThemeSnapshot::metric(ZzMetricToken token) const noexcept
{
    return metrics_[zzCheckedIndex(metrics_, token)];
}

QFont ZzThemeSnapshot::font(ZzTypographyToken token) const
{
    return fonts_[zzCheckedIndex(fonts_, token)];
}

int ZzThemeSnapshot::duration(ZzMotionToken token) const noexcept
{
    return durations_[zzCheckedIndex(durations_, token)];
}

quint64 ZzThemeSnapshot::revision() const noexcept
{
    return revision_;
}

bool ZzThemeSnapshot::reducedMotion() const noexcept
{
    return reducedMotion_;
}

} // namespace ZzFluentUI
