#ifndef ZZFLUENT_STYLE_ZZSTYLEDELEGATE_HPP
#define ZZFLUENT_STYLE_ZZSTYLEDELEGATE_HPP

#include <QRectF>
#include <QSize>
#include <QString>

#include "ZzFluent/Core/ZzFluentGlobal.hpp"
#include "ZzFluent/Core/ZzToken.hpp"

class QPainter;
class ZzTheme;

struct ZZ_FLUENT_EXPORT ZzStyleContext {
    const ZzTheme* theme = nullptr;
    ZzControlState state = ZzControlState::Normal;
    qreal hoverProgress = 0.0;
    qreal pressProgress = 0.0;
    bool enabled = true;
    QString text;
    bool popupVisible = false;
    bool itemSelected = false;
};

class ZZ_FLUENT_EXPORT ZzStyleDelegate
{
public:
    virtual ~ZzStyleDelegate() = default;

    virtual void paint(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const = 0;
    [[nodiscard]] virtual QSize sizeHint(const ZzStyleContext& context, const QSize& available) const;
};

#endif // ZZFLUENT_STYLE_ZZSTYLEDELEGATE_HPP
