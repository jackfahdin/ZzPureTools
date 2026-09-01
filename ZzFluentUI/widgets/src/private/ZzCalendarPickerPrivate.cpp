#include "ZzCalendarPickerPrivate.h"

#include <QtCore/QDate>
#include <QtCore/QEvent>
#include <QtCore/QLocale>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>

namespace ZzFluentUI {

ZzCalendarPickerPrivate::ZzCalendarPickerPrivate(ZzCalendarPicker *q)
    : QObject(q)
    , q_ptr(q)
    , calendar(new ZzCalendar(q))
{
    Q_ASSERT(q_ptr != nullptr);
    // Use the same borderless input surface as the other Fluent spin fields;
    // QDateEdit remains the owner of all date parsing and popup semantics.
    q_ptr->setFrame(false);
    if (auto *edit = q_ptr->findChild<QLineEdit *>()) {
        edit->setFrame(false);
    }
    q_ptr->setCalendarPopup(true);
    q_ptr->setCalendarWidget(calendar);
    q_ptr->setDisplayFormat(
        q_ptr->locale().dateFormat(QLocale::ShortFormat));
    q_ptr->setDate(QDate::currentDate());
    q_ptr->setAccelerated(true);
    q_ptr->setWrapping(false);
    q_ptr->setFocusPolicy(Qt::StrongFocus);
    q_ptr->installEventFilter(this);
    QObject::connect(
        calendar,
        &QCalendarWidget::selectionChanged,
        this,
        [this]() {
            if (popupMousePress) {
                popupCommitByMouse = true;
            }
        });
}

bool ZzCalendarPickerPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == q_ptr
        && event->type() == QEvent::MouseButtonRelease) {
        for (QWidget *candidate : q_ptr->findChildren<QWidget *>()) {
            if (candidate->isWindow()
                && candidate->windowFlags().testFlag(Qt::Popup)
                && candidate->isVisible()) {
                if (popup != candidate) {
                    if (popup != nullptr) {
                        popup->removeEventFilter(this);
                    }
                    popup = candidate;
                    popup->installEventFilter(this);
                    for (QObject *child : popup->findChildren<QObject *>()) {
                        child->installEventFilter(this);
                    }
                }
                openDate = q_ptr->date();
                popupCommitByMouse = false;
                break;
            }
        }
    } else if (popup != nullptr) {
        auto *popupWidget = static_cast<QWidget *>(popup);
        auto *watchedWidget = qobject_cast<QWidget *>(watched);
        if (watched != popup
            && (watchedWidget == nullptr
                || !popupWidget->isAncestorOf(watchedWidget))) {
            return QObject::eventFilter(watched, event);
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *widget = qobject_cast<QWidget *>(watched);
            bool dateGridRelease = false;
            while (widget != nullptr && widget != popupWidget) {
                if (qobject_cast<QTableView *>(widget) != nullptr) {
                    dateGridRelease = true;
                    break;
                }
                widget = widget->parentWidget();
            }
            if (dateGridRelease) {
                popupCommitByMouse = true;
            }
            popupMousePress = false;
        } else if (event->type() == QEvent::MouseButtonPress) {
            popupMousePress = true;
        } else if (event->type() == QEvent::Hide) {
            if (!popupCommitByMouse
                && openDate.isValid() && q_ptr->date() != openDate) {
                q_ptr->setDate(openDate);
            }
            popupCommitByMouse = false;
            popupMousePress = false;
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzFluentUI
