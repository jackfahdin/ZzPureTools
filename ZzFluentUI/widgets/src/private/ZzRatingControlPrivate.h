#pragma once

#include <limits>

#include <QtCore/QRect>
#include <QtGui/QAccessible>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <ZzFluentUI/ZzRatingPrecision.h>

#include "ZzWidgetTheme.h"

class QPainter;

namespace ZzFluentUI {

class ZzRatingControl;

/** @brief 持有评分控件的数值、预览状态和两张固定图标缓存。 */
class ZzRatingControlPrivate final
{
public:
    /**
     * @brief 绑定公开控件并注册 qreal 值无障碍接口。
     * @param q 非空、非拥有的公开评分控件。
     */
    explicit ZzRatingControlPrivate(ZzRatingControl *q);

    /** @brief 从 Qt 缓存删除本实例的无障碍接口。 */
    ~ZzRatingControlPrivate();

    /** @brief 返回当前精度对应的 1.0 或 0.5 步长。 */
    [[nodiscard]] qreal stepSize() const noexcept;

    /** @brief 把有限值钳制到范围并按当前精度量化。 */
    [[nodiscard]] qreal normalized(qreal value) const noexcept;

    /** @brief 设置真实评分并通知绘制与无障碍系统。 */
    [[nodiscard]] bool setRating(qreal value);

    /** @brief 返回最大星数对应的视觉内容矩形。 */
    [[nodiscard]] QRect ratingRect() const;

    /** @brief 返回指定逻辑星级索引经 RTL 映射后的图标矩形。 */
    [[nodiscard]] QRect glyphRect(int index) const;

    /** @brief 把局部鼠标位置映射为当前精度下的评分。 */
    [[nodiscard]] qreal ratingAt(const QPointF &position) const noexcept;

    /** @brief 更新悬停预览，不修改真实评分。 */
    void updatePreview(const QPointF &position);

    /** @brief 清除悬停预览并恢复真实评分绘制。 */
    void clearPreview();

    /** @brief 绘制当前真实值或悬停预览值。 */
    void paint(QPainter *painter);

    /** @brief 清空两张实例 pixmap 句柄并重建回退主题。 */
    void refreshTheme();

    /** @brief 向辅助技术发送浮点数值变化事件。 */
    void notifyAccessibleValueChanged(qreal value) const;

    ZzRatingControl *const q_ptr;
    ZzWidgetTheme theme;
    QPixmap emptyStar;
    QPixmap filledStar;
    qreal rating = 0.0;
    qreal previewRating = 0.0;
    qreal cachedDevicePixelRatio = 0.0;
    quint64 cachedThemeRevision = std::numeric_limits<quint64>::max();
    int maximumRating = 5;
    int cachedGlyphExtent = 0;
    ZzRatingPrecision precision = ZzRatingPrecision::Whole;
    QPalette::ColorGroup cachedColorGroup = QPalette::NColorGroups;
    bool readOnly = false;
    bool previewActive = false;
    bool mousePressed = false;
#if QT_CONFIG(accessibility)
    QAccessible::Id accessibleInterfaceId = 0;
#endif

private:
    /** @brief 按当前主题、DPR 和启用状态按需刷新两张星形图标。 */
    void ensurePixmaps();
};

} // namespace ZzFluentUI
