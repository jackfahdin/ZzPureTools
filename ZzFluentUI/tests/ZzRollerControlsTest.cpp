#include <algorithm>
#include <memory>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWheelEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzRoller.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace {

/** @brief 处理普通事件和延迟销毁，使 popup 与对象计数稳定。 */
void zzFlushRollerEvents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

/** @brief 返回选择器拥有的唯一 Qt::Popup 顶层窗口。 */
QWidget *zzRollerPopup(ZzFluentUI::ZzRollerPicker *picker)
{
    const QList<QWidget *> widgets = picker->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (widget->isWindow()
            && widget->windowFlags().testFlag(Qt::Popup)) {
            return widget;
        }
    }
    return nullptr;
}

/** @brief 渲染滚轮到透明图像，供多主题和方向检查使用。 */
QImage zzRenderRoller(ZzFluentUI::ZzRoller *roller)
{
    roller->resize(roller->sizeHint());
    QImage image(
        roller->size(),
        QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    roller->render(&painter);
    painter.end();
    return image;
}

/** @brief 判断图像是否包含实际绘制像素。 */
bool zzContainsRollerPixel(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() != 0) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 构造指定数量的稳定测试文本。 */
QStringList zzRollerItems(int count)
{
    QStringList items;
    items.reserve(count);
    for (int index = 0; index < count; ++index) {
        items.append(QStringLiteral("Item %1").arg(index));
    }
    return items;
}

} // namespace

