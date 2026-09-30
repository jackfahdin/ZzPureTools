#pragma once

#include <ZzFluentUI/ZzRangeSlider.h>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtCore/QPointF>

class QLabel;

namespace ZzFluentUI {

class ZzRangeSliderPrivate final
{
public:
    explicit ZzRangeSliderPrivate(ZzRangeSlider *owner) : q(owner) {}
    ZzRangeSlider *q;
    int minimum = 0;
    int maximum = 100;
    int lower = 0;
    int upper = 100;
    int lowerPosition = 0;
    int upperPosition = 100;
    int notifiedLower = 0;
    int notifiedUpper = 100;
    int notifiedPairLower = 0;
    int notifiedPairUpper = 100;
    bool notifying = false;
    int singleStep = 1;
    int pageStep = 10;
    Qt::Orientation orientation = Qt::Horizontal;
    ZzRangeSlider::SnapMode snapMode = ZzRangeSlider::SnapMode::NoSnap;
    ZzRangeSlider::Handle active = ZzRangeSlider::Handle::LowerHandle;
    bool ticks = true;
    int tickInterval = 10;
    bool tracking = true;
    bool valueTip = false;
    bool focusRing = false;
    ZzRangeSlider::Handle hovered = ZzRangeSlider::Handle::NoHandle;
    ZzRangeSlider::Handle pressed = ZzRangeSlider::Handle::NoHandle;
    qreal pressOffset = 0.0;
    bool coincidentPress = false;
    /** @brief 即使端点切换暂时无 pressed，也记录回调取消。 */
    quint64 dragCancellation = 0;
    QPointer<QLabel> tip;
    QVariantAnimation *lowerAnimation = nullptr;
    QVariantAnimation *upperAnimation = nullptr;

    [[nodiscard]] qreal axisPosition(int value) const;
    [[nodiscard]] int valueAt(qreal position) const;
    [[nodiscard]] int snapped(int value) const;
    [[nodiscard]] QPointF center(int value) const;
    [[nodiscard]] ZzRangeSlider::Handle handleAt(const QPointF &point) const;
    [[nodiscard]] ZzRangeSlider::Handle nearestHandle(const QPointF &point) const;
    void commit(int lowerValue, int upperValue, bool user);
    void notifyValues();
    void preview(ZzRangeSlider::Handle handle, int value);
    void cancelDrag();
    void showTip();
    void hideTip();
    void animateHandle(ZzRangeSlider::Handle handle);
    [[nodiscard]] qreal innerRadius(ZzRangeSlider::Handle handle) const;
    void accessibleValueChanged(ZzRangeSlider::Handle handle, int value);
    void accessibleFocusChanged();
};

} // namespace ZzFluentUI
