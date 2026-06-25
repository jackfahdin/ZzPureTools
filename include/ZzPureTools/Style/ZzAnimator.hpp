#ifndef ZZPURETOOLS_STYLE_ZZANIMATOR_HPP
#define ZZPURETOOLS_STYLE_ZZANIMATOR_HPP

#include <QObject>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Core/ZzToken.hpp"

class QVariantAnimation;

/**
 * @brief 控件交互动画驱动类。
 *
 * 通过属性动画驱动悬停和按下状态的过渡效果，
 * 并对外提供归一化的进度值，便于绘制时实现平滑视觉变化。
 */
class ZZ_PURE_TOOLS_EXPORT ZzAnimator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造动画驱动对象。
     * @param parent 父对象，默认为 nullptr。
     */
    explicit ZzAnimator(QObject* parent = nullptr);

    /**
     * @brief 设置悬停状态。
     * @param hovered 是否为悬停状态。
     */
    void setHovered(bool hovered);

    /**
     * @brief 设置按下状态。
     * @param pressed 是否为按下状态。
     */
    void setPressed(bool pressed);

    /// 重置悬停和按下状态。
    void reset();

    /**
     * @brief 获取悬停动画进度。
     * @return 归一化的悬停进度，范围 [0, 1]。
     */
    [[nodiscard]] qreal hoverProgress() const noexcept;

    /**
     * @brief 获取按下动画进度。
     * @return 归一化的按下进度，范围 [0, 1]。
     */
    [[nodiscard]] qreal pressProgress() const noexcept;

    /**
     * @brief 根据动画进度解析当前控件状态。
     * @param enabled 控件是否可用。
     * @return 当前解析后的控件状态。
     */
    [[nodiscard]] ZzControlState resolvedState(bool enabled) const;

    /**
     * @brief 设置动画时长。
     * @param durationMs 动画时长，单位为毫秒。
     */
    void setDuration(int durationMs);

signals:
    /// 动画进度更新时发出，通知控件重新绘制。
    void updated();

private:
    void configureAnimation(QVariantAnimation* animation, qreal& progress);

    QVariantAnimation* m_hoverAnimation = nullptr; ///< 悬停过渡动画。
    QVariantAnimation* m_pressAnimation = nullptr; ///< 按下过渡动画。
    qreal m_hoverProgress = 0.0;                   ///< 悬停进度，范围 [0, 1]。
    qreal m_pressProgress = 0.0;                   ///< 按下进度，范围 [0, 1]。
};

#endif // ZZPURETOOLS_STYLE_ZZANIMATOR_HPP
