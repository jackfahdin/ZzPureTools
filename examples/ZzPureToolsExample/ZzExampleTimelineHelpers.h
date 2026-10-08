#pragma once

#include "ZzExampleRadialGaugeHelpers.h"
#include <QDateTime>
#include <ZzFluentUI/ZzTimeline.h>

namespace ZzExample {
using ZzFluentUI::ZzTimeline;
using ZzFluentUI::ZzTimelineEvent;
/** @brief 固定示例时刻，使页面与截图可复现。 */
inline QDateTime timelineSampleTime() { return QDateTime(QDate(2026, 10, 8), QTime(10, 30)); }
void resetTimelineEvents(ZzTimeline* timeline);
void buildTimelineEventEditor(QFormLayout* form, ZzTimeline* timeline);
QColor timelineRailColor(const ZzTimeline* timeline);
} // namespace ZzExample
