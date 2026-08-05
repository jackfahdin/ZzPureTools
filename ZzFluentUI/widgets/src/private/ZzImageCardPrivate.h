#pragma once

#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/Qt>
#include <QtGui/QPixmap>

class QPainter;
class QStyleOptionButton;

namespace ZzFluentUI {

class ZzImageCard;

/** @brief 保存一次图片卡布局的图片区和文字区矩形。 */
struct ZzImageCardLayout final
{
    QRect imageRect;
    QRect titleRect;
    QRect descriptionRect;
};

/** @brief 持有图片卡隐式共享值并执行无缩放副本绘制。 */
class ZzImageCardPrivate final
{
public:
    /** @brief 绑定公开对象，不取得 QObject 所有权。 */
    explicit ZzImageCardPrivate(ZzImageCard *q) noexcept;

    /** @brief 初始化只绘制 surface 的按钮 style option。 */
    void initStyleOption(QStyleOptionButton *option) const;

    /** @brief 计算稳定 16:9 图片区和文字区。 */
    [[nodiscard]] ZzImageCardLayout layout() const;

    /** @brief 根据适配策略返回 pixmap 源矩形。 */
    [[nodiscard]] QRectF sourceRectFor(const QRectF &target) const;

    /** @brief 根据适配策略返回图片目标矩形。 */
    [[nodiscard]] QRectF targetRectFor(const QRectF &target) const;

    /** @brief 绘制 surface、图片或占位以及文字。 */
    void paint(QPainter *painter) const;

    ZzImageCard *const q_ptr;
    QPixmap pixmap;
    QString description;
    Qt::AspectRatioMode aspectRatioMode =
        Qt::KeepAspectRatioByExpanding;
};

} // namespace ZzFluentUI
