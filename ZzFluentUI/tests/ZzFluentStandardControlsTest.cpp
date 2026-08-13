#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QtGlobal>
#include <QtCore/QAbstractAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
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

/** @brief 统计接近目标颜色的不透明像素数量。 */
int zzColorPixelCount(const QImage &image, const QColor &expected)
{
    constexpr int tolerance = 8;
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance) {
                ++count;
            }
        }
    }
    return count;
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

    /**
     * @brief 验证标准选择控件的范围、互斥和方向语义不被 Fluent 样式改变。
     */
    void preservesStandardControlRangeAndSelectionSemantics()
    {
        QWidget host;
        QCheckBox checkBox(QStringLiteral("Check"), &host);
        checkBox.setTristate(true);
        QRadioButton firstRadio(QStringLiteral("First"), &host);
        QRadioButton secondRadio(QStringLiteral("Second"), &host);
        QSlider horizontalSlider(Qt::Horizontal, &host);
        QSlider verticalSlider(Qt::Vertical, &host);
        host.show();
        QCoreApplication::processEvents();

        checkBox.setCheckState(Qt::Unchecked);
        QTest::keyClick(&checkBox, Qt::Key_Space);
        QCOMPARE(checkBox.checkState(), Qt::PartiallyChecked);
        QTest::keyClick(&checkBox, Qt::Key_Space);
        QCOMPARE(checkBox.checkState(), Qt::Checked);

        firstRadio.setChecked(true);
        secondRadio.setChecked(true);
        QVERIFY(!firstRadio.isChecked());
        QVERIFY(secondRadio.autoExclusive());

        horizontalSlider.setRange(-10, 20);
        horizontalSlider.setSingleStep(3);
        horizontalSlider.setPageStep(7);
        horizontalSlider.setTracking(false);
        horizontalSlider.setValue(5);
        QTest::keyClick(&horizontalSlider, Qt::Key_Right);
        QCOMPARE(horizontalSlider.value(), 8);
        horizontalSlider.setLayoutDirection(Qt::RightToLeft);
        QTest::keyClick(&horizontalSlider, Qt::Key_Right);
        QCOMPARE(horizontalSlider.value(), 5);

        verticalSlider.setRange(0, 100);
        verticalSlider.setValue(50);
        QTest::keyClick(&verticalSlider, Qt::Key_Up);
        QCOMPARE(verticalSlider.value(), 51);
        QVERIFY(verticalSlider.orientation() == Qt::Vertical);

        QAccessibleInterface *accessible =
            QAccessible::queryAccessibleInterface(&checkBox);
        QVERIFY(accessible != nullptr);
        QCOMPARE(accessible->role(), QAccessible::CheckBox);
    }

    /**
     * @brief 验证文本编辑和组合框保留文本、模型、编辑及弹出生命周期语义。
     */
    void preservesTextAndPopupSemantics()
    {
        QWidget host;
        QLineEdit lineEdit(&host);
        QPlainTextEdit plainTextEdit(&host);
        QComboBox comboBox(&host);
        QStandardItemModel model(0, 1, &comboBox);
        for (const QString &text : {
                 QStringLiteral("Linux"),
                 QStringLiteral("Windows"),
                 QStringLiteral("macOS")}) {
            model.appendRow(new QStandardItem(text));
        }
        comboBox.setModel(&model);
        comboBox.setEditable(true);
        comboBox.setCurrentIndex(1);
        lineEdit.setText(QStringLiteral("editable text"));
        lineEdit.selectAll();
        QCOMPARE(lineEdit.selectedText(), QStringLiteral("editable text"));
        lineEdit.copy();
        lineEdit.clear();
        lineEdit.paste();
        QCOMPARE(lineEdit.text(), QStringLiteral("editable text"));

        plainTextEdit.setPlainText(QStringLiteral("first\nsecond"));
        plainTextEdit.moveCursor(QTextCursor::End);
        plainTextEdit.insertPlainText(QStringLiteral("\nthird"));
        QVERIFY(plainTextEdit.toPlainText().endsWith(QStringLiteral("third")));
        plainTextEdit.undo();
        QVERIFY(!plainTextEdit.toPlainText().endsWith(QStringLiteral("third")));

        QCOMPARE(comboBox.currentText(), QStringLiteral("Windows"));
        comboBox.lineEdit()->setText(QStringLiteral("custom"));
        QCOMPARE(comboBox.currentText(), QStringLiteral("custom"));
        comboBox.showPopup();
        QCoreApplication::processEvents();
        QVERIFY(comboBox.view() != nullptr);
        QVERIFY(comboBox.view()->model() == &model);
        comboBox.hidePopup();
        QVERIFY(!comboBox.view()->isVisible());
    }

    /**
     * @brief 验证菜单栏、工具栏和状态栏继续使用 QAction 与临时消息协议。
     */
    void preservesToolingAndStatusSurfaces()
    {
        QWidget host;
        auto *menuBar = new QMenuBar(&host);
        menuBar->setNativeMenuBar(false);
        QMenu *fileMenu = menuBar->addMenu(QStringLiteral("File"));
        QAction *openAction = fileMenu->addAction(
            QStringLiteral("Open"),
            QKeySequence::Open);
        QAction *checkAction = fileMenu->addAction(QStringLiteral("Watch"));
        checkAction->setCheckable(true);

        QToolBar toolBar(QStringLiteral("Commands"), &host);
        QAction *toolAction = toolBar.addAction(QStringLiteral("Build"));
        toolAction->setCheckable(true);
        QSignalSpy triggeredSpy(&toolBar, &QToolBar::actionTriggered);
        toolAction->trigger();
        QVERIFY(toolAction->isChecked());
        QCOMPARE(triggeredSpy.count(), 1);

        QStatusBar statusBar(&host);
        auto *permanent = new QLabel(QStringLiteral("Local"), &statusBar);
        statusBar.addPermanentWidget(permanent);
        statusBar.showMessage(QStringLiteral("Ready"));
        QCOMPARE(statusBar.currentMessage(), QStringLiteral("Ready"));
        QVERIFY(statusBar.findChildren<QLabel *>().contains(permanent));

        QSignalSpy openSpy(openAction, &QAction::triggered);
        openAction->trigger();
        QCOMPARE(openSpy.count(), 1);
        QVERIFY(menuBar->actions().contains(fileMenu->menuAction()));
    }

    /**
     * @brief 验证列表、表格和树视图的模型、选择、委托、展开及 RTL 语义。
     */
    void preservesItemViewSemantics()
    {
        QWidget host;
        QStandardItemModel listModel(3, 1, &host);
        QStandardItemModel tableModel(2, 2, &host);
        QStandardItemModel treeModel(&host);
        for (int row = 0; row < listModel.rowCount(); ++row) {
            listModel.setData(
                listModel.index(row, 0),
                QStringLiteral("List %1").arg(row));
        }
        for (int row = 0; row < tableModel.rowCount(); ++row) {
            for (int column = 0; column < tableModel.columnCount(); ++column) {
                tableModel.setData(
                    tableModel.index(row, column),
                    QStringLiteral("Cell %1/%2").arg(row).arg(column));
            }
        }
        auto *root = new QStandardItem(QStringLiteral("Root"));
        root->appendRow(new QStandardItem(QStringLiteral("Child")));
        treeModel.appendRow(root);

        QListView listView(&host);
        QTableView tableView(&host);
        QTreeView treeView(&host);
        listView.setModel(&listModel);
        tableView.setModel(&tableModel);
        treeView.setModel(&treeModel);
        listView.setSelectionMode(QAbstractItemView::ExtendedSelection);
        tableView.setSelectionMode(QAbstractItemView::SingleSelection);
        listView.setCurrentIndex(listModel.index(1, 0));
        listView.selectionModel()->select(
            listModel.index(0, 0),
            QItemSelectionModel::Select);
        QCOMPARE(listView.currentIndex(), listModel.index(1, 0));
        QVERIFY(listView.selectionModel()->isSelected(listModel.index(0, 0)));

        tableView.setLayoutDirection(Qt::RightToLeft);
        tableView.setCurrentIndex(tableModel.index(1, 1));
        QCOMPARE(tableView.currentIndex(), tableModel.index(1, 1));
        treeView.expand(root->index());
        QVERIFY(treeView.isExpanded(root->index()));
        treeView.setCurrentIndex(root->child(0)->index());
        QCOMPARE(treeView.currentIndex().data().toString(), QStringLiteral("Child"));

        for (QAbstractItemView *view : {
                 static_cast<QAbstractItemView *>(&listView),
                 static_cast<QAbstractItemView *>(&tableView),
                 static_cast<QAbstractItemView *>(&treeView)}) {
            view->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(view));
            QVERIFY(view->itemDelegate() != nullptr);
            view->setEnabled(false);
            QVERIFY(!view->isEnabled());
            view->setEnabled(true);
        }
    }

    /**
     * @brief 验证标准控件状态切换不会在样式层累积对象、动画或定时器。
     */
    void keepsStandardSurfaceObjectCountStable()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        auto *checkBox = new QCheckBox(QStringLiteral("Check"), &host);
        auto *comboBox = new QComboBox(&host);
        comboBox->addItems({QStringLiteral("One"), QStringLiteral("Two")});
        auto *progress = new QProgressBar(&host);
        progress->setRange(0, 100);
        auto *listView = new QListView(&host);
        auto *model = new QStandardItemModel(2, 1, listView);
        model->setData(model->index(0, 0), QStringLiteral("One"));
        model->setData(model->index(1, 0), QStringLiteral("Two"));
        listView->setModel(model);

        const qsizetype descendants = host.findChildren<QObject *>().size();
        const qsizetype animations = host.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = host.findChildren<QTimer *>().size();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            checkBox->setChecked(iteration % 2 == 0);
            comboBox->setCurrentIndex(iteration % 2);
            progress->setValue(iteration % 101);
            listView->setCurrentIndex(model->index(iteration % 2, 0));
            if (iteration % 2 == 0) {
                controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
            } else {
                controller.setMode(ZzFluentUI::ZzThemeMode::Light);
            }
        }
        QCOMPARE(host.findChildren<QObject *>().size(), descendants);
        QCOMPARE(host.findChildren<QAbstractAnimation *>().size(), animations);
        QCOMPARE(host.findChildren<QTimer *>().size(), timers);
    }

    /**
     * @brief 验证标准控件在主题、焦点、禁用和选中状态下均能产生稳定绘制。
     */
    void rendersStandardBreadthStates()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        auto *checkBox = new QCheckBox(QStringLiteral("Checked"), &host);
        checkBox->setChecked(true);
        auto *plainTextEdit = new QPlainTextEdit(&host);
        plainTextEdit->setPlainText(QStringLiteral("Standard text"));
        auto *progress = new QProgressBar(&host);
        progress->setRange(0, 100);
        progress->setValue(50);
        host.resize(320, 180);

        QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
        QPainter painter;
        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            image.fill(Qt::transparent);
            painter.begin(&image);
            host.render(&painter);
            painter.end();
            QVERIFY(zzContainsOpaquePixel(image));
            checkBox->setEnabled(false);
            plainTextEdit->setEnabled(false);
            progress->setEnabled(false);
            image.fill(Qt::transparent);
            painter.begin(&image);
            host.render(&painter);
            painter.end();
            QVERIFY(zzContainsOpaquePixel(image));
            checkBox->setEnabled(true);
            plainTextEdit->setEnabled(true);
            progress->setEnabled(true);
        }
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

    void preservesDigitalDisplayProtocol()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLCDNumber display;
        display.setStyle(&style);
        display.setDigitCount(8);

        display.setDecMode();
        display.display(-42.5);
        QCOMPARE(display.mode(), QLCDNumber::Dec);
        QCOMPARE(display.value(), -42.5);

        display.setHexMode();
        display.display(255);
        QCOMPARE(display.mode(), QLCDNumber::Hex);
        QCOMPARE(display.intValue(), 255);

        display.setOctMode();
        display.display(64);
        QCOMPARE(display.mode(), QLCDNumber::Oct);
        QCOMPARE(display.intValue(), 64);

        display.setBinMode();
        display.display(5);
        QCOMPARE(display.mode(), QLCDNumber::Bin);
        QCOMPARE(display.intValue(), 5);

        for (const QLCDNumber::SegmentStyle segmentStyle : {
                 QLCDNumber::Outline,
                 QLCDNumber::Filled,
                 QLCDNumber::Flat}) {
            display.setSegmentStyle(segmentStyle);
            QCOMPARE(display.segmentStyle(), segmentStyle);
        }
        display.setSmallDecimalPoint(true);
        QVERIFY(display.smallDecimalPoint());
        display.setDecMode();
        display.display(QStringLiteral("12:34"));
        QVERIFY(!display.checkOverflow(1234));

        display.setDigitCount(3);
        QSignalSpy overflowSpy(&display, &QLCDNumber::overflow);
        display.display(12345);
        QCOMPARE(overflowSpy.count(), 1);

        display.setAccessibleName(QStringLiteral("计数值"));
        QAccessibleInterface *accessible =
            QAccessible::queryAccessibleInterface(&display);
        QVERIFY(accessible != nullptr);
        QCOMPARE(
            accessible->text(QAccessible::Name),
            QStringLiteral("计数值"));
        display.setEnabled(false);
        QVERIFY(accessible->state().disabled);
    }

    void drawsScopedDigitalDisplaySurface()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLCDNumber display;
        display.setStyle(&style);

        QStyleOptionFrame option;
        option.rect = QRect(0, 0, 160, 56);
        option.state = QStyle::State_Enabled;
        option.frameShape = QFrame::Box;
        option.palette = style.standardPalette();
        QImage image(
            option.rect.size(),
            QImage::Format_ARGB32_Premultiplied);
        QPainter painter;

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            image.fill(Qt::transparent);
            painter.begin(&image);
            style.drawControl(
                QStyle::CE_ShapedFrame,
                &option,
                &painter,
                &display);
            painter.end();
            QCOMPARE(
                image.pixelColor(option.rect.center()),
                controller.snapshot()->color(
                    ZzFluentUI::ZzColorToken::SurfaceSecondary));
            QVERIFY(zzContainsColor(
                image,
                controller.snapshot()->color(
                    ZzFluentUI::ZzColorToken::ControlStroke)));
        }

        option.state = QStyle::State_None;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(
            QStyle::CE_ShapedFrame,
            &option,
            &painter,
            &display);
        painter.end();
        QCOMPARE(
            image.pixelColor(option.rect.center()),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlFillDisabled));

        QFrame ordinaryFrame;
        ordinaryFrame.setStyle(&style);
        option.state = QStyle::State_Enabled;
        QImage actual(
            option.rect.size(),
            QImage::Format_ARGB32_Premultiplied);
        QImage expected = actual;
        actual.fill(Qt::transparent);
        expected.fill(Qt::transparent);
        painter.begin(&actual);
        style.drawControl(
            QStyle::CE_ShapedFrame,
            &option,
            &painter,
            &ordinaryFrame);
        painter.end();
        painter.begin(&expected);
        style.baseStyle()->drawControl(
            QStyle::CE_ShapedFrame,
            &option,
            &painter,
            &ordinaryFrame);
        painter.end();
        QCOMPARE(actual, expected);

        display.setEnabled(true);
        display.setDigitCount(6);
        display.display(1234);
        display.resize(option.rect.size());
        display.setFrameStyle(QFrame::Box | QFrame::Plain);
        image.fill(Qt::transparent);
        painter.begin(&image);
        display.render(&painter);
        painter.end();
        const QColor surface = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::SurfaceSecondary);
        const int framedSurfacePixels = zzColorPixelCount(image, surface);
        QVERIFY(framedSurfacePixels > 0);

        display.setFrameStyle(QFrame::NoFrame);
        image.fill(Qt::transparent);
        painter.begin(&image);
        display.render(&painter);
        painter.end();
        QVERIFY(zzColorPixelCount(image, surface)
                < framedSurfacePixels / 3);
    }

    void keepsDigitalDisplayObjectCountStable()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLCDNumber display;
        display.setStyle(&style);
        display.resize(160, 56);

        const qsizetype descendants =
            display.findChildren<QObject *>().size();
        const qsizetype animations =
            display.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = display.findChildren<QTimer *>().size();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            display.display(iteration);
            display.setEnabled(iteration % 2 == 0);
            display.setFrameStyle(
                iteration % 3 == 0
                    ? QFrame::NoFrame
                    : QFrame::Box | QFrame::Plain);
            if (iteration % 100 == 0) {
                controller.setMode(
                    controller.mode() == ZzFluentUI::ZzThemeMode::Light
                        ? ZzFluentUI::ZzThemeMode::Dark
                        : ZzFluentUI::ZzThemeMode::Light);
            }
        }
        QCOMPARE(display.style(), &style);
        QCOMPARE(display.findChildren<QObject *>().size(), descendants);
        QCOMPARE(
            display.findChildren<QAbstractAnimation *>().size(),
            animations);
        QCOMPARE(display.findChildren<QTimer *>().size(), timers);
        QCOMPARE(animations, 0);
        QCOMPARE(timers, 0);
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
