#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QRect>
#include <QtCore/QVector>
#include <QtGui/QColor>

#include "ZzWidgetTheme.h"

class QAbstractItemModel;

namespace ZzFluentUI {

class ZzAnnotatedScrollBar;
enum class ZzScrollMarkerKind : int;

/** @brief 保存经模型规范化后的单个可绘制标记。 */
struct ZzMarker final
{
    QPersistentModelIndex source;
    qreal position = 0.0;
    ZzScrollMarkerKind kind;
    QColor color;
    int priority = 0;
};

/** @brief 保存一个轨道像素及其唯一胜出的标记缓存下标。 */
struct ZzPixelBucket final
{
    int pixel = 0;
    qsizetype markerIndex = -1;
};

/** @brief 管理非拥有模型连接、规范化标记和有界像素桶。 */
class ZzAnnotatedScrollBarPrivate final
{
public:
    /** @brief 绑定公开滚动条和主题回退缓存。 */
    explicit ZzAnnotatedScrollBarPrivate(ZzAnnotatedScrollBar *q);

    /** @brief 断开模型连接，避免销毁期间访问外部模型。 */
    ~ZzAnnotatedScrollBarPrivate();

    /** @brief 切换非拥有模型并重建轻量标记缓存。 */
    void setModel(QAbstractItemModel *newModel);

    /** @brief 从模型角色 O(n) 重建已验证的标记缓存。 */
    void rebuildMarkerCache();

    /** @brief 从标记缓存构造数量不超过可用像素数的绘制桶。 */
    void rebuildPixelBuckets();

    /** @brief 在未通知的继承属性变化后按缓存键同步像素桶。 */
    void ensurePixelBuckets();

    /** @brief 返回当前滚动条实际绘制轨道。 */
    [[nodiscard]] QRect grooveRect() const;

    /** @brief 返回颜色的相对对比度；Custom 低于阈值时回退语义色。 */
    [[nodiscard]] QColor markerColor(
        ZzScrollMarkerKind kind,
        const QColor &customColor) const;

    /** @brief 返回 Kind 的稳定碰撞优先级。 */
    [[nodiscard]] static int kindRank(ZzScrollMarkerKind kind) noexcept;

    /** @brief 模型销毁时清空缓存并只通知一次 nullptr。 */
    void handleModelDestroyed();

    ZzAnnotatedScrollBar *const q_ptr;
    ZzWidgetTheme theme;
    QPointer<QAbstractItemModel> model;
    QVector<QMetaObject::Connection> modelConnections;
    QVector<ZzMarker> markers;
    QVector<ZzPixelBucket> pixelBuckets;
    QRect pixelBucketGroove;
    Qt::Orientation pixelBucketOrientation = Qt::Vertical;
    Qt::LayoutDirection pixelBucketLayoutDirection = Qt::LeftToRight;
    bool pixelBucketInvertedAppearance = false;
    bool pixelBucketsCurrent = false;
    bool interactive = false;
};

} // namespace ZzFluentUI
