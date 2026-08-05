#include <QtCore/QCoreApplication>
#include <QtCore/QtGlobal>
#include <QtGui/QAction>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 判断图像是否包含接近目标值的不透明像素。 */
bool zzContainsColor(const QImage &image, const QColor &expected)
{
    constexpr int tolerance = 8;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 判断图像是否包含任何不透明绘制结果。 */
bool zzContainsOpaquePixel(const QImage &image)
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

/** @brief 判断矩形为空或完全位于给定边界内。 */
bool zzContainedOrEmpty(const QRect &bounds, const QRect &candidate)
{
    return candidate.isEmpty() || bounds.contains(candidate);
}

} // namespace

/**
 * @brief 验证标准 Qt Widgets 的 Fluent 绘制和原生交互语义。
 */
class ZzFluentStandardControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableLogicalMetrics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        QCOMPARE(style.pixelMetric(QStyle::PM_IndicatorWidth), 18);
        QCOMPARE(style.pixelMetric(QStyle::PM_IndicatorHeight), 18);
        QCOMPARE(style.pixelMetric(QStyle::PM_SliderLength), 20);
        QCOMPARE(style.pixelMetric(QStyle::PM_TabBarTabHSpace), 24);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuPanelWidth), 1);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuHMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuVMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarItemSpacing), 2);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolTipLabelFrameWidth), 8);
        QCOMPARE(style.styleHint(QStyle::SH_Menu_SubMenuPopupDelay), 200);
    }

    void preservesKeyboardSemantics()
    {
        QWidget host;
        QCheckBox checkBox(QStringLiteral("Check"), &host);
        QRadioButton radioButton(QStringLiteral("Radio"), &host);
        QSlider slider(Qt::Horizontal, &host);
        QLineEdit lineEdit(&host);
        QComboBox comboBox(&host);
        comboBox.addItems({QStringLiteral("A"), QStringLiteral("B")});
        host.show();
        QCoreApplication::processEvents();

        QTest::keyClick(&checkBox, Qt::Key_Space);
        QVERIFY(checkBox.isChecked());
        QTest::keyClick(&radioButton, Qt::Key_Space);
        QVERIFY(radioButton.isChecked());

        slider.setRange(0, 10);
        slider.setValue(5);
        QTest::keyClick(&slider, Qt::Key_Right);
        QCOMPARE(slider.value(), 6);

        QTest::keyClicks(&lineEdit, QStringLiteral("text"));
        QCOMPARE(lineEdit.text(), QStringLiteral("text"));

        comboBox.setCurrentIndex(0);
        QTest::keyClick(&comboBox, Qt::Key_Down);
        QCOMPARE(comboBox.currentIndex(), 1);
    }

    void providesStableComboBoxGeometry()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        for (const Qt::LayoutDirection direction : {
                 Qt::LeftToRight,
                 Qt::RightToLeft}) {
            for (const QSize size : {QSize(120, 36), QSize(18, 9)}) {
                QStyleOptionComboBox option;
                option.rect = QRect(QPoint(0, 0), size);
                option.direction = direction;
                option.state = QStyle::State_Enabled;
                option.subControls = QStyle::SC_All;

                const QRect frame = style.subControlRect(
                    QStyle::CC_ComboBox,
                    &option,
                    QStyle::SC_ComboBoxFrame);
                const QRect edit = style.subControlRect(
                    QStyle::CC_ComboBox,
                    &option,
                    QStyle::SC_ComboBoxEditField);
                const QRect arrow = style.subControlRect(
                    QStyle::CC_ComboBox,
                    &option,
                    QStyle::SC_ComboBoxArrow);

                QCOMPARE(frame, option.rect);
                QVERIFY(zzContainedOrEmpty(option.rect, edit));
                QVERIFY(zzContainedOrEmpty(option.rect, arrow));
                QVERIFY(!edit.intersects(arrow));
                QVERIFY(!arrow.isEmpty());
                if (arrow.width() < option.rect.width()) {
                    if (direction == Qt::LeftToRight) {
                        QVERIFY(arrow.center().x() > option.rect.center().x());
                    } else {
                        QVERIFY(arrow.center().x() < option.rect.center().x());
                    }
                }
                QCOMPARE(
                    style.hitTestComplexControl(
                        QStyle::CC_ComboBox,
                        &option,
                        arrow.center()),
                    QStyle::SC_ComboBoxArrow);
                if (!edit.isEmpty()) {
                    QCOMPARE(
                        style.hitTestComplexControl(
                            QStyle::CC_ComboBox,
                            &option,
                            edit.center()),
                        QStyle::SC_ComboBoxEditField);
                }
            }
        }

        QStyleOptionComboBox option;
        const QSize contents(220, 48);
        const QSize base = style.baseStyle()->sizeFromContents(
            QStyle::CT_ComboBox,
            &option,
            contents);
        const QSize fluent = style.sizeFromContents(
            QStyle::CT_ComboBox,
            &option,
            contents);
        QVERIFY(fluent.width() >= 96);
        QVERIFY(fluent.height() >= 32);
        QVERIFY(fluent.width() >= base.width());
        QVERIFY(fluent.height() >= base.height());
    }

    void scopesPopupItemStylingToComboBoxes()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QComboBox comboBox;
        comboBox.setStyle(&style);
        comboBox.addItems({QStringLiteral("One"), QStringLiteral("Two")});
        QAbstractItemView *popupView = comboBox.view();
        QVERIFY(popupView != nullptr);

        QStyleOptionViewItem item;
        item.rect = QRect(0, 0, 160, 32);
        item.state = QStyle::State_Enabled | QStyle::State_Selected;
        item.palette = style.standardPalette();
        const QSize popupItem = style.sizeFromContents(
            QStyle::CT_ItemViewItem,
            &item,
            QSize(80, 8),
            popupView);
        QVERIFY(popupItem.height() >= 32);

        QImage image(item.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawControl(
            QStyle::CE_ItemViewItem,
            &item,
            &painter,
            popupView);
        painter.end();
        QVERIFY(zzContainsColor(
            image,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::Accent)));

        QStyleOptionMenuItem menuItem;
        menuItem.rect = item.rect;
        menuItem.state = QStyle::State_Enabled | QStyle::State_Selected;
        menuItem.palette = style.standardPalette();
        menuItem.text = QStringLiteral("Selected item");
        menuItem.checkType = QStyleOptionMenuItem::Exclusive;
        menuItem.checked = true;
        const QSize popupMenuItem = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &menuItem,
            QSize(80, 8),
            popupView);
        QVERIFY(popupMenuItem.height() >= 32);
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(
            QStyle::CE_MenuItem,
            &menuItem,
            &painter,
            popupView);
        painter.end();
        QVERIFY(zzContainsColor(
            image,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::Accent)));

        QListView ordinaryView;
        const QSize ordinaryBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_ItemViewItem,
            &item,
            QSize(80, 8),
            &ordinaryView);
        QCOMPARE(
            style.sizeFromContents(
                QStyle::CT_ItemViewItem,
                &item,
                QSize(80, 8),
                &ordinaryView),
            ordinaryBase);
        QMenu ordinaryMenu;
        const QSize ordinaryMenuBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &menuItem,
            QSize(80, 8),
            &ordinaryMenu);
        const QSize ordinaryMenuFluent = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &menuItem,
            QSize(80, 8),
            &ordinaryMenu);
        QVERIFY(ordinaryMenuFluent.width() >= ordinaryMenuBase.width());
        QVERIFY(ordinaryMenuFluent.height() >= ordinaryMenuBase.height());
        QVERIFY(ordinaryMenuFluent.height() >= 32);
    }

    void drawsProgressAndPopupControls()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        QProgressBar progress;
        progress.setStyle(&style);
        progress.setRange(0, 100);
        progress.setValue(50);
        progress.resize(200, 24);
        QImage image(
            progress.size(),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        progress.render(&painter);
        painter.end();
        QVERIFY(zzContainsOpaquePixel(image));

        QMenu menu;
        menu.setStyle(&style);
        QAction *action = menu.addAction(QStringLiteral("Open"));
        QVERIFY(action != nullptr);

        QDialog dialog;
        dialog.setStyle(&style);
        QTabBar tabs(&dialog);
        tabs.addTab(QStringLiteral("One"));
        tabs.addTab(QStringLiteral("Two"));
        QCOMPARE(tabs.count(), 2);
        QCOMPARE(menu.style(), &style);
        QCOMPARE(dialog.style(), &style);
    }

    void drawsEveryPromisedFluentSurface()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QPalette palette;
        palette.setColor(QPalette::Base, QColor(Qt::blue));
        palette.setColor(QPalette::Button, QColor(Qt::blue));
        palette.setColor(QPalette::Mid, QColor(Qt::red));
        palette.setColor(QPalette::Text, QColor(Qt::red));
        palette.setColor(QPalette::ButtonText, QColor(Qt::white));
        palette.setColor(QPalette::Highlight, QColor(Qt::green));
        palette.setColor(QPalette::HighlightedText, QColor(Qt::white));

        QImage image(QSize(120, 36), QImage::Format_ARGB32_Premultiplied);
        QPainter painter;

        QStyleOptionButton button;
        button.rect = QRect(0, 0, 80, 32);
        button.state = QStyle::State_Enabled | QStyle::State_MouseOver;
        button.palette = palette;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(QStyle::CE_PushButton, &button, &painter);
        painter.end();
        QCOMPARE(
            image.pixelColor(40, 16),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlFillHover));

        QStyleOptionFrame input;
        input.rect = QRect(0, 0, 80, 32);
        input.state = QStyle::State_Enabled | QStyle::State_HasFocus;
        input.palette = palette;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawPrimitive(
            QStyle::PE_PanelLineEdit,
            &input,
            &painter);
        painter.end();
        QCOMPARE(image.pixelColor(40, 16), QColor(Qt::blue));
        QVERIFY(zzContainsColor(image, QColor(Qt::green)));

        QStyleOptionComboBox combo;
        combo.rect = QRect(0, 0, 120, 32);
        combo.state = QStyle::State_Enabled;
        combo.direction = Qt::RightToLeft;
        combo.palette = palette;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawComplexControl(
            QStyle::CC_ComboBox,
            &combo,
            &painter);
        painter.end();
        const QRect arrowRect = style.subControlRect(
            QStyle::CC_ComboBox,
            &combo,
            QStyle::SC_ComboBoxArrow);
        QVERIFY(arrowRect.center().x() < combo.rect.center().x());
        QVERIFY(zzContainsColor(image, QColor(Qt::red)));

        QStyleOptionTab tab;
        tab.rect = QRect(0, 0, 80, 32);
        tab.state = QStyle::State_Enabled | QStyle::State_Selected;
        tab.palette = palette;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(QStyle::CE_TabBarTab, &tab, &painter);
        painter.end();
        QCOMPARE(image.pixelColor(40, 16), QColor(Qt::blue));
        QCOMPARE(image.pixelColor(40, 30), QColor(Qt::green));

        QStyleOptionProgressBar busy;
        busy.rect = QRect(0, 0, 120, 16);
        busy.minimum = 0;
        busy.maximum = 0;
        busy.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        busy.palette = palette;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(QStyle::CE_ProgressBar, &busy, &painter);
        painter.end();
        QCOMPARE(image.pixelColor(60, 8), QColor(Qt::green));
        QCOMPARE(image.pixelColor(8, 8), QColor(Qt::red));

        QStyleOption toolTip;
        toolTip.rect = QRect(0, 0, 80, 32);
        toolTip.state = QStyle::State_Enabled;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawPrimitive(
            QStyle::PE_PanelTipLabel,
            &toolTip,
            &painter);
        painter.end();
        QCOMPARE(
            image.pixelColor(40, 16),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::SurfaceSecondary));

        button.state = QStyle::State_None;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(QStyle::CE_PushButton, &button, &painter);
        painter.end();
        QCOMPARE(
            image.pixelColor(40, 16),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlFillDisabled));

        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        auto *lineEdit = new QLineEdit(&host);
        auto *textEdit = new QTextEdit(&host);
        auto *comboBox = new QComboBox(&host);
        auto *tabs = new QTabBar(&host);
        auto *pushButton = new QPushButton(QStringLiteral("Apply"), &host);
        auto *progress = new QProgressBar(&host);
        comboBox->addItem(QStringLiteral("One"));
        tabs->addTab(QStringLiteral("One"));
        progress->setRange(0, 0);
        layout->addWidget(lineEdit);
        layout->addWidget(textEdit);
        layout->addWidget(comboBox);
        layout->addWidget(tabs);
        layout->addWidget(pushButton);
        layout->addWidget(progress);
        host.setStyle(&style);
        lineEdit->setStyle(&style);
        textEdit->setStyle(&style);
        comboBox->setStyle(&style);
        tabs->setStyle(&style);
        pushButton->setStyle(&style);
        progress->setStyle(&style);
        host.resize(320, 360);
        image = QImage(host.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        painter.begin(&image);
        host.render(&painter);
        painter.end();
        QCOMPARE(lineEdit->style(), &style);
        QCOMPARE(textEdit->style(), &style);
        QCOMPARE(comboBox->style(), &style);
        QCOMPARE(tabs->style(), &style);
        QCOMPARE(pushButton->style(), &style);
        QCOMPARE(progress->style(), &style);
        QVERIFY(zzContainsOpaquePixel(image));
        QToolTip::showText(QPoint(0, 0), QStringLiteral("Tip"), &host);
        QToolTip::hideText();
    }

    void respectsProgressDirectionAndPartialCheckState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QPalette palette;
        palette.setColor(QPalette::Mid, QColor(Qt::red));
        palette.setColor(QPalette::Highlight, QColor(Qt::green));
        palette.setColor(QPalette::Text, QColor(Qt::red));
        palette.setColor(QPalette::HighlightedText, QColor(Qt::white));
        palette.setColor(QPalette::Base, QColor(Qt::blue));

        QStyleOptionProgressBar progress;
        progress.rect = QRect(0, 0, 100, 12);
        progress.minimum = 0;
        progress.maximum = 100;
        progress.progress = 25;
        progress.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        progress.direction = Qt::RightToLeft;
        progress.palette = palette;
        QImage image(
            progress.rect.size(),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawControl(QStyle::CE_ProgressBar, &progress, &painter);
        painter.end();
        QCOMPARE(image.pixelColor(90, 6), QColor(Qt::green));
        QCOMPARE(image.pixelColor(10, 6), QColor(Qt::red));

        QStyleOption check;
        check.rect = QRect(0, 0, 18, 18);
        check.state = QStyle::State_Enabled | QStyle::State_NoChange;
        check.palette = palette;
        image = QImage(
            check.rect.size(),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawPrimitive(
            QStyle::PE_IndicatorCheckBox,
            &check,
            &painter);
        painter.end();
        QCOMPARE(image.pixelColor(9, 4), QColor(Qt::green));
        QVERIFY(zzContainsColor(image, QColor(Qt::white)));
    }
};

QTEST_MAIN(ZzFluentStandardControlsTest)

#include "ZzFluentStandardControlsTest.moc"
