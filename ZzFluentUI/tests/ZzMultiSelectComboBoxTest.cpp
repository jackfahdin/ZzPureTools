#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QWindow>
#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace {

/** @brief 处理普通事件和延迟销毁，使 popup 与对象计数稳定。 */
void zzFlushMultiSelectEvents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

/** @brief 返回标准组合框 popup 当前是否可见。 */
bool zzIsMultiSelectPopupVisible(
    const ZzFluentUI::ZzMultiSelectComboBox &box)
{
    return box.view() != nullptr && box.view()->window() != nullptr
        && box.view()->window()->isVisible();
}

/** @brief 从 optionToggled 信号参数中读取选项快照。 */
ZzFluentUI::ZzMultiSelectOption zzMultiSelectOptionArgument(
    const QList<QVariant> &arguments)
{
    if (arguments.isEmpty()) {
        return {};
    }
    return qvariant_cast<ZzFluentUI::ZzMultiSelectOption>(
        arguments.constFirst());
}

/** @brief 创建带稳定键和值载荷的测试选项集合。 */
QList<ZzFluentUI::ZzMultiSelectOption> zzMultiSelectOptions(int count)
{
    QList<ZzFluentUI::ZzMultiSelectOption> options;
    options.reserve(count);
    for (int index = 0; index < count; ++index) {
        options.append({
            QStringLiteral("key-%1").arg(index),
            QStringLiteral("Option %1").arg(index),
            {}, index, true, false});
    }
    return options;
}

/** @brief 将控件渲染到透明图像，供主题与方向检查使用。 */
QImage zzRenderMultiSelect(ZzFluentUI::ZzMultiSelectComboBox *box)
{
    box->resize(240, 36);
    QImage image(box->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    box->render(&painter);
    painter.end();
    return image;
}

/** @brief 判断图像是否包含实际绘制像素。 */
bool zzContainsMultiSelectPixel(const QImage &image)
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

} // namespace

