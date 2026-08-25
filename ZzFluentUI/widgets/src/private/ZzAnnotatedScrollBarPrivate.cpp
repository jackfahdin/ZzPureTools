#include "ZzAnnotatedScrollBarPrivate.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QVariant>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionSlider>

#include <ZzFluentUI/ZzAnnotatedScrollBar.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

/** @brief 计算 sRGB 颜色的 WCAG 相对亮度。 */
qreal zzLuminance(const QColor &color)
{
    const auto linear = [](qreal channel) {
        return channel <= 0.04045
            ? channel / 12.92
            : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF())
        + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

/** @brief 返回两个不透明颜色的 WCAG 对比度。 */
qreal zzContrastRatio(const QColor &first, const QColor &second)
{
    const qreal firstLuminance = zzLuminance(first);
    const qreal secondLuminance = zzLuminance(second);
    return (qMax(firstLuminance, secondLuminance) + 0.05)
        / (qMin(firstLuminance, secondLuminance) + 0.05);
}

/** @brief 把模型整数安全解析为受支持的标记种类。 */
ZzScrollMarkerKind zzMarkerKind(const QVariant &value)
{
    bool converted = false;
    const int rawKind = value.toInt(&converted);
    if (!converted || rawKind < static_cast<int>(ZzScrollMarkerKind::Information)
        || rawKind > static_cast<int>(ZzScrollMarkerKind::Custom)) {
        return ZzScrollMarkerKind::Information;
    }
    return static_cast<ZzScrollMarkerKind>(rawKind);
}

} // namespace

ZzAnnotatedScrollBarPrivate::ZzAnnotatedScrollBarPrivate(
    ZzAnnotatedScrollBar *q)
    : q_ptr(q)
    , theme(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

ZzAnnotatedScrollBarPrivate::~ZzAnnotatedScrollBarPrivate()
{
    for (const QMetaObject::Connection &connection : modelConnections) {
        QObject::disconnect(connection);
    }
}

void ZzAnnotatedScrollBarPrivate::setModel(QAbstractItemModel *newModel)
{
    if (model == newModel) {
        return;
    }
    for (const QMetaObject::Connection &connection : modelConnections) {
        QObject::disconnect(connection);
    }
    modelConnections.clear();
    model = newModel;
    if (model != nullptr) {
        const auto refresh = [this] { rebuildMarkerCache(); };
        modelConnections.append(QObject::connect(
            model,
            &QAbstractItemModel::modelReset,
            q_ptr,
            refresh));
        modelConnections.append(QObject::connect(
            model,
            &QAbstractItemModel::rowsInserted,
            q_ptr,
            [refresh](const QModelIndex &, int, int) { refresh(); }));
        modelConnections.append(QObject::connect(
            model,
            &QAbstractItemModel::rowsRemoved,
            q_ptr,
            [refresh](const QModelIndex &, int, int) { refresh(); }));
        modelConnections.append(QObject::connect(
            model,
            &QAbstractItemModel::dataChanged,
            q_ptr,
            [refresh](const QModelIndex &, const QModelIndex &, const QList<int> &) {
                refresh();
            }));
        modelConnections.append(QObject::connect(
            model,
            &QAbstractItemModel::layoutChanged,
            q_ptr,
            [refresh](const QList<QPersistentModelIndex> &,
                      QAbstractItemModel::LayoutChangeHint) { refresh(); }));
        modelConnections.append(QObject::connect(
            model,
            &QObject::destroyed,
            q_ptr,
            [this] { handleModelDestroyed(); }));
    }
    rebuildMarkerCache();
    Q_EMIT q_ptr->markerModelChanged(model);
}

void ZzAnnotatedScrollBarPrivate::rebuildMarkerCache()
{
    markers.clear();
    if (model == nullptr) {
        pixelBuckets.clear();
        q_ptr->update();
        return;
    }

    const int rows = model->rowCount();
    markers.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex source = model->index(row, 0);
        if (!source.isValid()) {
            continue;
        }
        bool positionConverted = false;
        const qreal position = model->data(
                                   source,
                                   static_cast<int>(ZzScrollMarkerRole::Position))
                                   .toDouble(&positionConverted);
        if (!positionConverted || !qIsFinite(position)
            || position < 0.0 || position > 1.0) {
            continue;
        }
        const ZzScrollMarkerKind kind = zzMarkerKind(model->data(
            source,
            static_cast<int>(ZzScrollMarkerRole::Kind)));
        bool priorityConverted = false;
        const int priority = model->data(
                                 source,
                                 static_cast<int>(ZzScrollMarkerRole::Priority))
                                 .toInt(&priorityConverted);
        const QColor customColor = model->data(
            source,
            static_cast<int>(ZzScrollMarkerRole::Color)).value<QColor>();
        markers.append(ZzMarker{
            QPersistentModelIndex(source),
            position,
            kind,
            markerColor(kind, customColor),
            priorityConverted ? priority : 0});
    }
    rebuildPixelBuckets();
    q_ptr->update();
}

