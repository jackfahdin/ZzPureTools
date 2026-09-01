#include "ZzCalendarPickerPrivate.h"

#include <QtCore/QDate>
#include <QtCore/QEvent>
#include <QtCore/QLocale>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QCalendarWidget>
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
    calendar->installEventFilter(this);
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

void ZzCalendarPickerPrivate::clearPopupTransaction()
{
    if (popup) {
        popup->removeEventFilter(this);
    }
    popup = nullptr;
    openDate = {};
    popupCommitByMouse = false;
    popupCommitByKeyboard = false;
    popupMousePress = false;
}

void ZzCalendarPickerPrivate::attachPopup(QWidget *candidate)
{
    if (candidate == nullptr || candidate == popup) {
        return;
    }
    if (popup) {
        popup->removeEventFilter(this);
    }
    popup = candidate;
    popup->installEventFilter(this);
    for (QObject *child : popup->findChildren<QObject *>()) {
        child->installEventFilter(this);
    }
    QObject::connect(
        popup,
        &QObject::destroyed,
        this,
        [this]() { clearPopupTransaction(); });
}

void ZzCalendarPickerPrivate::observeVisiblePopup()
{
    QWidget *candidate = nullptr;
    if (QCalendarWidget *calendarWidget = q_ptr->calendarWidget()) {
        QWidget *window = calendarWidget->window();
        if (window != q_ptr && window->isWindow()
            && window->windowFlags().testFlag(Qt::Popup)
            && window->isVisible()) {
            candidate = window;
        }
    }
    if (candidate == nullptr) {
        for (QWidget *widget : q_ptr->findChildren<QWidget *>()) {
            if (widget->isWindow()
                && widget->windowFlags().testFlag(Qt::Popup)
                && widget->isVisible()) {
                candidate = widget;
                break;
            }
        }
    }
    if (candidate == nullptr) {
        return;
    }
    const bool changed = popup != candidate;
    attachPopup(candidate);
    if (changed || !openDate.isValid()) {
        openDate = q_ptr->date();
        popupCommitByMouse = false;
        popupCommitByKeyboard = false;
        popupMousePress = false;
    }
    candidate->setPalette(q_ptr->palette());
}

bool ZzCalendarPickerPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == q_ptr
        && (event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::KeyPress
            || event->type() == QEvent::KeyRelease
            || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::Show)) {
        observeVisiblePopup();
    } else if (watched == calendar
        && (event->type() == QEvent::Show
            || event->type() == QEvent::PaletteChange)) {
        observeVisiblePopup();
    }

    QWidget *popupWidget = popup.data();
    if (popupWidget == nullptr) {
        return QObject::eventFilter(watched, event);
    }
    auto *watchedWidget = qobject_cast<QWidget *>(watched);
    if (watched != popupWidget
        && (watchedWidget == nullptr
            || !popupWidget->isAncestorOf(watchedWidget))) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *widget = watchedWidget;
        popupMousePress = false;
        while (widget != nullptr && widget != popupWidget) {
            if (qobject_cast<QTableView *>(widget) != nullptr) {
                popupMousePress = true;
                break;
            }
            widget = widget->parentWidget();
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto *widget = watchedWidget;
        bool dateGridRelease = false;
        while (widget != nullptr && widget != popupWidget) {
            if (qobject_cast<QTableView *>(widget) != nullptr) {
                dateGridRelease = true;
                break;
            }
            widget = widget->parentWidget();
        }
        if (dateGridRelease && popupMousePress) {
            popupCommitByMouse = true;
        }
        popupMousePress = false;
    } else if (event->type() == QEvent::KeyRelease) {
        const auto *keyEvent = static_cast<const QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter) {
            popupCommitByKeyboard = true;
        }
    } else if (event->type() == QEvent::Hide && watched == popupWidget) {
        if (!popupCommitByMouse && !popupCommitByKeyboard
            && openDate.isValid() && q_ptr->date() != openDate) {
            q_ptr->setDate(openDate);
        }
        clearPopupTransaction();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzFluentUI
