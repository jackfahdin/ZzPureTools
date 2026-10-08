#pragma once

#include <QVariantAnimation>
#include <ZzFluentUI/ZzTimeline.h>

namespace ZzFluentUI {
class ZzTimelineModel;
class ZzTimelineDelegate;
class ZzTimelineEventPrivate {
public:
    QDateTime timestamp;
    QString timeText;
    QString title;
    QString description;
    ZzTimelineEvent::Status status = ZzTimelineEvent::Normal;
    QColor color;
    QString icon;
    ZzTimelineEvent::Placement placement = ZzTimelineEvent::Automatic;
};
class ZzTimelinePrivate {
public:
    Qt::Orientation orientation = Qt::Vertical;
    ZzTimeline::LayoutMode layoutMode = ZzTimeline::ContentOnRight;
    bool reverse = false;
    bool timestampVisible = true;
    bool descriptionVisible = true;
    QString timestampFormat = QStringLiteral("yyyy-MM-dd HH:mm");
    int timestampWidth = 116;
    int nodeSize = 14;
    qreal lineWidth = 2.0;
    int itemSpacing = 18;
    int horizontalItemWidth = 240;
    int contentPadding = 12;
    QColor lineColor;
    int titleFontPixelSize = 0;
    int descriptionFontPixelSize = 0;
    int timestampFontPixelSize = 0;
    bool animationEnabled = true;
    int animationDuration = 1400;
    ZzTimelineModel* model = nullptr;
    ZzTimelineDelegate* delegate = nullptr;
    QVariantAnimation* pulseAnimation = nullptr;
    qreal pulseProgress = 0.0;
};
} // namespace ZzFluentUI
