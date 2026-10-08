#pragma once

#include <QtCore/qglobal.h>
#include <QtGui/QFont>
#include <QtGui/QColor>
#include <QtCore/QPointer>
#include <QtCore/QMetaObject>
#include <QtCore/QString>

class QVariantAnimation;
class QPainter;
class QWidget;

namespace ZzFluentUI {

class ZzProgressRing;

/** @brief 持有环形进度的单一持久动画和纯呈现状态。 */
class ZzProgressRingPrivate final
{
public:
    /** @brief 绑定公开控件并创建一次线性循环动画。 */
    explicit ZzProgressRingPrivate(ZzProgressRing *q);

    /** @brief 停止动画并断开所有捕获私有状态的回调。 */
    ~ZzProgressRingPrivate();

    /** @brief 根据可见性、启用状态、范围与 style 偏好同步动画。 */
    void syncAnimation();

    /** @brief 停止动画并把相位复位到确定的静态起点。 */
    void stopAnimation() noexcept;

    /** @brief 返回 minimum/maximum 是否表达 Qt 不确定进度。 */
    [[nodiscard]] bool isIndeterminate() const noexcept;

    /** @brief 以当前相位重定向同一动画的周期；参数已由公开接口收敛。 */
    void setIndeterminateDuration(int milliseconds);

    /** @brief 返回显式字体或按圆环短边自动缩放的数值字体。 */
    [[nodiscard]] QFont valueFont() const;
    [[nodiscard]] QFont titleFont() const;
    void drawText(QPainter &painter, const QRectF &contentRect) const;

    ZzProgressRing *const q_ptr;
    QVariantAnimation *const animation;
    qreal phase = 0.0;
    qreal thickness = 6.0;
    int notifiedRingWidth = 6;
    int indeterminateDuration = 800;
    QString title;
    QFont customTitleFont;
    QFont customValueFont;
    QColor ringColor;
    QColor trackColor;
    QColor titleColor;
    QColor valueColor;
    int textSpacing = 4;
    QPointer<QWidget> centerWidget;
    QMetaObject::Connection centerDestroyedConnection;
    quint64 centerRevision = 0;
};

} // namespace ZzFluentUI
