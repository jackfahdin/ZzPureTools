#include <array>
#include <cstddef>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QFont>
#include <QtGui/QImage>
#include <QtGui/QIntValidator>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace {

/** @brief 向测试公开标准组合框的样式选项初始化结果。 */
class ZzComboBoxStyleOptionProbe final : public QComboBox
{
public:
    /** @brief 返回与当前控件状态一致的组合框样式选项。 */
    [[nodiscard]] QStyleOptionComboBox styleOption() const
    {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        return option;
    }
};

/** @brief 立即处理事件与延迟销毁对象，使对象预算可重复测量。 */
void zzFlushDeferredObjects()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

/** @brief 判断图像是否包含实际绘制出的非透明像素。 */
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

/** @brief 将组合框渲染到固定图像，供跨主题状态检查使用。 */
QImage zzRenderComboBox(QComboBox *comboBox)
{
    comboBox->resize(196, 36);
    QImage image(comboBox->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    comboBox->render(&painter);
    painter.end();
    return image;
}

} // namespace

/** @brief 验证标准 QComboBox 的原生语义、公开 popup 和对象预算。 */
class ZzComboBoxControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void paintsLabelOnlyInLabelControl()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionComboBox option;
        option.rect = QRect(0, 0, 220, 36);
        option.state = QStyle::State_Enabled;
        option.palette = style.standardPalette();
        const auto panel = [&] {
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawComplexControl(QStyle::CC_ComboBox, &option, &painter);
            return image;
        };
        const QImage empty = panel();
        option.currentText = QStringLiteral("Only one label");
        QPixmap icon(16, 16);
        icon.fill(Qt::red);
        option.currentIcon = QIcon(icon);
        option.iconSize = icon.size();
        QCOMPARE(panel(), empty);

        QImage label(empty.size(), empty.format());
        label.fill(Qt::transparent);
        QPainter painter(&label);
        style.drawControl(QStyle::CE_ComboBoxLabel, &option, &painter);
        painter.end();
        QVERIFY(zzContainsOpaquePixel(label));
    }

    void respectsFrameAndSeparatesEditableSurface()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionComboBox option;
        option.rect = QRect(0, 0, 220, 36);
        option.state = QStyle::State_Enabled;
        option.palette = style.standardPalette();
        option.palette.setColor(QPalette::Button, QColor(Qt::cyan));
        option.palette.setColor(QPalette::Base, QColor(Qt::magenta));
        option.subControls = QStyle::SC_ComboBoxFrame;
        const auto panel = [&] {
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawComplexControl(QStyle::CC_ComboBox, &option, &painter);
            return image;
        };
        QCOMPARE(panel().pixelColor(100, 18), QColor(Qt::cyan));
        option.editable = true;
        QCOMPARE(panel().pixelColor(100, 18), QColor(Qt::magenta));
        option.frame = false;
        QVERIFY(!zzContainsOpaquePixel(panel()));
        option.frame = true;
        option.subControls = QStyle::SC_None;
        QVERIFY(!zzContainsOpaquePixel(panel()));
    }

    void usesReadableHighContrastPopupText()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&controller);
        QComboBox combo;
        combo.setStyle(&style);
        combo.addItem(QStringLiteral("Readable selection"));
        QStyleOptionViewItem option;
        option.rect = QRect(0, 0, 240, 36);
        option.state = QStyle::State_Enabled | QStyle::State_Selected;
        option.palette = style.standardPalette();
        option.features = QStyleOptionViewItem::HasDisplay;
        option.text = combo.itemText(0);
        // 模型背景不能覆盖选中表面，否则高对比反色文字会变为黑底黑字。
        option.backgroundBrush = QBrush(Qt::black);
        option.font.setPixelSize(20);
        option.fontMetrics = QFontMetrics(option.font);
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawControl(QStyle::CE_ItemViewItem, &option, &painter, combo.view());
        painter.end();
        const QColor foreground = option.palette.color(QPalette::HighlightedText);
        QCOMPARE(image.pixelColor(230, 18), option.palette.color(QPalette::Highlight));
        int textPixels = 0;
        for (int y = 8; y < 28; ++y) {
            for (int x = 20; x < 210; ++x) {
                textPixels += image.pixelColor(x, y) == foreground ? 1 : 0;
            }
        }
        QVERIFY2(textPixels > 20, "高对比选中项必须使用可读的反色前景");
        int indicatorPixels = 0;
        for (int y = 8; y < 28; ++y) {
            for (int x = 2; x < 10; ++x) {
                indicatorPixels += image.pixelColor(x, y) == foreground ? 1 : 0;
            }
        }
        QVERIFY2(indicatorPixels > 5, "高对比指示条不能与选中背景同色");
        option.state = QStyle::State_Enabled;
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawControl(QStyle::CE_ItemViewItem, &option, &painter, combo.view());
        painter.end();
        QCOMPARE(image.pixelColor(230, 18), QColor(Qt::black));
    }

    void keepsNaturalSizeContentVisible_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("hasIcon");
        QTest::addColumn<int>("fontPointSize");
        QTest::addColumn<bool>("editable");

        QTest::newRow("long-text")
            << QStringLiteral("Production cluster with extended label")
            << false
            << 0
            << false;
        QTest::newRow("icon")
            << QStringLiteral("Remote session with icon")
            << true
            << 0
            << false;
        QTest::newRow("large-font")
            << QStringLiteral("Large font selection")
            << false
            << 18
            << false;
        QTest::newRow("editable")
            << QStringLiteral("Editable custom session name")
            << false
            << 0
            << true;
    }

    void keepsNaturalSizeContentVisible()
    {
        QFETCH(QString, text);
        QFETCH(bool, hasIcon);
        QFETCH(int, fontPointSize);
        QFETCH(bool, editable);

        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        for (const Qt::LayoutDirection direction : {
                 Qt::LeftToRight,
                 Qt::RightToLeft}) {
            ZzComboBoxStyleOptionProbe comboBox;
            comboBox.setStyle(&style);
            comboBox.setLayoutDirection(direction);
            comboBox.setSizeAdjustPolicy(QComboBox::AdjustToContents);
            comboBox.setEditable(editable);
            if (fontPointSize > 0) {
                QFont font = comboBox.font();
                font.setPointSize(fontPointSize);
                comboBox.setFont(font);
            }

            QPixmap decoration(16, 16);
            decoration.fill(Qt::blue);
            if (hasIcon) {
                comboBox.setIconSize(decoration.size());
                comboBox.addItem(QIcon(decoration), text);
            } else {
                comboBox.addItem(text);
            }
            if (editable) {
                comboBox.setEditText(text);
            }

            comboBox.ensurePolished();
            comboBox.resize(comboBox.sizeHint());
            const QStyleOptionComboBox option = comboBox.styleOption();
            const QRect editField = style.subControlRect(
                QStyle::CC_ComboBox,
                &option,
                QStyle::SC_ComboBoxEditField,
                &comboBox);
            const int decorationWidth = hasIcon
                ? comboBox.iconSize().width() + 4
                : 0;
            const int requiredWidth = comboBox.fontMetrics().boundingRect(
                text).width()
                + decorationWidth
                + 4;

            QVERIFY2(
                editField.width() >= requiredWidth,
                qPrintable(QStringLiteral(
                    "自然尺寸裁切内容：direction=%1 hint=%2 edit=%3 required=%4")
                               .arg(direction == Qt::LeftToRight
                                        ? QStringLiteral("LTR")
                                        : QStringLiteral("RTL"))
                               .arg(comboBox.sizeHint().width())
                               .arg(editField.width())
                               .arg(requiredWidth)));
        }
    }

    void preservesModelAndSelectionSemantics()
    {
        QStandardItemModel model;
        auto *group = new QStandardItem(QStringLiteral("Environments"));
        auto *local = new QStandardItem(QStringLiteral("Local"));
        auto *staging = new QStandardItem(QStringLiteral("Staging"));
        auto *production = new QStandardItem(QStringLiteral("Production"));
        QPixmap decoration(8, 8);
        decoration.fill(Qt::red);
        local->setData(QIcon(decoration), Qt::DecorationRole);
        local->setData(17, Qt::UserRole);
        staging->setEnabled(false);
        group->appendRow(local);
        group->appendRow(staging);
        group->appendRow(production);
        model.appendRow(group);

        QComboBox comboBox;
        comboBox.setModel(&model);
        comboBox.setRootModelIndex(model.index(0, 0));
        comboBox.setPlaceholderText(QStringLiteral("Select environment"));
        QCOMPARE(comboBox.count(), 3);
        QCOMPARE(comboBox.itemText(0), QStringLiteral("Local"));
        QCOMPARE(comboBox.itemData(0, Qt::UserRole).toInt(), 17);
        QVERIFY(!comboBox.itemIcon(0).isNull());
        QVERIFY(!(model.index(1, 0, comboBox.rootModelIndex()).flags()
                  & Qt::ItemIsEnabled));

        QSignalSpy indexSpy(&comboBox, &QComboBox::currentIndexChanged);
        comboBox.setCurrentIndex(2);
        QCOMPARE(comboBox.currentText(), QStringLiteral("Production"));
        QCOMPARE(comboBox.currentData(Qt::DisplayRole).toString(),
                 QStringLiteral("Production"));
        QCOMPARE(indexSpy.count(), 1);

        comboBox.setCurrentIndex(-1);
        QCOMPARE(comboBox.currentIndex(), -1);
        QVERIFY(comboBox.currentText().isEmpty());
        QCOMPARE(comboBox.placeholderText(),
                 QStringLiteral("Select environment"));

        group->insertRow(1, new QStandardItem(QStringLiteral("Preview")));
        QCOMPARE(comboBox.count(), 4);
        QCOMPARE(comboBox.itemText(1), QStringLiteral("Preview"));
        group->removeRow(1);
        QCOMPARE(comboBox.count(), 3);

        model.clear();
        QCOMPARE(comboBox.count(), 0);
        QCOMPARE(comboBox.currentIndex(), -1);
    }

    void preservesEditableValidatorCompleterAndInsertion()
    {
        QComboBox comboBox;
        comboBox.setEditable(true);
        comboBox.setInsertPolicy(QComboBox::InsertAtBottom);
        comboBox.setDuplicatesEnabled(false);
        comboBox.addItems({QStringLiteral("12"), QStringLiteral("24")});
        QLineEdit *editor = comboBox.lineEdit();
        QVERIFY(editor != nullptr);
        editor->setValidator(new QIntValidator(0, 999, editor));
        auto *completer = new QCompleter(
            QStringList{QStringLiteral("120"), QStringLiteral("240")},
            &comboBox);
        comboBox.setCompleter(completer);
        QCOMPARE(comboBox.completer(), completer);
        QCOMPARE(comboBox.insertPolicy(), QComboBox::InsertAtBottom);
        QVERIFY(!comboBox.duplicatesEnabled());

        QSignalSpy editSpy(&comboBox, &QComboBox::editTextChanged);
        QSignalSpy textSpy(&comboBox, &QComboBox::currentTextChanged);
        editor->selectAll();
        QTest::keyClicks(editor, QStringLiteral("42"));
        QCOMPARE(editor->text(), QStringLiteral("42"));
        QTest::keyClicks(editor, QStringLiteral("x"));
        QCOMPARE(editor->text(), QStringLiteral("42"));
        QVERIFY(editor->hasAcceptableInput());
        editor->selectAll();
        QVERIFY(editor->hasSelectedText());

        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(comboBox.count(), 3);
        QCOMPARE(comboBox.itemText(2), QStringLiteral("42"));
        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(comboBox.count(), 3);
        QVERIFY(editSpy.count() >= 2);
        QVERIFY(textSpy.count() >= 2);
    }

    void avoidsNestedEditorSurface()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QComboBox comboBox;
        comboBox.setEditable(true);
        comboBox.setStyle(&style);
        comboBox.resize(196, 36);

        QLineEdit *const editor = comboBox.lineEdit();
        QVERIFY(editor != nullptr);
        QStyleOption option;
        option.initFrom(editor);
        option.rect = editor->rect();
        const QColor background = style.standardPalette().color(
            QPalette::Base);
        QImage image(editor->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(background);
        QPainter painter(&image);
        style.drawPrimitive(
            QStyle::PE_PanelLineEdit,
            &option,
            &painter,
            editor);
        painter.end();

        QImage expected(image.size(), image.format());
        expected.fill(background);
        QCOMPARE(image, expected);
    }

    void preservesKeyboardMouseAndPopupSemantics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QComboBox comboBox;
        comboBox.setStyle(&style);
        comboBox.addItems({
            QStringLiteral("Alpha"),
            QStringLiteral("Beta"),
            QStringLiteral("Gamma")});
        comboBox.resize(180, 36);
        comboBox.show();
        comboBox.setFocus();
        QCoreApplication::processEvents();

        comboBox.setCurrentIndex(0);
        QTest::keyClick(&comboBox, Qt::Key_Down);
        QCOMPARE(comboBox.currentIndex(), 1);
        QTest::keyClick(&comboBox, Qt::Key_Up);
        QCOMPARE(comboBox.currentIndex(), 0);
        QTest::keyClick(&comboBox, Qt::Key_End);
        QCOMPARE(comboBox.currentIndex(), 2);
        QTest::keyClick(&comboBox, Qt::Key_Home);
        QCOMPARE(comboBox.currentIndex(), 0);

        QAbstractItemView *popupView = comboBox.view();
        QVERIFY(popupView != nullptr);
        QVERIFY(popupView->window() != nullptr);
        // 本测试按控件安装样式；真实应用通过 QApplication 安装同一样式。
        popupView->window()->setStyle(&style);
        popupView->setStyle(&style);
        popupView->viewport()->setStyle(&style);
        QTest::keyClick(
            &comboBox,
            Qt::Key_Down,
            Qt::AltModifier);
        QCoreApplication::processEvents();
        QVERIFY(popupView->window()->isVisible());
        QVERIFY(popupView->window()->geometry().top()
            >= comboBox.mapToGlobal(comboBox.rect().bottomLeft()).y());
        // 列表视口必须避开弹出面板的圆角，不能把四角边框覆盖成缺口。
        const QPoint viewportOrigin = popupView->viewport()->mapTo(
            popupView->window(), QPoint(0, 0));
        QVERIFY(viewportOrigin.x() >= 4);
        QVERIFY(viewportOrigin.y() >= 4);
        QTest::keyClick(popupView, Qt::Key_Escape);
        QCoreApplication::processEvents();
        QVERIFY(!popupView->window()->isVisible());

        QSignalSpy activatedSpy(
            &comboBox,
            qOverload<int>(&QComboBox::activated));
        comboBox.showPopup();
        QCoreApplication::processEvents();
        const QModelIndex beta = comboBox.model()->index(1, 0);
        popupView->scrollTo(beta);
        QCoreApplication::processEvents();
        const QRect betaRect = popupView->visualRect(beta);
        QVERIFY(!betaRect.isEmpty());
        QTest::mouseClick(
            popupView->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            betaRect.center());
        QCoreApplication::processEvents();
        QCOMPARE(comboBox.currentIndex(), 1);
        QCOMPARE(activatedSpy.count(), 1);
        QVERIFY(!popupView->window()->isVisible());

        comboBox.showPopup();
        QCoreApplication::processEvents();
        popupView->setCurrentIndex(comboBox.model()->index(2, 0));
        QTest::keyClick(popupView, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(comboBox.currentIndex(), 2);
    }

    void rendersDecorationRtlAndEveryTheme()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QComboBox comboBox;
        comboBox.setStyle(&style);
        QPixmap decoration(12, 12);
        decoration.fill(Qt::green);
        comboBox.addItem(QIcon(decoration), QStringLiteral("Long selection"));
        comboBox.addItem(QStringLiteral("Disabled"));
        comboBox.setLayoutDirection(Qt::RightToLeft);

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            comboBox.setPalette(style.standardPalette());
            const QImage image = zzRenderComboBox(&comboBox);
            QVERIFY(zzContainsOpaquePixel(image));
            QVERIFY(!comboBox.itemIcon(0).isNull());
            QCOMPARE(comboBox.layoutDirection(), Qt::RightToLeft);
        }
    }

    void preservesAccessibleComboBoxRoleAndState()
    {
        QComboBox comboBox;
        comboBox.setAccessibleName(QStringLiteral("Environment"));
        comboBox.addItems({QStringLiteral("Local"), QStringLiteral("Remote")});
        comboBox.setCurrentIndex(1);
        comboBox.setEditable(true);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&comboBox);
        if (interface == nullptr) {
            QFAIL("标准组合框缺少无障碍接口");
            return;
        }
        QCOMPARE(interface->role(), QAccessible::ComboBox);
        QCOMPARE(comboBox.accessibleName(), QStringLiteral("Environment"));
        const QString interfaceName = interface->text(QAccessible::Name);
        QVERIFY2(
            interfaceName == comboBox.accessibleName()
                || interfaceName == comboBox.currentText(),
            qPrintable(QStringLiteral(
                "无障碍名称既不是显式名称也不是当前值：%1")
                           .arg(interfaceName)));
        QCOMPARE(interface->text(QAccessible::Value),
                 QStringLiteral("Remote"));
        QVERIFY(interface->state().focusable);
        QVERIFY(interface->state().editable);

        comboBox.setEnabled(false);
        QVERIFY(interface->state().disabled);
    }

    void keepsPerInstanceInfrastructureStable()
    {
        constexpr int comboBoxCount = 100;
        constexpr int columnCount = 10;
        constexpr int stateChangeRounds = 1000;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        std::vector<QComboBox *> comboBoxes;
        comboBoxes.reserve(comboBoxCount);

        for (int index = 0; index < comboBoxCount; ++index) {
            auto *comboBox = new QComboBox(&host);
            comboBox->setStyle(&style);
            comboBox->setGeometry(
                (index % columnCount) * 124,
                (index / columnCount) * 40,
                116,
                32);
            comboBox->addItems({
                QStringLiteral("Alpha"),
                QStringLiteral("Beta"),
                QStringLiteral("Gamma")});
            comboBox->setEditable((index % 2) != 0);
            comboBox->setCurrentIndex(index % comboBox->count());
            comboBoxes.push_back(comboBox);
        }
        host.resize(columnCount * 124, 400);
        host.show();
        for (int index = 0; index < comboBoxCount; ++index) {
            QComboBox *comboBox = comboBoxes[static_cast<std::size_t>(index)];
            const bool editable = (index % 2) != 0;
            comboBox->setEditable(!editable);
            comboBox->setEditable(editable);
            comboBox->setFocus();
        }
        host.setFocus();
        zzFlushDeferredObjects();

        const qsizetype initialObjects =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        for (int round = 0; round < stateChangeRounds; ++round) {
            const int index = round % comboBoxCount;
            QComboBox *comboBox = comboBoxes[static_cast<std::size_t>(index)];
            const bool editable = (index % 2) != 0;
            const int currentIndex = index % comboBox->count();
            comboBox->setCurrentIndex((currentIndex + 1) % comboBox->count());
            comboBox->setEnabled(false);
            comboBox->setEditable(editable);
            comboBox->setLayoutDirection(Qt::RightToLeft);
            comboBox->setItemData(0, round, Qt::UserRole);
            comboBox->setItemData(0, QVariant(), Qt::UserRole);
            comboBox->setLayoutDirection(Qt::LeftToRight);
            comboBox->setEditable(editable);
            comboBox->setEnabled(true);
            comboBox->setCurrentIndex(currentIndex);
            comboBox->setFocus();
        }
        host.setFocus();
        zzFlushDeferredObjects();

        QCOMPARE(host.findChildren<QObject *>().size(), initialObjects);
        QCOMPARE(host.findChildren<QAbstractAnimation *>().size(),
                 initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
    }
};

QTEST_MAIN(ZzComboBoxControlsTest)

#include "ZzComboBoxControlsTest.moc"
