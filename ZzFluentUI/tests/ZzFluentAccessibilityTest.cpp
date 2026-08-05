#include <QtCore/QCoreApplication>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {

/** @brief 查询非空 accessible 接口并返回给当前测试。 */
QAccessibleInterface *zzAccessible(QWidget *widget)
{
    return QAccessible::queryAccessibleInterface(widget);
}

/** @brief 判断透明图像是否包含焦点环像素。 */
bool zzHasVisiblePixel(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > 0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

/** @brief 验证第一阶段 Fluent 交互控件的辅助功能与键盘契约。 */
class ZzFluentAccessibilityTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesNamesRolesAndStates()
    {
        ZzFluentUI::ZzPushButton push(QStringLiteral("Apply"));
        push.setAccessibleName(QStringLiteral("Apply changes"));
        ZzFluentUI::ZzIconButton icon;
        icon.setAccessibleName(QStringLiteral("Refresh"));
        icon.setEnabled(false);
        ZzFluentUI::ZzToggleSwitch toggle(QStringLiteral("Wi-Fi"));
        toggle.setAccessibleName(QStringLiteral("Wi-Fi"));
        toggle.setChecked(true);
        ZzFluentUI::ZzNavigationView navigation;
        navigation.setAccessibleName(QStringLiteral("Primary navigation"));

        QAccessibleInterface *pushInterface = zzAccessible(&push);
        QAccessibleInterface *iconInterface = zzAccessible(&icon);
        QAccessibleInterface *toggleInterface = zzAccessible(&toggle);
        QAccessibleInterface *navigationInterface = zzAccessible(&navigation);
        QVERIFY(pushInterface != nullptr);
        QVERIFY(iconInterface != nullptr);
        QVERIFY(toggleInterface != nullptr);
        QVERIFY(navigationInterface != nullptr);
        QCOMPARE(pushInterface->role(), QAccessible::Button);
        QCOMPARE(iconInterface->role(), QAccessible::Button);
        QCOMPARE(toggleInterface->role(), QAccessible::CheckBox);
        QCOMPARE(navigationInterface->role(), QAccessible::List);
        QCOMPARE(
            pushInterface->text(QAccessible::Name),
            QStringLiteral("Apply changes"));
        QCOMPARE(
            iconInterface->text(QAccessible::Name),
            QStringLiteral("Refresh"));
        QVERIFY(iconInterface->state().disabled);
        QVERIFY(toggleInterface->state().checked);
    }

    void followsFixedTabAndBacktabOrder()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *push = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Apply"),
            &window);
        auto *icon = new ZzFluentUI::ZzIconButton(&window);
        auto *toggle = new ZzFluentUI::ZzToggleSwitch(
            QStringLiteral("Wi-Fi"),
            &window);
        push->setAccessibleName(QStringLiteral("Apply"));
        icon->setAccessibleName(QStringLiteral("Refresh"));
        toggle->setAccessibleName(QStringLiteral("Wi-Fi"));
        layout->addWidget(push);
        layout->addWidget(icon);
        layout->addWidget(toggle);
        for (QWidget *widget : window.findChildren<QWidget *>()) {
            widget->setStyle(&style);
        }
        QWidget::setTabOrder(push, icon);
        QWidget::setTabOrder(icon, toggle);
        window.show();
        QCoreApplication::processEvents();

        push->setFocus(Qt::TabFocusReason);
        QCOMPARE(QApplication::focusWidget(), push);
        QTest::keyClick(push, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), icon);
        QTest::keyClick(icon, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), toggle);
        QTest::keyClick(toggle, Qt::Key_Backtab);
        QCOMPARE(QApplication::focusWidget(), icon);
    }

    void drawsFocusRingForEveryFocusTarget()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzPushButton push;
        ZzFluentUI::ZzIconButton icon;
        ZzFluentUI::ZzToggleSwitch toggle;
        const QList<QWidget *> targets{&push, &icon, &toggle};

        for (QWidget *target : targets) {
            QImage image(
                QSize(80, 32),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            QStyleOptionFocusRect option;
            option.rect = image.rect();
            option.state = QStyle::State_Enabled | QStyle::State_HasFocus;
            option.palette = target->palette();
            style.drawPrimitive(
                QStyle::PE_FrameFocusRect,
                &option,
                &painter,
                target);
            painter.end();
            QVERIFY2(
                zzHasVisiblePixel(image),
                target->metaObject()->className());
        }
    }

    void keyboardCommandsEmitExactlyOneIntent()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *push = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Apply"),
            &window);
        auto *navigation = new ZzFluentUI::ZzNavigationView(&window);
        auto *message = new ZzFluentUI::ZzMessageBar(&window);
        auto *model = new QStandardItemModel(navigation);
        model->appendRow(new QStandardItem(QStringLiteral("Home")));
        navigation->setModel(model);
        navigation->setCurrentIndex(model->index(0, 0));
        layout->addWidget(push);
        layout->addWidget(navigation);
        layout->addWidget(message);
        window.show();
        QCoreApplication::processEvents();
        QSignalSpy pushSpy(push, &QPushButton::clicked);
        QSignalSpy navigationSpy(
            navigation,
            &ZzFluentUI::ZzNavigationView::navigationRequested);
        QSignalSpy closeSpy(
            message,
            &ZzFluentUI::ZzMessageBar::closeRequested);

        QTest::keyClick(push, Qt::Key_Space);
        QTest::keyClick(navigation, Qt::Key_Return);
        QTest::keyClick(message, Qt::Key_Escape);

        QCOMPARE(pushSpy.count(), 1);
        QCOMPARE(navigationSpy.count(), 1);
        QCOMPARE(closeSpy.count(), 1);
    }

    void exposesNamedBreadcrumbAndTitleChrome()
    {
        ZzFluentUI::ZzBreadcrumbBar breadcrumb;
        breadcrumb.setItems({
            QStringLiteral("Home"),
            QStringLiteral("Settings")});
        const auto breadcrumbButtons =
            breadcrumb.findChildren<QToolButton *>();
        QCOMPARE(breadcrumbButtons.size(), 2);
        for (QToolButton *button : breadcrumbButtons) {
            QVERIFY(!button->accessibleName().isEmpty());
            QAccessibleInterface *interface = zzAccessible(button);
            QVERIFY(interface != nullptr);
            QCOMPARE(interface->role(), QAccessible::Button);
        }

        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setTitle(QStringLiteral("Workspace"));
        for (QWidget *widget : titleBar.interactiveWidgets()) {
            QVERIFY(!widget->accessibleName().isEmpty());
            QAccessibleInterface *interface = zzAccessible(widget);
            QVERIFY(interface != nullptr);
            QCOMPARE(interface->role(), QAccessible::Button);
        }
    }

    void exposesCalendarSemanticsAndKeyboardNavigation()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *picker = new ZzFluentUI::ZzCalendarPicker(&window);
        auto *calendar = new ZzFluentUI::ZzCalendar(&window);
        const QDate selectedDate(2026, 8, 5);
        picker->setAccessibleName(QStringLiteral("Project due date"));
        picker->setDate(selectedDate);
        calendar->setAccessibleName(QStringLiteral("Project calendar"));
        calendar->setSelectedDate(selectedDate);
        layout->addWidget(picker);
        layout->addWidget(calendar);
        window.show();
        calendar->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        QAccessibleInterface *pickerInterface = zzAccessible(picker);
        QAccessibleInterface *calendarInterface = zzAccessible(calendar);
        QVERIFY(pickerInterface != nullptr);
        QVERIFY(calendarInterface != nullptr);
        QCOMPARE(
            pickerInterface->text(QAccessible::Name),
            QStringLiteral("Project due date"));
        QCOMPARE(
            calendarInterface->text(QAccessible::Name),
            QStringLiteral("Project calendar"));
        QCOMPARE(picker->focusPolicy(), Qt::StrongFocus);
        QVERIFY(calendarInterface->childCount() > 0);

        QWidget *focusTarget = QApplication::focusWidget();
        QVERIFY(focusTarget != nullptr);
        if (focusTarget == nullptr) {
            return;
        }
        QTest::keyClick(focusTarget, Qt::Key_Right);
        QCOMPARE(calendar->selectedDate(), selectedDate.addDays(1));
    }

    void exposesCardNamesDescriptionsStatesAndKeyboardActivation()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *action = new ZzFluentUI::ZzActionCard(
            QStringLiteral("Open settings"),
            QStringLiteral("Manage application preferences"),
            &window);
        auto *image = new ZzFluentUI::ZzImageCard(
            QStringLiteral("Project Aurora"),
            QStringLiteral("Open project preview"),
            &window);
        action->setCheckable(true);
        action->setChecked(true);
        image->setEnabled(false);
        layout->addWidget(action);
        layout->addWidget(image);
        window.show();
        action->setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        QAccessibleInterface *actionInterface = zzAccessible(action);
        QAccessibleInterface *imageInterface = zzAccessible(image);
        QVERIFY(actionInterface != nullptr);
        QVERIFY(imageInterface != nullptr);
        QCOMPARE(actionInterface->role(), QAccessible::CheckBox);
        QCOMPARE(imageInterface->role(), QAccessible::Button);
        QCOMPARE(
            actionInterface->text(QAccessible::Name),
            QStringLiteral("Open settings"));
        QCOMPARE(
            actionInterface->text(QAccessible::Description),
            QStringLiteral("Manage application preferences"));
        QCOMPARE(
            imageInterface->text(QAccessible::Description),
            QStringLiteral("Open project preview"));
        QVERIFY(actionInterface->state().checked);
        QVERIFY(actionInterface->state().focused);
        QVERIFY(imageInterface->state().disabled);

        QSignalSpy clickSpy(action, &QAbstractButton::clicked);
        QTest::keyClick(action, Qt::Key_Return);
        QCOMPARE(clickSpy.count(), 1);
    }
};

QTEST_MAIN(ZzFluentAccessibilityTest)

#include "ZzFluentAccessibilityTest.moc"
