#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QtGlobal>
#include <QtCore/QAbstractAnimation>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QFont>
#include <QtGui/QFocusEvent>
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

#include <limits>

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

/** @brief 返回接近目标颜色的不透明像素包围盒。 */
[[nodiscard]] QRect zzColorBounds(
    const QImage &image,
    const QColor &expected)
{
    QRect bounds;
    constexpr int tolerance = 8;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            const bool matches = actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance;
            if (matches) {
                bounds = bounds.united(QRect(x, y, 1, 1));
            }
        }
    }
    return bounds;
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

    /** @brief 单选框提供可见交互反馈，禁用后不会随 hover/press 改变。 */
    void radioIndicatorRespondsOnlyWhenEnabled()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionButton option;
        option.rect = QRect(0, 0, 18, 18);
        option.palette = style.standardPalette();
        const auto render = [&style, &option](QStyle::State state) {
            option.state = state;
            QImage image(18, 18, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawPrimitive(QStyle::PE_IndicatorRadioButton, &option, &painter);
            painter.end();
            return image;
        };
        for (const auto selected : {QStyle::State_Off, QStyle::State_On}) {
            const auto normal = render(QStyle::State_Enabled | selected);
            const auto hovered = render(QStyle::State_Enabled | selected | QStyle::State_MouseOver);
            const auto pressed = render(QStyle::State_Enabled | selected | QStyle::State_Sunken);
            QVERIFY(normal != hovered);
            QVERIFY(hovered != pressed);
            const auto disabled = render(selected);
            QCOMPARE(disabled, render(selected | QStyle::State_MouseOver | QStyle::State_Sunken));
        }
    }

    /** @brief 复选和半选都有状态反馈，禁用后不保留鲜艳选中色。 */
    void checkIndicatorRespondsToInteractionAndDisable()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionButton option;
        option.rect = QRect(0, 0, 18, 18);
        option.palette = style.standardPalette();
        const auto render = [&](QStyle::State state) {
            option.state = state;
            QImage image(18, 18, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, &painter);
            return image;
        };
        for (const auto selected : {QStyle::State_Off, QStyle::State_On, QStyle::State_NoChange}) {
            const auto normal = render(QStyle::State_Enabled | selected);
            QVERIFY(normal != render(QStyle::State_Enabled | selected | QStyle::State_MouseOver));
            QVERIFY(normal != render(QStyle::State_Enabled | selected | QStyle::State_Sunken));
            const auto disabled = render(selected);
            QCOMPARE(disabled, render(selected | QStyle::State_MouseOver | QStyle::State_Sunken));
            QVERIFY(!zzContainsColor(disabled, option.palette.color(QPalette::Active, QPalette::Highlight)));
        }
    }

    /** @brief 默认尺寸的滑块必须容纳完整手柄，而不是被轨道厚度裁切。 */
    void sliderSizeHintContainsHandle()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
            QSlider slider(orientation);
            slider.setStyle(&style);
            slider.resize(slider.sizeHint());
            QStyleOptionSlider option;
            option.initFrom(&slider);
            option.orientation = orientation;
            option.minimum = 0;
            option.maximum = 100;
            option.sliderPosition = 50;
            const auto handle = style.subControlRect(QStyle::CC_Slider, &option,
                QStyle::SC_SliderHandle, &slider);
            QVERIFY(slider.rect().contains(handle));
        }
    }

    /** @brief 键盘焦点轮廓在水平/垂直滑块两端都不越出控件。 */
    void sliderFocusRemainsInsideAtBothEnds()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
            QSlider slider(orientation);
            slider.setStyle(&style);
            slider.ensurePolished();
            slider.resize(slider.sizeHint());
            QFocusEvent focus(QEvent::FocusIn, Qt::TabFocusReason);
            QCoreApplication::sendEvent(&slider, &focus);
            QVERIFY(style.isFocusVisualVisible(&slider));
            QStyleOptionSlider option;
            option.initFrom(&slider);
            option.rect = QRect(QPoint(8, 8), slider.size());
            option.orientation = orientation;
            option.minimum = 0;
            option.maximum = 100;
            option.state = QStyle::State_Enabled | QStyle::State_HasFocus;
            option.palette = style.standardPalette();
            for (const int position : {0, 100}) {
                option.sliderPosition = position;
                QImage image(slider.size() + QSize(16, 16), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                style.drawComplexControl(QStyle::CC_Slider, &option, &painter, &slider);
                painter.end();
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        if (!option.rect.contains(x, y)) {
                            QCOMPARE(image.pixelColor(x, y).alpha(), 0);
                        }
                    }
                }
            }
        }
    }

    /** @brief 输入焦点强调集中在底线，只读输入不显示可编辑提示。 */
    void inputFocusUsesUnderlineAndRespectsReadOnly()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLineEdit edit;
        edit.setStyle(&style);
        QStyleOptionFrame option;
        option.rect = QRect(0, 0, 120, 32);
        option.palette = style.standardPalette();
        option.palette.setColor(QPalette::Accent, Qt::magenta);
        option.palette.setColor(QPalette::Highlight, Qt::magenta);
        option.state = QStyle::State_Enabled | QStyle::State_HasFocus;
        const auto render = [&] {
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter, &edit);
            return image;
        };
        const auto focus = render();
        const auto accentBounds = zzColorBounds(focus, QColor(Qt::magenta));
        QVERIFY(!accentBounds.isEmpty());
        QVERIFY(accentBounds.top() >= 29);
        edit.setReadOnly(true);
        QVERIFY(!zzContainsColor(render(), QColor(Qt::magenta)));
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

        host.resize(320, 180);
        host.show();
        QCoreApplication::processEvents();

        const auto renderProgress = [progress] {
            QImage image(
                progress->size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            progress->render(&painter);
        };

        const qsizetype descendants = host.findChildren<QObject *>().size();
        const qsizetype animations = host.findChildren<QAbstractAnimation *>().size();
        const qsizetype timers = host.findChildren<QTimer *>().size();
        qsizetype styleDescendants = -1;
        for (int iteration = 0; iteration < 1000; ++iteration) {
            checkBox->setChecked(iteration % 2 == 0);
            comboBox->setCurrentIndex(iteration % 2);
            progress->setRange(0, iteration % 2 == 0 ? 0 : 100);
            progress->setValue(iteration % 101);
            progress->setEnabled(iteration % 5 != 0);
            progress->setVisible(iteration % 7 != 0);
            controller.setReducedMotion(iteration % 11 == 0);
            listView->setCurrentIndex(model->index(iteration % 2, 0));
            if (iteration % 2 == 0) {
                controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
            } else {
                controller.setMode(ZzFluentUI::ZzThemeMode::Light);
            }
            renderProgress();
            QCoreApplication::processEvents();
            QVERIFY(style.findChildren<QVariantAnimation *>().size() <= 1);
            QVERIFY(style.findChildren<QTimer *>().isEmpty());
            if (styleDescendants < 0
                && !style.findChildren<QVariantAnimation *>().isEmpty()) {
                styleDescendants = style.findChildren<QObject *>().size();
            }
            if (styleDescendants >= 0) {
                QCOMPARE(style.findChildren<QObject *>().size(), styleDescendants);
            }
        }
        controller.setReducedMotion(false);
        progress->show();
        progress->setEnabled(true);
        progress->setRange(0, 100);
        QCoreApplication::processEvents();
        const auto styleAnimations = style.findChildren<QVariantAnimation *>();
        QVERIFY(styleAnimations.size() <= 1);
        if (!styleAnimations.isEmpty()) {
            QTRY_COMPARE(
                styleAnimations.constFirst()->state(),
                QAbstractAnimation::Stopped);
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

    /** @brief 验证细进度线与独立标签区域不会互相覆盖。 */
    void laysOutThinProgressWithoutCoveringText()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QPalette palette;
        const QColor track(Qt::red);
        const QColor indicator(Qt::green);
        palette.setColor(QPalette::Active, QPalette::Mid, track);
        palette.setColor(QPalette::Active, QPalette::Highlight, indicator);
        palette.setColor(QPalette::Active, QPalette::Text, QColor(Qt::blue));
        palette.setColor(
            QPalette::Active,
            QPalette::HighlightedText,
            QColor(Qt::blue));

        const auto render = [&style, &palette](
                                const QSize &size,
                                bool horizontal,
                                int value) {
            QStyleOptionProgressBar option;
            option.rect = QRect(QPoint(), size);
            option.minimum = 0;
            option.maximum = 100;
            option.progress = value;
            option.text = QStringLiteral("%1%").arg(value);
            option.textVisible = true;
            option.state = QStyle::State_Enabled;
            option.state.setFlag(QStyle::State_Horizontal, horizontal);
            option.palette = palette;
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawControl(QStyle::CE_ProgressBar, &option, &painter);
            return image;
        };

        const QImage horizontal = render(QSize(160, 36), true, 50);
        const QRect horizontalTrack = zzColorBounds(
            render(QSize(160, 36), true, 0),
            track);
        const QRect horizontalIndicator = zzColorBounds(horizontal, indicator);
        const QRect horizontalText = zzColorBounds(horizontal, QColor(Qt::blue));
        QVERIFY(horizontalTrack.height() <= 5);
        QVERIFY(horizontalIndicator.height() <= 5);
        QVERIFY(!horizontalText.isEmpty());
        QVERIFY(horizontalText.bottom() < horizontalTrack.top());
        QVERIFY(qAbs(horizontalIndicator.width()
                     - horizontalTrack.width() / 2)
                <= 2);
        QVERIFY(horizontalTrack.top() > horizontal.height() / 2);

        const QImage vertical = render(QSize(36, 160), false, 25);
        const QRect verticalTrack = zzColorBounds(
            render(QSize(36, 160), false, 0),
            track);
        const QRect verticalIndicator = zzColorBounds(vertical, indicator);
        const QRect verticalText = zzColorBounds(vertical, QColor(Qt::blue));
        QVERIFY(verticalTrack.width() <= 5);
        QVERIFY(verticalIndicator.width() <= 5);
        QVERIFY(!verticalText.isEmpty());
        QVERIFY(verticalText.right() < verticalTrack.left());
        QVERIFY(qAbs(verticalIndicator.height()
                     - verticalTrack.height() / 4)
                <= 2);
        QVERIFY(verticalTrack.left() > vertical.width() / 2);
    }

    /** @brief 验证水平进度标签保留 QProgressBar 的逻辑文字对齐。 */
    void honorsHorizontalProgressLabelAlignment()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const QColor text(Qt::blue);
        QPalette palette;
        palette.setColor(QPalette::All, QPalette::Mid, Qt::red);
        palette.setColor(QPalette::All, QPalette::Highlight, Qt::green);
        palette.setColor(QPalette::All, QPalette::Text, text);

        const auto render = [&style, &palette](Qt::Alignment alignment) {
            QProgressBar progress;
            progress.setStyle(&style);
            progress.setPalette(palette);
            progress.setFont(QFont(QStringLiteral("DejaVu Sans"), 18));
            progress.setRange(0, 100);
            progress.setValue(50);
            progress.setFormat(QStringLiteral("50%"));
            progress.setAlignment(alignment | Qt::AlignVCenter);
            progress.resize(180, 48);
            QImage image(
                progress.size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            progress.render(&painter);
            return image;
        };

        const QRect leftText = zzColorBounds(
            render(Qt::AlignLeft),
            text);
        const QRect rightText = zzColorBounds(
            render(Qt::AlignRight),
            text);
        QVERIFY(!leftText.isEmpty());
        QVERIFY(!rightText.isEmpty());
        QVERIFY(leftText.left() <= 3);
        QVERIFY(rightText.right() >= 176);
        QVERIFY(leftText.left() + 40 < rightText.left());
    }

    /** @brief 验证竖向进度标签旋转，并由 bottomToTop 改变文字方向。 */
    void rotatesVerticalProgressLabelAndHonorsBottomToTop()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const QColor text(Qt::blue);
        const QColor indicator(Qt::green);
        QPalette palette;
        palette.setColor(QPalette::All, QPalette::Mid, Qt::red);
        palette.setColor(QPalette::All, QPalette::Highlight, indicator);
        palette.setColor(QPalette::All, QPalette::Text, text);

        const auto render = [&style, &palette](bool bottomToTop) {
            QProgressBar progress;
            progress.setStyle(&style);
            progress.setPalette(palette);
            progress.setFont(QFont(QStringLiteral("DejaVu Sans"), 18));
            progress.setOrientation(Qt::Vertical);
            progress.setRange(0, 100);
            progress.setValue(50);
            progress.setFormat(QStringLiteral("50%"));
            progress.setAlignment(Qt::AlignCenter);
            progress.setTextDirection(
                bottomToTop
                    ? QProgressBar::BottomToTop
                    : QProgressBar::TopToBottom);
            progress.resize(64, 220);
            QImage image(
                progress.size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            progress.render(&painter);
            return image;
        };

        const QImage bottomToTop = render(true);
        const QImage topToBottom = render(false);
        const QRect bottomToTopText = zzColorBounds(bottomToTop, text);
        const QRect topToBottomText = zzColorBounds(topToBottom, text);
        QVERIFY(!bottomToTopText.isEmpty());
        QVERIFY(!topToBottomText.isEmpty());
        QVERIFY(bottomToTopText.height() > bottomToTopText.width());
        QVERIFY(topToBottomText.height() > topToBottomText.width());
        QCOMPARE(
            zzColorBounds(bottomToTop, indicator),
            zzColorBounds(topToBottom, indicator));
        QVERIFY(bottomToTop != topToBottom);
    }

    /** @brief 验证分离标签区域会进入进度条自然尺寸预算。 */
    void sizesProgressForSeparatedText()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionProgressBar option;
        option.text = QStringLiteral("50%");
        option.textVisible = true;
        option.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        const int textBudget = option.fontMetrics.height() + 8;
        const QSize horizontal = style.sizeFromContents(
            QStyle::CT_ProgressBar,
            &option,
            QSize(1, 1));
        QVERIFY(horizontal.height() >= textBudget);

        option.state.setFlag(QStyle::State_Horizontal, false);
        const QSize vertical = style.sizeFromContents(
            QStyle::CT_ProgressBar,
            &option,
            QSize(1, 1));
        QVERIFY(vertical.width() >= textBudget);

        option.textVisible = false;
        option.state.setFlag(QStyle::State_Horizontal, true);
        const QSize baseHorizontal = style.baseStyle()->sizeFromContents(
            QStyle::CT_ProgressBar,
            &option,
            QSize(1, 1));
        const QSize noTextHorizontal = style.sizeFromContents(
            QStyle::CT_ProgressBar,
            &option,
            QSize(1, 1));
        QCOMPARE(
            noTextHorizontal.height(),
            qMax(baseHorizontal.height(), 4));

        option.state.setFlag(QStyle::State_Horizontal, false);
        const QSize baseVertical = style.baseStyle()->sizeFromContents(
            QStyle::CT_ProgressBar,
            &option,
            QSize(1, 1));
        const QSize noTextVertical = style.sizeFromContents(
            QStyle::CT_ProgressBar,
            &option,
            QSize(1, 1));
        QCOMPARE(noTextVertical.width(), qMax(baseVertical.width(), 4));
    }

    /** @brief 验证极小 option rect 的进度绘制不会越过逻辑边界。 */
    void keepsTinyProgressInsideOptionRect()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        for (const QSize size : {QSize(1, 1), QSize(2, 3), QSize(3, 2)}) {
            for (const bool horizontal : {true, false}) {
                QStyleOptionProgressBar option;
                option.rect = QRect(1, 1, size.width(), size.height());
                option.minimum = 0;
                option.maximum = 100;
                option.progress = 50;
                option.textVisible = false;
                option.state = QStyle::State_Enabled;
                option.state.setFlag(QStyle::State_Horizontal, horizontal);
                option.palette = style.standardPalette();
                const QColor sentinel(Qt::magenta);
                QImage image(
                    size + QSize(2, 2),
                    QImage::Format_ARGB32_Premultiplied);
                image.fill(sentinel);
                QPainter painter(&image);
                style.drawControl(QStyle::CE_ProgressBar, &option, &painter);
                painter.end();
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        if (!option.rect.contains(x, y)) {
                            QCOMPARE(image.pixelColor(x, y), sentinel);
                        }
                    }
                }
            }
        }
    }

    /** @brief 验证样式绘制不改写 QProgressBar 的公开协议。 */
    void preservesLinearProgressProtocol()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QProgressBar progress;
        progress.setStyle(&style);
        progress.setRange(-5, 15);
        progress.setValue(7);
        progress.setFormat(QStringLiteral("完成 %v/%m"));
        progress.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        progress.setOrientation(Qt::Vertical);
        QSignalSpy values(&progress, &QProgressBar::valueChanged);
        progress.setValue(8);
        QCOMPARE(values.count(), 1);
        progress.resize(36, 160);
        QImage image(progress.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        progress.render(&painter);
        painter.end();
        QCOMPARE(progress.format(), QStringLiteral("完成 %v/%m"));
        QCOMPARE(progress.alignment(), Qt::AlignRight | Qt::AlignVCenter);
        QCOMPARE(progress.orientation(), Qt::Vertical);
        QCOMPARE(progress.minimum(), -5);
        QCOMPARE(progress.maximum(), 15);
        QCOMPARE(progress.value(), 8);
        QAccessibleInterface *accessible =
            QAccessible::queryAccessibleInterface(&progress);
        QVERIFY(accessible != nullptr);
        QCOMPARE(accessible->role(), QAccessible::ProgressBar);
        QAccessibleValueInterface *valuesInterface =
            accessible->valueInterface();
        QVERIFY(valuesInterface != nullptr);
        QCOMPARE(valuesInterface->currentValue().toInt(), 8);
    }

    /** @brief 验证范围、方向和各调色板颜色组决定线性进度绘制。 */
    void respectsProgressRangeDirectionAndPalette()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const QColor activeTrack(Qt::red);
        const QColor activeIndicator(Qt::green);
        const QColor disabledTrack(Qt::cyan);
        const QColor disabledIndicator(Qt::yellow);
        QPalette palette;
        palette.setColor(QPalette::Active, QPalette::Mid, activeTrack);
        palette.setColor(
            QPalette::Active,
            QPalette::Highlight,
            activeIndicator);
        palette.setColor(QPalette::Disabled, QPalette::Mid, disabledTrack);
        palette.setColor(
            QPalette::Disabled,
            QPalette::Highlight,
            disabledIndicator);

        const auto render = [&style](const QStyleOptionProgressBar &option) {
            QImage image(
                option.rect.size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawControl(QStyle::CE_ProgressBar, &option, &painter);
            return image;
        };
        const auto horizontal = [&palette, &render, activeTrack, activeIndicator](
                                    Qt::LayoutDirection direction,
                                    bool inverted,
                                    bool fromMaximum) {
            QStyleOptionProgressBar option;
            option.rect = QRect(0, 0, 160, 36);
            option.minimum = 0;
            option.maximum = 100;
            option.progress = 50;
            option.textVisible = false;
            option.direction = direction;
            option.invertedAppearance = inverted;
            option.state = QStyle::State_Enabled | QStyle::State_Horizontal;
            option.palette = palette;
            const QImage image = render(option);
            QStyleOptionProgressBar emptyOption = option;
            emptyOption.progress = 0;
            const QRect track = zzColorBounds(
                render(emptyOption),
                activeTrack);
            const QRect indicator = zzColorBounds(image, activeIndicator);
            QVERIFY(!track.isEmpty());
            QVERIFY(!indicator.isEmpty());
            QVERIFY(qAbs(indicator.width() - track.width() / 2) <= 2);
            if (fromMaximum) {
                QVERIFY(indicator.right() >= track.right() - 1);
            } else {
                QVERIFY(indicator.left() <= track.left() + 1);
            }
        };
        horizontal(Qt::LeftToRight, false, false);
        horizontal(Qt::RightToLeft, false, true);
        horizontal(Qt::LeftToRight, true, true);
        horizontal(Qt::RightToLeft, true, false);

        QStyleOptionProgressBar range;
        range.rect = QRect(0, 0, 160, 36);
        range.minimum = 20;
        range.maximum = 120;
        range.progress = 70;
        range.textVisible = false;
        range.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        range.palette = palette;
        QImage image = render(range);
        QStyleOptionProgressBar emptyRange = range;
        emptyRange.progress = range.minimum;
        const QRect nonZeroTrack = zzColorBounds(
            render(emptyRange),
            activeTrack);
        QVERIFY(qAbs(zzColorBounds(image, activeIndicator).width()
                     - nonZeroTrack.width() / 2)
                <= 2);

        range.minimum = std::numeric_limits<int>::min();
        range.maximum = std::numeric_limits<int>::max();
        range.progress = 0;
        image = render(range);
        QStyleOptionProgressBar emptyExtreme = range;
        emptyExtreme.progress = range.minimum;
        const QRect extremeTrack = zzColorBounds(
            render(emptyExtreme),
            activeTrack);
        QVERIFY(qAbs(zzColorBounds(image, activeIndicator).width()
                     - extremeTrack.width() / 2)
                <= 2);

        range.minimum = 0;
        range.maximum = 100;
        range.progress = 0;
        image = render(range);
        QVERIFY(zzColorBounds(image, activeIndicator).isEmpty());
        const QRect completeTrack = zzColorBounds(image, activeTrack);
        range.progress = 100;
        image = render(range);
        QVERIFY(qAbs(zzColorBounds(image, activeIndicator).width()
                     - completeTrack.width())
                <= 2);
        range.minimum = 9;
        range.maximum = 8;
        range.progress = 9;
        image = render(range);
        QVERIFY(zzColorBounds(image, activeIndicator).isEmpty());

        QStyleOptionProgressBar vertical;
        vertical.rect = QRect(0, 0, 36, 160);
        vertical.minimum = 0;
        vertical.maximum = 100;
        vertical.progress = 25;
        vertical.textVisible = false;
        vertical.state = QStyle::State_Enabled;
        vertical.palette = palette;
        image = render(vertical);
        QStyleOptionProgressBar emptyVertical = vertical;
        emptyVertical.progress = 0;
        const QRect verticalTrack = zzColorBounds(
            render(emptyVertical),
            activeTrack);
        const QRect bottomIndicator = zzColorBounds(image, activeIndicator);
        QVERIFY(bottomIndicator.bottom() >= verticalTrack.bottom() - 1);
        QVERIFY(qAbs(bottomIndicator.height() - verticalTrack.height() / 4)
                <= 2);
        vertical.invertedAppearance = true;
        image = render(vertical);
        const QRect topIndicator = zzColorBounds(image, activeIndicator);
        QVERIFY(topIndicator.top() <= verticalTrack.top() + 1);
        vertical.bottomToTop = false;
        image = render(vertical);
        QCOMPARE(zzColorBounds(image, activeIndicator), topIndicator);
        vertical.bottomToTop = true;
        image = render(vertical);
        QCOMPARE(zzColorBounds(image, activeIndicator), topIndicator);

        range.minimum = 0;
        range.maximum = 100;
        range.progress = 50;
        range.state = QStyle::State_None | QStyle::State_Horizontal;
        image = render(range);
        QVERIFY(zzContainsColor(image, disabledTrack));
        QVERIFY(zzContainsColor(image, disabledIndicator));
        QVERIFY(!zzContainsColor(image, activeTrack));
        QVERIFY(!zzContainsColor(image, activeIndicator));

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            QStyleOptionProgressBar themed;
            themed.rect = QRect(0, 0, 160, 36);
            themed.minimum = 0;
            themed.maximum = 100;
            themed.progress = 50;
            themed.textVisible = false;
            themed.state = QStyle::State_Enabled | QStyle::State_Horizontal;
            themed.palette = style.standardPalette();
            image = render(themed);
            const QPalette::ColorGroup group =
                themed.palette.currentColorGroup();
            QVERIFY(zzContainsColor(
                image,
                themed.palette.color(group, QPalette::Mid)));
            QVERIFY(zzContainsColor(
                image,
                themed.palette.color(group, QPalette::Highlight)));
        }
    }

    /** @brief 验证同一样式的忙碌进度条共享唯一循环动画并及时停机。 */
    void sharesOneBusyProgressAnimation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        auto *first = new QProgressBar(&host);
        auto *second = new QProgressBar(&host);
        first->setStyle(&style);
        second->setStyle(&style);
        first->setRange(0, 0);
        second->setRange(0, 0);
        first->setTextVisible(false);
        second->setTextVisible(false);
        layout->addWidget(first);
        layout->addWidget(second);
        host.resize(240, 80);
        host.show();
        QCoreApplication::processEvents();

        const auto render = [](QProgressBar *progress) {
            QImage image(
                progress->size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            progress->render(&painter);
            return image;
        };

        const QImage firstFrame = render(first);
        render(second);
        const auto animations = style.findChildren<QAbstractAnimation *>();
        QCOMPARE(animations.size(), 1);
        auto *animation = qobject_cast<QVariantAnimation *>(
            animations.constFirst());
        QVERIFY(animation != nullptr);
        QCOMPARE(animation->duration(), 1800);
        QCOMPARE(animation->loopCount(), -1);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QTRY_VERIFY_WITH_TIMEOUT(render(first) != firstFrame, 600);

        first->setRange(0, 100);
        QCoreApplication::processEvents();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        second->hide();
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);

        first->setRange(0, 0);
        render(first);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        first->setEnabled(false);
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);

        QSignalSpy stoppedUpdates(
            animation,
            &QVariantAnimation::valueChanged);
        QTest::qWait(250);
        QCOMPARE(stoppedUpdates.count(), 0);

        first->setEnabled(true);
        render(first);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        controller.setReducedMotion(false);
        render(first);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        first->setStyle(style.baseStyle());
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);

        second->show();
        render(second);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        delete second;
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
    }

    /** @brief 验证减少动效与无控件上下文只绘制居中的固定短段。 */
    void keepsBusyProgressStaticWhenMotionIsReduced()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        QProgressBar progress;
        progress.setStyle(&style);
        progress.setRange(0, 0);
        progress.setTextVisible(false);
        progress.resize(200, 24);
        progress.show();
        QCoreApplication::processEvents();

        const auto renderWidget = [&progress] {
            QImage image(
                progress.size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            progress.render(&painter);
            return image;
        };
        const QImage firstFrame = renderWidget();
        QTest::qWait(100);
        QCOMPARE(renderWidget(), firstFrame);
        QVERIFY(style.findChildren<QAbstractAnimation *>().isEmpty());

        const QColor track(Qt::red);
        const QColor indicator(Qt::green);
        QStyleOptionProgressBar option;
        option.rect = QRect(0, 0, 200, 24);
        option.minimum = 0;
        option.maximum = 0;
        option.progress = 0;
        option.textVisible = false;
        option.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        option.palette.setColor(QPalette::Active, QPalette::Mid, track);
        option.palette.setColor(
            QPalette::Active,
            QPalette::Highlight,
            indicator);
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawControl(QStyle::CE_ProgressBar, &option, &painter, nullptr);
        painter.end();

        const QRect trackBounds = zzColorBounds(image, track);
        const QRect indicatorBounds = zzColorBounds(image, indicator);
        QVERIFY(!trackBounds.isEmpty());
        QVERIFY(!indicatorBounds.isEmpty());
        QVERIFY(qAbs(indicatorBounds.width() - trackBounds.width() * 0.28) <= 2);
        QVERIFY(qAbs(indicatorBounds.center().x() - trackBounds.center().x()) <= 1);
        QVERIFY(style.findChildren<QAbstractAnimation *>().isEmpty());
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
        // 选中指示条应与同一 palette 中的输入焦点、进度条使用相同局部强调色。
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
