#pragma once

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

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

private:
    void observeVisiblePopup();
    void attachPopup(QWidget *candidate);
    void clearPopupTransaction();

public:
    ZzCalendarPicker *const q_ptr;
    ZzCalendar *calendar = nullptr;
    QPointer<QWidget> popup;
    QDate openDate;
    bool popupCommitByMouse = false;
    bool popupCommitByKeyboard = false;
    bool popupMousePress = false;
};

} // namespace ZzFluentUI
