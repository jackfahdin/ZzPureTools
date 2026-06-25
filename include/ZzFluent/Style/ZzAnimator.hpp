#ifndef ZZFLUENT_STYLE_ZZANIMATOR_HPP
#define ZZFLUENT_STYLE_ZZANIMATOR_HPP

#include <QObject>

#include "ZzFluent/Core/ZzFluentGlobal.hpp"
#include "ZzFluent/Core/ZzToken.hpp"

class QVariantAnimation;

class ZZ_FLUENT_EXPORT ZzAnimator : public QObject
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

#endif // ZZFLUENT_STYLE_ZZANIMATOR_HPP