/** @brief 验证多选组合框的值模型、输入语义与对象稳定性。 */
class ZzMultiSelectComboBoxTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableDefaultsAndLargeValueCollection()
    {
        ZzFluentUI::ZzMultiSelectComboBox box;
        QCOMPARE(box.optionCount(), 0);
        QCOMPARE(box.selectionCount(), 0);
        QCOMPARE(box.currentIndex(), -1);
        QCOMPARE(box.maxVisibleItems(), 8);
        QCOMPARE(box.focusPolicy(), Qt::StrongFocus);
        QVERIFY(box.isEditable());
        QLineEdit *editor = box.lineEdit();
        if (editor == nullptr) {
            QFAIL("多选组合框缺少内部只读编辑器");
            return;
        }
        QVERIFY(editor->isReadOnly());
        QVERIFY(!editor->hasFrame());
        QCOMPARE(editor->focusPolicy(), Qt::NoFocus);

        box.setPlaceholderText(QStringLiteral("Select environments"));
        QCOMPARE(box.placeholderText(), QStringLiteral("Select environments"));
        QCOMPARE(editor->placeholderText(), QStringLiteral("Select environments"));

        QList<ZzFluentUI::ZzMultiSelectOption> options =
            zzMultiSelectOptions(40);
        options[1].text = QStringLiteral("Duplicate");
        options[2].text = QStringLiteral("Duplicate");
        options[3].text = QStringLiteral("Comma, value");
        options[4].enabled = false;
        options[4].selected = true;
        options[5].key.clear();
        options[33].key = QStringLiteral("key-0");
        QPixmap pixmap(8, 8);
        pixmap.fill(Qt::green);
        options[39].icon = QIcon(pixmap);
        options[39].data = QVariantMap{{QStringLiteral("id"), 39}};

        QSignalSpy optionsSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::optionsChanged);
        QSignalSpy selectionSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::selectionChanged);
        box.setOptions(options);
        QCOMPARE(optionsSpy.count(), 1);
        QCOMPARE(selectionSpy.count(), 1);
        QCOMPARE(box.optionCount(), 40);
        QCOMPARE(box.currentIndex(), -1);
        QCOMPARE(box.selectedKeys(), QStringList{QStringLiteral("key-4")});

        const QList<ZzFluentUI::ZzMultiSelectOption> normalized =
            box.options();
        QSet<QString> keys;
        for (const ZzFluentUI::ZzMultiSelectOption &option : normalized) {
            QVERIFY(!option.key.isEmpty());
            keys.insert(option.key);
        }
        QCOMPARE(keys.size(), 40);
        QVERIFY(normalized.at(33).key != QStringLiteral("key-0"));
        QCOMPARE(normalized.at(39).data.toMap().value(
                     QStringLiteral("id")).toInt(),
                 39);
        QVERIFY(!normalized.at(39).icon.isNull());

        box.setOptions(normalized);
        QCOMPARE(optionsSpy.count(), 1);
        QCOMPARE(selectionSpy.count(), 1);
        const QString generated = box.addOption(
            QStringLiteral("Added"), 41, {}, true);
        QVERIFY(!generated.isEmpty());
        QCOMPARE(box.optionCount(), 41);
        QCOMPARE(optionsSpy.count(), 2);
        QCOMPARE(selectionSpy.count(), 2);
        QVERIFY(box.removeOption(generated));
        QCOMPARE(optionsSpy.count(), 3);
        QCOMPARE(selectionSpy.count(), 3);
        QVERIFY(!box.removeOption(QStringLiteral("missing")));
        QVERIFY(!box.removeOptionAt(-1));
        QVERIFY(!box.removeOptionAt(box.optionCount()));
        QVERIFY(box.removeOptionAt(39));
        QCOMPARE(box.optionCount(), 39);
        box.clearOptions();
        QCOMPARE(box.optionCount(), 0);
        QCOMPARE(box.selectionCount(), 0);
        QCOMPARE(box.currentIndex(), -1);
        box.clearOptions();
        QCOMPARE(optionsSpy.count(), 5);
        QCOMPARE(selectionSpy.count(), 4);
    }

    void exposesConsistentModelRolesFlagsAndMutations()
    {
        ZzFluentUI::ZzMultiSelectComboBox box;
        QPixmap pixmap(8, 8);
        pixmap.fill(Qt::red);
        const QIcon icon(pixmap);
        box.setOptions({
            {QStringLiteral("enabled"), QStringLiteral("Enabled"), icon,
             17, true, false},
            {QStringLiteral("disabled"), QStringLiteral("Disabled"), {},
             QStringLiteral("payload"), false, true}});
        QAbstractItemModel *model = box.model();
        if (model == nullptr) {
            QFAIL("多选组合框缺少内部选项模型");
            return;
        }
        QAbstractItemModelTester tester(
            model,
            QAbstractItemModelTester::FailureReportingMode::QtTest,
            &box);
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->columnCount(), 1);
        QCOMPARE(model->rowCount(model->index(0, 0)), 0);
        QVERIFY(!model->data(QModelIndex(), Qt::DisplayRole).isValid());

        const QModelIndex enabled = model->index(0, 0);
        const QModelIndex disabled = model->index(1, 0);
        QCOMPARE(model->data(enabled, Qt::DisplayRole).toString(),
                 QStringLiteral("Enabled"));
        QCOMPARE(model->data(enabled, Qt::EditRole).toString(),
                 QStringLiteral("Enabled"));
        QCOMPARE(model->data(enabled, Qt::UserRole).toInt(), 17);
        QCOMPARE(model->data(enabled,
                             ZzFluentUI::ZzMultiSelectComboBox::KeyRole)
                     .toString(),
                 QStringLiteral("enabled"));
        QCOMPARE(model->data(enabled, Qt::CheckStateRole).toInt(),
                 static_cast<int>(Qt::Unchecked));
        QVERIFY(!qvariant_cast<QIcon>(
                     model->data(enabled, Qt::DecorationRole)).isNull());
        QVERIFY(enabled.flags() & Qt::ItemIsEnabled);
        QVERIFY(enabled.flags() & Qt::ItemIsSelectable);
        QVERIFY(enabled.flags() & Qt::ItemIsUserCheckable);
        QVERIFY(!(disabled.flags() & Qt::ItemIsEnabled));
        QVERIFY(!(disabled.flags() & Qt::ItemIsUserCheckable));
        QCOMPARE(model->data(disabled, Qt::CheckStateRole).toInt(),
                 static_cast<int>(Qt::Checked));

        QSignalSpy selectionSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::selectionChanged);
        QVERIFY(model->setData(enabled, Qt::Checked, Qt::CheckStateRole));
        QCOMPARE(selectionSpy.count(), 1);
        QCOMPARE(box.selectedText(), QStringLiteral("Enabled, Disabled"));
        QCOMPARE(box.currentText(), box.selectedText());
        QCOMPARE(box.currentIndex(), -1);
        QVERIFY(!model->setData(enabled, Qt::PartiallyChecked,
                               Qt::CheckStateRole));
        QVERIFY(!model->setData(QModelIndex(), Qt::Checked,
                                Qt::CheckStateRole));

        QPersistentModelIndex persistent(enabled);
        box.setOptions({
            {QStringLiteral("replacement"), QStringLiteral("Replacement"),
             {}, {}, true, false}});
        QVERIFY(!persistent.isValid());
        QCOMPARE(model->rowCount(), 1);
    }

    void appliesBulkSelectionOnceAndPreservesStableIdentity()
    {
        ZzFluentUI::ZzMultiSelectComboBox box;
        box.setOptions({
            {QStringLiteral("first"), QStringLiteral("Same"), {}, 1,
             true, false},
            {QStringLiteral("second"), QStringLiteral("Same"), {}, 2,
             true, false},
            {QStringLiteral("comma"), QStringLiteral("A, B"), {}, 3,
             true, false},
            {QStringLiteral("disabled"), QStringLiteral("Disabled"), {},
             4, false, true}});
        QSignalSpy selectionSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::selectionChanged);
        QSignalSpy toggledSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::optionToggled);

        box.setSelectedKeys({
            QStringLiteral("comma"), QStringLiteral("first"),
            QStringLiteral("first"), QStringLiteral("missing")});
        QCOMPARE(selectionSpy.count(), 1);
        QCOMPARE(toggledSpy.count(), 0);
        QCOMPARE(box.selectedKeys(),
                 QStringList({QStringLiteral("first"),
                              QStringLiteral("comma")}));
        QCOMPARE(box.selectedIndexes(), QList<int>({0, 2}));
        QCOMPARE(box.selectedText(), QStringLiteral("Same, A, B"));
        QCOMPARE(box.selectionCount(), 2);
        QCOMPARE(box.selectedOptions().at(1).data.toInt(), 3);

        box.setSelectedIndexes({3, 1, 1, -1, box.optionCount()});
        QCOMPARE(selectionSpy.count(), 2);
        QCOMPARE(box.selectedKeys(),
                 QStringList({QStringLiteral("second"),
                              QStringLiteral("disabled")}));
        box.selectAll();
        QCOMPARE(selectionSpy.count(), 3);
        QCOMPARE(box.selectionCount(), 4);
        box.selectAll();
        QCOMPARE(selectionSpy.count(), 3);
        box.clearSelection();
        QCOMPARE(selectionSpy.count(), 4);
        box.clearSelection();
        QCOMPARE(selectionSpy.count(), 4);
        QVERIFY(box.setOptionSelected(QStringLiteral("disabled"), true));
        QVERIFY(!box.setOptionSelected(QStringLiteral("disabled"), true));
        QVERIFY(!box.setOptionSelected(QStringLiteral("missing"), true));
        QVERIFY(box.setOptionSelectedAt(0, true));
        QVERIFY(!box.setOptionSelectedAt(-1, true));
        QVERIFY(!box.setOptionSelectedAt(box.optionCount(), true));
        QCOMPARE(selectionSpy.count(), 6);
        QCOMPARE(toggledSpy.count(), 0);
    }

    void keepsPopupOpenAcrossMouseAndKeyboardToggles()
    {
        ZzFluentUI::ZzMultiSelectComboBox box;
        box.setOptions({
            {QStringLiteral("first"), QStringLiteral("First"), {}, 1,
             true, false},
            {QStringLiteral("second"), QStringLiteral("Second"), {}, 2,
             true, false},
            {QStringLiteral("disabled"), QStringLiteral("Disabled"), {},
             3, false, false}});
        box.resize(240, 36);
        box.move(32, 32);
        box.show();
        box.setFocus();
        zzFlushMultiSelectEvents();
        box.showPopup();
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QAbstractItemView *view = box.view();
        if (view == nullptr) {
            QFAIL("多选组合框缺少标准popup视图");
            return;
        }

        QSignalSpy selectionSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::selectionChanged);
        QSignalSpy toggledSpy(
            &box,
            &ZzFluentUI::ZzMultiSelectComboBox::optionToggled);
        for (const int row : {0, 1}) {
            const QModelIndex item = box.model()->index(row, 0);
            view->scrollTo(item);
            zzFlushMultiSelectEvents();
            const QRect itemRect = view->visualRect(item);
            QVERIFY(!itemRect.isEmpty());
            QTest::mouseClick(
                view->viewport(),
                Qt::LeftButton,
                Qt::NoModifier,
                itemRect.center());
            zzFlushMultiSelectEvents();
            QVERIFY(zzIsMultiSelectPopupVisible(box));
        }
        QCOMPARE(box.selectedKeys(),
                 QStringList({QStringLiteral("first"),
                              QStringLiteral("second")}));
        QCOMPARE(selectionSpy.count(), 2);
        QCOMPARE(toggledSpy.count(), 2);
        QCOMPARE(zzMultiSelectOptionArgument(toggledSpy.at(1)).key,
                 QStringLiteral("second"));
        QCOMPARE(toggledSpy.at(1).at(1).toBool(), true);
        const QModelIndex disabled = box.model()->index(2, 0);
        const QRect disabledRect = view->visualRect(disabled);
        QVERIFY(!disabledRect.isEmpty());
        QTest::mouseClick(
            view->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            disabledRect.center());
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QCOMPARE(box.selectionCount(), 2);
        QCOMPARE(toggledSpy.count(), 2);

        view->setCurrentIndex(box.model()->index(0, 0));
        QTest::keyClick(view, Qt::Key_Space);
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QCOMPARE(box.selectedKeys(), QStringList{QStringLiteral("second")});
        view->setCurrentIndex(box.model()->index(1, 0));
        QTest::keyPress(view, Qt::Key_Return);
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QCOMPARE(box.selectionCount(), 0);
        QTest::keyRelease(view, Qt::Key_Return);
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));

        view->setCurrentIndex(QModelIndex());
        QTest::keyClick(view, Qt::Key_Enter);
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QCOMPARE(box.selectionCount(), 0);
        QCOMPARE(toggledSpy.count(), 4);
        QCOMPARE(box.currentIndex(), -1);
    }

    void preservesNativeCloseSummaryWheelAndAccessibility()
    {
        QWidget host;
        ZzFluentUI::ZzMultiSelectComboBox box(&host);
        box.setGeometry(8, 8, 240, 36);
        host.resize(300, 120);
        box.setAccessibleName(QStringLiteral("Environments"));
        box.setOptions({
            {QStringLiteral("first"), QStringLiteral("First"), {}, {},
             true, true},
            {QStringLiteral("second"), QStringLiteral("Second"), {}, {},
             true, false}});
        host.show();
        box.setFocus();
        zzFlushMultiSelectEvents();

        QCOMPARE(box.selectedText(), QStringLiteral("First"));
        QCOMPARE(box.currentText(), QStringLiteral("First"));
        QCOMPARE(box.currentIndex(), -1);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&box);
        if (interface == nullptr) {
            QFAIL("多选组合框缺少标准QComboBox无障碍接口");
            return;
        }
        QCOMPARE(interface->role(), QAccessible::ComboBox);
        QCOMPARE(interface->text(QAccessible::Value),
                 QStringLiteral("First"));
        QVERIFY(interface->state().focusable);

        const QList<int> beforeWheel = box.selectedIndexes();
        QWheelEvent wheel(
            QPointF(20.0, 18.0),
            QPointF(box.mapToGlobal(QPoint(20, 18))),
            QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&box, &wheel);
        QCOMPARE(box.selectedIndexes(), beforeWheel);
        QCOMPARE(box.currentIndex(), -1);

        box.showPopup();
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QTest::keyClick(box.view(), Qt::Key_Escape);
        zzFlushMultiSelectEvents();
        QVERIFY(!zzIsMultiSelectPopupVisible(box));

        QTest::mouseClick(box.lineEdit(), Qt::LeftButton);
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QTest::keyClick(box.view(), Qt::Key_Tab);
        zzFlushMultiSelectEvents();
        QVERIFY(!zzIsMultiSelectPopupVisible(box));

        box.showPopup();
        zzFlushMultiSelectEvents();
        QVERIFY(zzIsMultiSelectPopupVisible(box));
        QWindow *popupWindow = box.view()->window()->windowHandle();
        if (popupWindow == nullptr) {
            QFAIL("多选组合框popup缺少窗口句柄");
            return;
        }
        QTest::mouseClick(
            popupWindow,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(-20, -20));
        zzFlushMultiSelectEvents();
        QVERIFY(!zzIsMultiSelectPopupVisible(box));

        box.setEnabled(false);
        QVERIFY(interface->state().disabled);
    }

    void rendersThroughFluentStyleAcrossThemesAndDirections()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzMultiSelectComboBox box;
        box.setStyle(&style);
        QPixmap pixmap(12, 12);
        pixmap.fill(Qt::blue);
        box.setOptions({
            {QStringLiteral("long"),
             QStringLiteral("A long selected value that elides safely"),
             QIcon(pixmap), {}, true, true},
            {QStringLiteral("disabled"), QStringLiteral("Disabled"), {},
             {}, false, false}});
        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            box.setPalette(style.standardPalette());
            for (const Qt::LayoutDirection direction : {
                     Qt::LeftToRight,
                     Qt::RightToLeft}) {
                box.setLayoutDirection(direction);
                const QImage image = zzRenderMultiSelect(&box);
                QVERIFY(zzContainsMultiSelectPixel(image));
                QCOMPARE(box.style(), &style);
                QCOMPARE(box.layoutDirection(), direction);
                QVERIFY(box.sizeHint().height() >= 32);
            }
        }
    }

    void keepsObjectsStableAcrossCollectionAndStateChanges()
    {
        constexpr int boxCount = 100;
        constexpr int optionsPerBox = 20;
        constexpr int rounds = 1000;
        QWidget host;
        std::vector<ZzFluentUI::ZzMultiSelectComboBox *> boxes;
        boxes.reserve(boxCount);
        const QList<ZzFluentUI::ZzMultiSelectOption> options =
            zzMultiSelectOptions(optionsPerBox);
        for (int index = 0; index < boxCount; ++index) {
            auto *box = new ZzFluentUI::ZzMultiSelectComboBox(&host);
            box->setOptions(options);
            box->setGeometry((index % 10) * 124,
                             (index / 10) * 40,
                             116, 32);
            boxes.push_back(box);
        }
        host.resize(1240, 400);
        host.show();
        zzFlushMultiSelectEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();

        for (int round = 0; round < rounds; ++round) {
            ZzFluentUI::ZzMultiSelectComboBox *box =
                boxes.at(static_cast<std::size_t>(round % boxCount));
            const int row = round % optionsPerBox;
            QVERIFY(box->setOptionSelectedAt(row, true));
            box->setSelectedIndexes({row, (row + 1) % optionsPerBox});
            box->selectAll();
            box->clearSelection();
            box->setPlaceholderText(QStringLiteral("Temporary"));
            box->setLayoutDirection(Qt::RightToLeft);
            box->setEnabled(false);
            box->setEnabled(true);
            box->setLayoutDirection(Qt::LeftToRight);
            box->setPlaceholderText({});
        }
        zzFlushMultiSelectEvents();
        QCOMPARE(host.findChildren<QObject *>().size(), initialDescendants);
        QCOMPARE(host.findChildren<QAbstractAnimation *>().size(),
                 initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        for (const ZzFluentUI::ZzMultiSelectComboBox *box : boxes) {
            QCOMPARE(box->optionCount(), optionsPerBox);
            QCOMPARE(box->selectionCount(), 0);
            QCOMPARE(box->currentIndex(), -1);
        }

        ZzFluentUI::ZzMultiSelectComboBox large;
        const qsizetype beforeItems = large.findChildren<QObject *>().size();
        large.setOptions(zzMultiSelectOptions(10000));
        QCOMPARE(large.optionCount(), 10000);
        QCOMPARE(large.findChildren<QObject *>().size(), beforeItems);
    }
};

QTEST_MAIN(ZzMultiSelectComboBoxTest)

#include "ZzMultiSelectComboBoxTest.moc"
