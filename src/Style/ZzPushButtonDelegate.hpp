#ifndef ZZPUSHBUTTONDELEGATE_HPP
#define ZZPUSHBUTTONDELEGATE_HPP

#include "ZzPureTools/Style/ZzStyleDelegate.hpp"
#include "ZzPureTools/Widgets/ZzPushButton.hpp"

class ZzPushButtonDelegate : public ZzStyleDelegate
{
public:
    explicit ZzPushButtonDelegate(ZzPushButton::ZzButtonStyle style);

    void setButtonStyle(ZzPushButton::ZzButtonStyle style);
    void paint(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const override;

private:
    void paintStandard(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const;
    void paintAccent(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const;

    ZzPushButton::ZzButtonStyle m_buttonStyle;
};

#endif // ZZPUSHBUTTONDELEGATE_HPP
