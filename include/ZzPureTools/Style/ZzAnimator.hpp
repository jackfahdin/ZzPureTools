#ifndef ZZPURETOOLS_STYLE_ZZANIMATOR_HPP
#define ZZPURETOOLS_STYLE_ZZANIMATOR_HPP

#include <QObject>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Core/ZzToken.hpp"

class QVariantAnimation;

class ZZ_PURE_TOOLS_EXPORT ZzAnimator : public QObject
{
    Q_OBJECT

public:
    explicit ZzAnimator(QObject* parent = nullptr);

    void setHovered(bool hovered);
    void setPressed(bool pressed);
    void reset();

    [[nodiscard]] qreal hoverProgress() const noexcept;
    [[nodiscard]] qreal pressProgress() const noexcept;
    [[nodiscard]] ZzControlState resolvedState(bool enabled) const;

    void setDuration(int durationMs);

signals:
    void updated();

private:
    void configureAnimation(QVariantAnimation* animation, qreal& progress);

    QVariantAnimation* m_hoverAnimation = nullptr;
    QVariantAnimation* m_pressAnimation = nullptr;
    qreal m_hoverProgress = 0.0;
    qreal m_pressProgress = 0.0;
};

#endif // ZZPURETOOLS_STYLE_ZZANIMATOR_HPP