/** @brief 验证滚轮及多列选择器的索引、输入、事务和对象预算。 */
class ZzRollerControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesEmptyAndMutationBoundaries()
    {
        ZzFluentUI::ZzRoller roller;
        QCOMPARE(roller.itemCount(), 0);
        QCOMPARE(roller.currentIndex(), -1);
        QCOMPARE(roller.currentText(), QString{});
        QCOMPARE(roller.minimum(), -1);
        QCOMPARE(roller.maximum(), -1);
        QCOMPARE(roller.value(), -1);
        QCOMPARE(roller.itemHeight(), 36);
        QCOMPARE(roller.visibleItemCount(), 5);
        QCOMPARE(roller.buttonSymbols(), QAbstractSpinBox::NoButtons);
        QCOMPARE(roller.focusPolicy(), Qt::StrongFocus);
        QCOMPARE(roller.sizePolicy().horizontalPolicy(),
                 QSizePolicy::Preferred);
        QCOMPARE(roller.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
        QVERIFY(roller.minimumSizeHint().width() >= 96);
        QCOMPARE(roller.sizeHint().height(), 180);

        QLineEdit *editor = roller.findChild<QLineEdit *>();
        if (editor == nullptr) {
            QFAIL("滚轮缺少内部只读编辑器");
        }
        QVERIFY(editor->isReadOnly());
        QVERIFY(editor->isHidden());
        QCOMPARE(editor->focusPolicy(), Qt::NoFocus);

        QSignalSpy itemsSpy(&roller, &ZzFluentUI::ZzRoller::itemsChanged);
        QSignalSpy indexSpy(
            &roller,
            &ZzFluentUI::ZzRoller::currentIndexChanged);
        QSignalSpy textSpy(
            &roller,
            &ZzFluentUI::ZzRoller::currentTextChanged);
        QSignalSpy activatedSpy(
            &roller,
            &ZzFluentUI::ZzRoller::activated);

        roller.setItems({
            QStringLiteral("Zero"),
            QStringLiteral("Duplicate"),
            QStringLiteral("Duplicate"),
            QString{}});
        QCOMPARE(roller.itemCount(), 4);
        QCOMPARE(roller.currentIndex(), 0);
        QCOMPARE(roller.currentText(), QStringLiteral("Zero"));
        QCOMPARE(itemsSpy.size(), 1);
        QCOMPARE(indexSpy.size(), 1);
        QCOMPARE(textSpy.size(), 1);
        QCOMPARE(activatedSpy.size(), 0);

        roller.setItems(roller.items());
        QCOMPARE(itemsSpy.size(), 1);
        roller.setCurrentIndex(-1);
        roller.setCurrentIndex(roller.itemCount());
        QCOMPARE(roller.currentIndex(), 0);
        QVERIFY(roller.setCurrentText(QStringLiteral("Duplicate")));
        QCOMPARE(roller.currentIndex(), 1);
        QCOMPARE(activatedSpy.size(), 0);
        QVERIFY(!roller.setCurrentText(QStringLiteral("Missing")));

        QVERIFY(roller.insertItem(0, QStringLiteral("Before")));
        QCOMPARE(roller.currentIndex(), 2);
        QCOMPARE(roller.currentText(), QStringLiteral("Duplicate"));
        QVERIFY(!roller.insertItem(-1, QStringLiteral("Invalid")));
        QVERIFY(!roller.insertItem(
            roller.itemCount() + 1,
            QStringLiteral("Invalid")));
        QVERIFY(roller.removeItem(0));
        QCOMPARE(roller.currentIndex(), 1);
        QCOMPARE(roller.currentText(), QStringLiteral("Duplicate"));
        QVERIFY(roller.setItemText(1, QStringLiteral("Changed")));
        QCOMPARE(roller.currentText(), QStringLiteral("Changed"));
        QVERIFY(!roller.setItemText(1, QStringLiteral("Changed")));
        QVERIFY(!roller.removeItem(-1));
        QVERIFY(!roller.removeItem(roller.itemCount()));

        roller.setItemHeight(0);
        QCOMPARE(roller.itemHeight(), 24);
        roller.setItemHeight(200);
        QCOMPARE(roller.itemHeight(), 96);
        roller.setVisibleItemCount(0);
        QCOMPARE(roller.visibleItemCount(), 3);
        roller.setVisibleItemCount(6);
        QCOMPARE(roller.visibleItemCount(), 7);
        roller.setVisibleItemCount(100);
        QCOMPARE(roller.visibleItemCount(), 9);
        QCOMPARE(roller.sizeHint().height(), 864);

        roller.setItems(zzRollerItems(40));
        roller.setCurrentIndex(39);
        roller.setItems(zzRollerItems(3));
        QCOMPARE(roller.currentIndex(), 2);
        QCOMPARE(roller.currentText(), QStringLiteral("Item 2"));
        roller.clearItems();
        QCOMPARE(roller.itemCount(), 0);
        QCOMPARE(roller.currentIndex(), -1);
        QCOMPARE(roller.currentText(), QString{});
        roller.clearItems();
        QCOMPARE(activatedSpy.size(), 0);
    }

    void supportsDiscreteUserInputWithoutAnimation()
    {
        QWidget host;
        ZzFluentUI::ZzRoller roller(&host);
        roller.setItems(zzRollerItems(8));
        roller.setWrapping(false);
        roller.resize(roller.sizeHint());
        host.resize(roller.size());
        host.show();
        roller.setFocus();
        zzFlushRollerEvents();

        QSignalSpy activatedSpy(
            &roller,
            &ZzFluentUI::ZzRoller::activated);
        QTest::keyClick(&roller, Qt::Key_Down);
        QCOMPARE(roller.currentIndex(), 0);
        QCOMPARE(activatedSpy.size(), 0);
        QTest::keyClick(&roller, Qt::Key_Up);
        QCOMPARE(roller.currentIndex(), 1);
        QTest::keyClick(&roller, Qt::Key_PageUp);
        QCOMPARE(roller.currentIndex(), 6);
        QTest::keyClick(&roller, Qt::Key_End);
        QCOMPARE(roller.currentIndex(), 7);
        QTest::keyClick(&roller, Qt::Key_Up);
        QCOMPARE(roller.currentIndex(), 7);
        QTest::keyClick(&roller, Qt::Key_Home);
        QCOMPARE(roller.currentIndex(), 0);

        roller.setWrapping(true);
        roller.setCurrentIndex(7);
        const qsizetype beforeWrappedKey = activatedSpy.size();
        QTest::keyClick(&roller, Qt::Key_Up);
        QCOMPARE(roller.currentIndex(), 0);
        QCOMPARE(activatedSpy.size(), beforeWrappedKey + 1);

        QWheelEvent wheelUp(
            QPointF(20.0, 20.0),
            QPointF(roller.mapToGlobal(QPoint(20, 20))),
            QPoint(),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        QCoreApplication::sendEvent(&roller, &wheelUp);
        QCOMPARE(roller.currentIndex(), 1);

        QWheelEvent naturalWheel(
            QPointF(20.0, 20.0),
            QPointF(roller.mapToGlobal(QPoint(20, 20))),
            QPoint(),
            QPoint(0, 120),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            true);
        QCoreApplication::sendEvent(&roller, &naturalWheel);
        QCOMPARE(roller.currentIndex(), 0);

        QWheelEvent halfWheelOne(
            QPointF(20.0, 20.0),
            QPointF(roller.mapToGlobal(QPoint(20, 20))),
            QPoint(),
            QPoint(0, 60),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        QWheelEvent halfWheelTwo(
            QPointF(20.0, 20.0),
            QPointF(roller.mapToGlobal(QPoint(20, 20))),
            QPoint(),
            QPoint(0, 60),
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        QCoreApplication::sendEvent(&roller, &halfWheelOne);
        QCOMPARE(roller.currentIndex(), 0);
        QCoreApplication::sendEvent(&roller, &halfWheelTwo);
        QCOMPARE(roller.currentIndex(), 1);

        roller.setCurrentIndex(2);
        const QPoint belowCenter(
            roller.width() / 2,
            (roller.visibleItemCount() / 2 + 1) * roller.itemHeight()
                + roller.itemHeight() / 2);
        QTest::mouseClick(&roller, Qt::LeftButton, Qt::NoModifier,
                          belowCenter);
        QCOMPARE(roller.currentIndex(), 3);

        const qsizetype beforeDrag = activatedSpy.size();
        const QPoint center(
            roller.width() / 2,
            roller.height() / 2);
        QTest::mousePress(
            &roller,
            Qt::LeftButton,
            Qt::NoModifier,
            center);
        QTest::mouseMove(
            &roller,
            center - QPoint(0, roller.itemHeight() + 2),
            1);
        QTest::mouseRelease(
            &roller,
            Qt::LeftButton,
            Qt::NoModifier,
            center - QPoint(0, roller.itemHeight() + 2));
        QCOMPARE(roller.currentIndex(), 4);
        QCOMPARE(activatedSpy.size(), beforeDrag + 1);
        QCOMPARE(roller.findChildren<QAbstractAnimation *>().size(), 0);
        QCOMPARE(roller.findChildren<QTimer *>().size(), 0);
    }

    void rendersAcrossThemesDirectionsAndAccessibility()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzRoller roller;
        roller.setStyle(&style);
        roller.setPalette(style.standardPalette());
        roller.setAccessibleName(QStringLiteral("Minute"));
        roller.setItems({
            QStringLiteral("00"),
            QStringLiteral("15"),
            QStringLiteral("30"),
            QStringLiteral("45")});
        roller.setCurrentIndex(2);

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            roller.setPalette(style.standardPalette());
            for (const Qt::LayoutDirection direction : {
                     Qt::LeftToRight,
                     Qt::RightToLeft}) {
                roller.setLayoutDirection(direction);
                QVERIFY(zzContainsRollerPixel(zzRenderRoller(&roller)));
            }
        }

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&roller);
        if (interface == nullptr) {
            QFAIL("滚轮缺少标准QSpinBox无障碍接口");
            return;
        }
        QCOMPARE(interface->role(), QAccessible::SpinBox);
        QCOMPARE(interface->text(QAccessible::Name),
                 QStringLiteral("Minute"));
        QVERIFY(interface->state().focusable);
        QAccessibleValueInterface *valueInterface =
            interface->valueInterface();
        QVERIFY(valueInterface != nullptr);
        QCOMPARE(valueInterface->currentValue().toInt(), 2);
        QCOMPARE(valueInterface->minimumValue().toInt(), 0);
        QCOMPARE(valueInterface->maximumValue().toInt(), 3);
        roller.setEnabled(false);
        QVERIFY(interface->state().disabled);
    }

    void rendersSubtleCenterBandAndStableRows()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzRoller roller;
        roller.setStyle(&style);
        roller.setPalette(style.standardPalette());
        roller.setItems({QStringLiteral("0"), QStringLiteral("1"),
                         QStringLiteral("2"), QStringLiteral("3"),
                         QStringLiteral("4")});
        roller.setCurrentIndex(2);
        roller.setItemHeight(32);
        roller.setVisibleItemCount(5);
        roller.resize(roller.sizeHint());

        const QImage image = zzRenderRoller(&roller);
        const int centerY = image.height() / 2;
        const QColor center = image.pixelColor(image.width() / 8, centerY);
        const QColor base = roller.palette().color(QPalette::Base);
        const QColor highlight = roller.palette().color(QPalette::Highlight);
        const auto distance = [](const QColor &left, const QColor &right) {
            return std::abs(left.red() - right.red())
                + std::abs(left.green() - right.green())
                + std::abs(left.blue() - right.blue());
        };
        QVERIFY2(distance(center, base) < distance(highlight, base),
                 "center band must be a low-saturation Highlight blend");

        const QRect expectedCenter(
            0,
            2 * roller.itemHeight(),
            roller.width(),
            roller.itemHeight());
        const QRect expectedAbove(
            0,
            roller.itemHeight(),
            roller.width(),
            roller.itemHeight());
        QCOMPARE(expectedCenter.height(), roller.itemHeight());
        QCOMPARE(expectedAbove.bottom() + 1, expectedCenter.top());
        QCOMPARE(image.height(), roller.visibleItemCount() * roller.itemHeight());

        const QImage beforeHover = image;
        const QPoint hoverPosition(
            roller.width() / 2,
            expectedAbove.top() + roller.itemHeight() / 2);
        QMouseEvent hoverEvent(
            QEvent::MouseMove,
            QPointF(hoverPosition),
            QPointF(hoverPosition),
            QPointF(hoverPosition),
            Qt::NoButton,
            Qt::NoButton,
            Qt::NoModifier);
        QCoreApplication::sendEvent(&roller, &hoverEvent);
        const QImage afterHover = zzRenderRoller(&roller);
        for (int row = 0; row < roller.visibleItemCount(); ++row) {
            int differences = 0;
            const int top = row * roller.itemHeight();
            for (int y = top; y < top + roller.itemHeight(); ++y) {
                for (int x = 0; x < roller.width(); ++x) {
                    differences += beforeHover.pixelColor(x, y)
                        != afterHover.pixelColor(x, y);
                }
            }
            if (row == 1) {
                QVERIFY(differences > 0);
            } else {
                QCOMPARE(differences, 0);
            }
        }
    }

    void normalizesPickerColumnsAndBulkSignals()
    {
        ZzFluentUI::ZzRollerPicker picker;
        QSignalSpy columnsSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::columnsChanged);
        QSignalSpy selectionSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::currentSelectionChanged);
        QSignalSpy textSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::currentTextChanged);

        picker.setColumns({
            {QStringLiteral("time"),
             {QStringLiteral("00"), QStringLiteral("01")},
             1, true, 20},
            {QStringLiteral("time"),
             {QStringLiteral("AM"), QStringLiteral("AM")},
             99, false, 900},
            {{}, {}, 0, true, 96}});
        QCOMPARE(picker.columnCount(), 3);
        QCOMPARE(columnsSpy.size(), 1);
        QCOMPARE(selectionSpy.size(), 1);
        const QList<ZzFluentUI::ZzRollerColumn> normalized =
            picker.columns();
        QVERIFY(!normalized.at(0).key.isEmpty());
        QVERIFY(!normalized.at(1).key.isEmpty());
        QVERIFY(!normalized.at(2).key.isEmpty());
        QVERIFY(normalized.at(0).key != normalized.at(1).key);
        QVERIFY(normalized.at(1).key != normalized.at(2).key);
        QCOMPARE(normalized.at(0).minimumWidth, 64);
        QCOMPARE(normalized.at(1).minimumWidth, 512);
        QCOMPARE(picker.currentIndexes(), QList<int>({1, 1, -1}));
        QCOMPARE(picker.currentTexts(),
                 QStringList({QStringLiteral("01"),
                              QStringLiteral("AM"), QString{}}));
        QCOMPARE(picker.currentText(), QStringLiteral("01 / AM"));
        QCOMPARE(picker.text(), picker.currentText());
        QVERIFY(textSpy.size() >= 1);

        picker.setColumns(picker.columns());
        QCOMPARE(columnsSpy.size(), 1);
        QVERIFY(!picker.setCurrentIndex(-1, 0));
        QVERIFY(!picker.setCurrentIndex(3, 0));
        QVERIFY(!picker.setCurrentIndex(0, -1));
        QVERIFY(!picker.setCurrentIndex(0, 2));
        QVERIFY(!picker.setCurrentIndex(0, 1));
        QVERIFY(picker.setCurrentIndex(0, 0));
        QCOMPARE(picker.currentText(0), QStringLiteral("00"));

        const qsizetype beforeBulk = selectionSpy.size();
        picker.setCurrentIndexes({1, 0, 4, 8});
        QCOMPARE(picker.currentIndexes(), QList<int>({1, 0, -1}));
        QCOMPARE(selectionSpy.size(), beforeBulk + 1);
        picker.setCurrentIndexes({-1, 0});
        QCOMPARE(selectionSpy.size(), beforeBulk + 1);

        QVERIFY(!picker.setColumnItems(-1, {}));
        QVERIFY(!picker.setColumnItems(3, {}));
        QVERIFY(picker.setColumnItems(
            0,
            {QStringLiteral("Only")}));
        QCOMPARE(picker.currentIndex(0), 0);
        QCOMPARE(picker.currentText(0), QStringLiteral("Only"));
        QVERIFY(!picker.insertColumn(-1, {}));
        QVERIFY(!picker.insertColumn(5, {}));
        const QString addedKey = picker.addColumn(
            {{}, {QStringLiteral("Extra")}, 0, true, 80});
        QVERIFY(!addedKey.isEmpty());
        QCOMPARE(picker.columnCount(), 4);
        QVERIFY(picker.removeColumn(addedKey));
        QVERIFY(!picker.removeColumn(QStringLiteral("missing")));
        QVERIFY(!picker.removeColumnAt(-1));
        QCOMPARE(picker.columnCount(), 3);

        const QVariant metatype = QVariant::fromValue(normalized.at(0));
        QVERIFY(metatype.canConvert<ZzFluentUI::ZzRollerColumn>());
    }

    void commitsAndRollsBackReusablePopup()
    {
        QWidget host;
        ZzFluentUI::ZzRollerPicker picker(&host);
        picker.setAccessibleName(QStringLiteral("Appointment time"));
        picker.setColumns({
            {QStringLiteral("hour"), zzRollerItems(12), 0, true, 96},
            {QStringLiteral("minute"),
             {QStringLiteral("00"), QStringLiteral("15"),
              QStringLiteral("30"), QStringLiteral("45")},
             0, true, 96},
            {QStringLiteral("period"),
             {QStringLiteral("AM"), QStringLiteral("PM")},
             0, false, 80}});
        picker.setGeometry(20, 20, 320, 36);
        host.resize(380, 430);
        host.show();
        zzFlushRollerEvents();

        QSignalSpy visibleSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::popupVisibleChanged);
        QSignalSpy changedSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::currentSelectionChanged);
        QSignalSpy activatedSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::selectionActivated);
        QSignalSpy acceptedSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::selectionAccepted);
        QSignalSpy canceledSpy(
            &picker,
            &ZzFluentUI::ZzRollerPicker::selectionCanceled);

        picker.showPopup();
        zzFlushRollerEvents();
        QVERIFY(picker.isPopupVisible());
        QWidget *popup = zzRollerPopup(&picker);
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());
        QScreen *screen = popup->screen();
        QVERIFY(screen != nullptr);
        QVERIFY(screen->availableGeometry().contains(popup->geometry()));
        const QList<ZzFluentUI::ZzRoller *> rollers =
            picker.findChildren<ZzFluentUI::ZzRoller *>();
        QCOMPARE(rollers.size(), 3);
        QDialogButtonBox *buttons =
            picker.findChild<QDialogButtonBox *>();
        QVERIFY(buttons != nullptr);
        QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
        QPushButton *cancel = buttons->button(QDialogButtonBox::Cancel);
        QVERIFY(ok != nullptr);
        QVERIFY(cancel != nullptr);
        QVERIFY(!ok->icon().isNull());
        QVERIFY(!cancel->icon().isNull());
        QCOMPARE(visibleSpy.size(), 1);

        picker.showPopup();
        QCOMPARE(visibleSpy.size(), 1);
        QTest::keyClick(rollers.at(0), Qt::Key_Up);
        QCOMPARE(picker.currentIndex(0), 1);
        QCOMPARE(activatedSpy.size(), 1);
        QCOMPARE(changedSpy.size(), 1);
        picker.cancelPopup();
        zzFlushRollerEvents();
        QCOMPARE(picker.currentIndex(0), 0);
        QCOMPARE(canceledSpy.size(), 1);
        QCOMPARE(acceptedSpy.size(), 0);
        QVERIFY(!picker.isPopupVisible());

        picker.showPopup();
        QTest::keyClick(rollers.at(1), Qt::Key_Up);
        QCOMPARE(picker.currentIndex(1), 1);
        picker.acceptPopup();
        QCOMPARE(picker.currentIndex(1), 1);
        QCOMPARE(acceptedSpy.size(), 1);

        picker.showPopup();
        QTest::keyClick(rollers.at(2), Qt::Key_Up);
        QCOMPARE(picker.currentIndex(2), 1);
        popup->hide();
        zzFlushRollerEvents();
        QCOMPARE(picker.currentIndex(2), 0);
        QCOMPARE(canceledSpy.size(), 2);

        picker.showPopup();
        QTest::keyClick(rollers.at(0), Qt::Key_Up);
        QTest::keyClick(rollers.at(0), Qt::Key_Escape);
        zzFlushRollerEvents();
        QVERIFY(!picker.isPopupVisible());
        QCOMPARE(picker.currentIndex(0), 0);
        QCOMPARE(canceledSpy.size(), 3);

        picker.showPopup();
        QTest::keyClick(rollers.at(0), Qt::Key_Up);
        QTest::keyClick(rollers.at(0), Qt::Key_Return);
        zzFlushRollerEvents();
        QVERIFY(!picker.isPopupVisible());
        QCOMPARE(picker.currentIndex(0), 1);
        QCOMPARE(acceptedSpy.size(), 2);

        picker.showPopup();
        QTest::keyClick(rollers.at(0), Qt::Key_Up);
        picker.setColumns(picker.columns());
        QVERIFY(picker.isPopupVisible());
        QVERIFY(picker.setColumnItems(
            0,
            {QStringLiteral("A"), QStringLiteral("B")}));
        QVERIFY(!picker.isPopupVisible());
        QCOMPARE(canceledSpy.size(), 4);

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&picker);
        if (interface == nullptr) {
            QFAIL("滚轮选择器缺少标准QPushButton无障碍接口");
            return;
        }
        QCOMPARE(interface->role(), QAccessible::Button);
        QCOMPARE(interface->text(QAccessible::Name),
                 QStringLiteral("Appointment time"));
        QVERIFY(interface->state().focusable);
    }

    void keepsObjectBudgetStableAfterPopupPrewarm()
    {
        QWidget host;
        std::vector<ZzFluentUI::ZzRoller *> rollers;
        rollers.reserve(100);
        const QStringList items = zzRollerItems(20);
        for (int index = 0; index < 100; ++index) {
            auto *roller = new ZzFluentUI::ZzRoller(&host);
            roller->setItems(items);
            rollers.push_back(roller);
        }
        ZzFluentUI::ZzRollerPicker picker(&host);
        picker.setColumns({
            {QStringLiteral("one"), items, 0, true, 96},
            {QStringLiteral("two"), items, 1, false, 96},
            {QStringLiteral("three"), items, 2, true, 96}});
        picker.showPopup();
        picker.cancelPopup();
        zzFlushRollerEvents();

        const qsizetype descendants =
            host.findChildren<QObject *>().size();
        const qsizetype animations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = host.findChildren<QTimer *>().size();
        QCOMPARE(animations, 0);
        QCOMPARE(timers, 0);

        for (int round = 0; round < 1000; ++round) {
            ZzFluentUI::ZzRoller *roller = rollers.at(
                static_cast<std::size_t>(round % 100));
            roller->setCurrentIndex(round % 20);
            roller->setWrapping((round % 2) == 0);
            roller->setLayoutDirection(
                (round % 2) == 0
                    ? Qt::LeftToRight
                    : Qt::RightToLeft);
            if (round < 32) {
                picker.showPopup();
                picker.setCurrentIndexes({
                    round % 20,
                    (round + 1) % 20,
                    (round + 2) % 20});
                if ((round % 2) == 0) {
                    picker.acceptPopup();
                } else {
                    picker.cancelPopup();
                }
            }
        }
        zzFlushRollerEvents();
        QCOMPARE(host.findChildren<QObject *>().size(), descendants);
        QCOMPARE(host.findChildren<QAbstractAnimation *>().size(),
                 animations);
        QCOMPARE(host.findChildren<QTimer *>().size(), timers);

        ZzFluentUI::ZzRoller large;
        const qsizetype largeDescendants =
            large.findChildren<QObject *>().size();
        large.setItems(zzRollerItems(10000));
        QCOMPARE(large.itemCount(), 10000);
        QCOMPARE(large.findChildren<QObject *>().size(),
                 largeDescendants);
    }
};

QTEST_MAIN(ZzRollerControlsTest)

#include "ZzRollerControlsTest.moc"
