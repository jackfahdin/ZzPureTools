#pragma once

#include <QtCore/QDate>
#include <QtCore/QObject>

namespace ZzFluentUI {

class ZzCalendar;
class ZzCalendarPicker;

/** @brief 保存 QDateEdit 所有的唯一 ZzCalendar 非拥有指针。 */
class ZzCalendarPickerPrivate final : public QObject
{
public:
    /**
     * @brief 创建并向 QDateEdit 转移日历 QWidget 所有权。
     * @param q 非空、非拥有的公开日期选择器。
     */
    explicit ZzCalendarPickerPrivate(ZzCalendarPicker *q);

    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzCalendarPicker *const q_ptr;
    ZzCalendar *calendar = nullptr;
    QObject *popup = nullptr;
    QDate openDate;
    bool popupCommitByMouse = false;
    bool popupMousePress = false;
};

} // namespace ZzFluentUI