void ZzAnnotatedScrollBarPrivate::rebuildPixelBuckets()
{
    pixelBuckets.clear();
    const QRect groove = grooveRect();
    pixelBucketGroove = groove;
    pixelBucketOrientation = q_ptr->orientation();
    pixelBucketLayoutDirection = q_ptr->layoutDirection();
    pixelBucketInvertedAppearance = q_ptr->invertedAppearance();
    pixelBucketsCurrent = true;
    if (markers.isEmpty()) {
        return;
    }

    const int firstPixel = q_ptr->orientation() == Qt::Vertical
        ? groove.top()
        : groove.left();
    const int pixelCount = q_ptr->orientation() == Qt::Vertical
        ? groove.height()
        : groove.width();
    if (pixelCount <= 0) {
        return;
    }

    const bool reverse = q_ptr->orientation() == Qt::Horizontal
        ? (q_ptr->layoutDirection() == Qt::RightToLeft)
            != q_ptr->invertedAppearance()
        : q_ptr->invertedAppearance();
    QVector<qsizetype> winners(pixelCount, -1);
    for (qsizetype markerIndex = 0; markerIndex < markers.size(); ++markerIndex) {
        const ZzMarker &candidate = markers.at(markerIndex);
        const qreal visualPosition = reverse
            ? 1.0 - candidate.position
            : candidate.position;
        const int localPixel = qBound(
            0,
            qRound(visualPosition * static_cast<qreal>(pixelCount - 1)),
            pixelCount - 1);
        qsizetype &winner = winners[localPixel];
        if (winner < 0
            || candidate.priority > markers.at(winner).priority
            || (candidate.priority == markers.at(winner).priority
                && kindRank(candidate.kind) > kindRank(markers.at(winner).kind))) {
            winner = markerIndex;
        }
    }
    pixelBuckets.reserve(pixelCount);
    for (int localPixel = 0; localPixel < pixelCount; ++localPixel) {
        if (winners.at(localPixel) >= 0) {
            pixelBuckets.append(ZzPixelBucket{
                firstPixel + localPixel,
                winners.at(localPixel)});
        }
    }
}

void ZzAnnotatedScrollBarPrivate::ensurePixelBuckets()
{
    const QRect groove = grooveRect();
    if (!pixelBucketsCurrent || pixelBucketGroove != groove
        || pixelBucketOrientation != q_ptr->orientation()
        || pixelBucketLayoutDirection != q_ptr->layoutDirection()
        || pixelBucketInvertedAppearance != q_ptr->invertedAppearance()) {
        rebuildPixelBuckets();
    }
}

QRect ZzAnnotatedScrollBarPrivate::grooveRect() const
{
    QStyleOptionSlider option;
    q_ptr->initMarkerStyleOption(&option);
    return q_ptr->style()->subControlRect(
        QStyle::CC_ScrollBar,
        &option,
        QStyle::SC_ScrollBarGroove,
        q_ptr);
}

QColor ZzAnnotatedScrollBarPrivate::markerColor(
    ZzScrollMarkerKind kind,
    const QColor &customColor) const
{
    const auto snapshot = theme.snapshot();
    ZzColorToken token = ZzColorToken::Information;
    switch (kind) {
    case ZzScrollMarkerKind::Success:
        token = ZzColorToken::Success;
        break;
    case ZzScrollMarkerKind::Warning:
        token = ZzColorToken::Warning;
        break;
    case ZzScrollMarkerKind::Error:
        token = ZzColorToken::Error;
        break;
    case ZzScrollMarkerKind::Custom:
        if (customColor.isValid()
            && customColor.alpha() == 255
            && zzContrastRatio(
                   customColor,
                   snapshot->color(ZzColorToken::Surface))
                >= 3.0) {
            return customColor;
        }
        break;
    case ZzScrollMarkerKind::Information:
        break;
    }
    return snapshot->color(token);
}

int ZzAnnotatedScrollBarPrivate::kindRank(ZzScrollMarkerKind kind) noexcept
{
    switch (kind) {
    case ZzScrollMarkerKind::Error:
        return 4;
    case ZzScrollMarkerKind::Warning:
        return 3;
    case ZzScrollMarkerKind::Success:
        return 2;
    case ZzScrollMarkerKind::Information:
    case ZzScrollMarkerKind::Custom:
        return 1;
    }
    return 0;
}

void ZzAnnotatedScrollBarPrivate::handleModelDestroyed()
{
    model = nullptr;
    modelConnections.clear();
    markers.clear();
    pixelBuckets.clear();
    pixelBucketsCurrent = false;
    q_ptr->update();
    Q_EMIT q_ptr->markerModelChanged(nullptr);
}

} // namespace ZzFluentUI
