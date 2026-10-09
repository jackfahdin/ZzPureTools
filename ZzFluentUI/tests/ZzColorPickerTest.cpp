#include <cstring>
#include <memory>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QTranslator>
#include <QtCore/QPointer>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QAbstractSlider>
#include <QtWidgets/QComboBox>

#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

/** @brief 为颜色选择器色板无障碍名称提供确定翻译。 */
class ZzColorPickerTranslator final : public QTranslator
{
public:
    /** @brief 声明测试翻译器包含可安装的内存翻译。 */
    [[nodiscard]] bool isEmpty() const override
    {
        return false;
    }

    /** @brief 翻译色板名称，其余文本保持原文。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (sourceText != nullptr
            && std::strcmp(sourceText, "颜色色板") == 0) {
            return QStringLiteral("Translated color palette");
        }
        return {};
    }
};

/** @brief 验证颜色选择器唯一值同步、model/view 和对象预算。 */
class ZzColorPickerTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 按 objectName 返回固定色板视图。 */
    static QListView *paletteView(ZzFluentUI::ZzColorPicker *picker)
    {
        auto *view = picker->findChild<QListView *>(
            QStringLiteral("zzColorPaletteView"));
        Q_ASSERT(view != nullptr);
        return view;
    }

    /** @brief 按 objectName 返回固定数值编辑器。 */
    static ZzFluentUI::ZzSpinBox *spinBox(
        ZzFluentUI::ZzColorPicker *picker,
        const QString &name)
    {
        auto *editor = picker->findChild<ZzFluentUI::ZzSpinBox *>(name);
        Q_ASSERT(editor != nullptr);
        return editor;
    }

    /** @brief 按 objectName 返回固定十六进制编辑器。 */
    static QLineEdit *hexEditor(ZzFluentUI::ZzColorPicker *picker)
    {
        auto *editor = picker->findChild<QLineEdit *>(
            QStringLiteral("zzHexColorEditor"));
        Q_ASSERT(editor != nullptr);
        return editor;
    }

    /** @brief 创建应用级 Fluent style 供三主题绘制测试使用。 */
    static std::unique_ptr<ZzFluentUI::ZzFluentStyle> createStyle(
        ZzFluentUI::ZzThemeController *controller)
    {
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        Q_ASSERT(fusion != nullptr);
        return std::make_unique<ZzFluentUI::ZzFluentStyle>(
            controller,
            fusion.release());
    }

private Q_SLOTS:
    /** @brief 通过真实 HEX focus-out 覆盖重装配和不同绘制部件点击。 */
    void pendingHexFocusChangeMayDeletePicker_data()
    {
        QTest::addColumn<QString>("action");
        QTest::newRow("appearance") << QString("appearance");
        QTest::newRow("slider") << QString("zzColorChannelSlider0");
        QTest::newRow("shade") << QString("zzColorShadeStrip");
        QTest::newRow("spectrum-page") << QString("zzColorSpectrum");
    }

    /** @brief Qt focusOut/setParent 调用栈内同步删除编辑器导致崩溃时失败。 */
    void pendingHexFocusChangeMayDeletePicker()
    {
        QFETCH(QString, action);
        QPointer<ZzFluentUI::ZzColorPicker> picker = new ZzFluentUI::ZzColorPicker;
        if (action != "appearance") {
            picker->setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
            picker->findChild<QTabBar *>("zzColorPickerTabs")->setCurrentIndex(2);
        }
        picker->resize(360, 480);
        picker->show();
        picker->activateWindow();
        QCoreApplication::processEvents();
        auto *hex = hexEditor(picker);
        hex->setFocus();
        hex->selectAll();
        QTest::keyClicks(hex, "#123456");
        QVERIFY(hex->hasFocus());
        QObject::connect(picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
                         this, [picker] { delete picker.data(); });
        if (action == "appearance") {
            picker->setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
        } else {
            if (action == "zzColorSpectrum") {
                picker->findChild<QTabBar *>("zzColorPickerTabs")->setCurrentIndex(0);
            }
            if (picker) {
                auto *target = picker->findChild<QWidget *>(action);
                QVERIFY(target);
                QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, target->rect().center());
            }
        }
        QCoreApplication::processEvents();
        QVERIFY(picker.isNull());
    }

    /** @brief 延迟 HEX 通知被后续滑条编辑覆盖或逆序发出时失败。 */
    void focusOutNotificationsPreserveHexThenSliderChronology()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
        picker.findChild<QTabBar *>("zzColorPickerTabs")->setCurrentIndex(2);
        picker.show();
        picker.activateWindow();
        QCoreApplication::processEvents();
        auto *hex = hexEditor(&picker);
        hex->setFocus();
        hex->selectAll();
        QTest::keyClicks(hex, "#123456");
        QSignalSpy spy(&picker, &ZzFluentUI::ZzColorPicker::currentColorChanged);
        auto *red = picker.findChild<QWidget *>("zzColorChannelSlider0");
        QTest::mousePress(red, Qt::LeftButton, Qt::NoModifier, QPoint(red->width() - 1, red->height() / 2));
        QCOMPARE(picker.currentColor(), QColor(255, 52, 86));
        QCoreApplication::processEvents();
        QCOMPARE(spy.size(), 2);
        QCOMPARE(spy.at(0).at(0).value<QColor>(), QColor(18, 52, 86));
        QCOMPARE(spy.at(1).at(0).value<QColor>(), QColor(255, 52, 86));
    }

    /** @brief 清焦点后同步刷新待通知颜色，避免在对话框完成操作后才发预览。 */
    void commitPendingEditsFlushesBeforeDialogLikeCompletion()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.show();
        picker.activateWindow();
        QCoreApplication::processEvents();
        auto *hex = hexEditor(&picker);
        hex->setFocus();
        hex->selectAll();
        QTest::keyClicks(hex, "#123456");
        QStringList events;
        QObject::connect(&picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
                         this, [&events](const QColor &color) { events.append(color.name()); });
        hex->clearFocus();
        QCOMPARE(picker.currentColor(), QColor("#123456"));
        QVERIFY(events.isEmpty());
        picker.commitPendingEdits();
        events.append("selected");
        QCOMPARE(events, QStringList({"#123456", "selected"}));
        QCoreApplication::processEvents();
        QCOMPARE(events, QStringList({"#123456", "selected"}));
    }

    /** @brief 安全冲刷延迟通知时同步删除控件后继续访问状态时失败。 */
    void commitPendingEditsMayDeletePicker()
    {
        QPointer<ZzFluentUI::ZzColorPicker> picker = new ZzFluentUI::ZzColorPicker;
        picker->show();
        picker->activateWindow();
        QCoreApplication::processEvents();
        auto *hex = hexEditor(picker);
        hex->setFocus();
        hex->selectAll();
        QTest::keyClicks(hex, "#123456");
        QObject::connect(picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
                         this, [picker] { delete picker.data(); });
        hex->clearFocus();
        QVERIFY(picker);
        picker->commitPendingEdits();
        QVERIFY(picker.isNull());
        QCoreApplication::processEvents();
    }

    /** @brief 独立覆盖数值透明度、通道透明滑条和色谱透明滑条。 */
    void hsvAlphaEditingPreservesExactRgb_data()
    {
        QTest::addColumn<QString>("editorName");
        QTest::addColumn<int>("page");
        QTest::newRow("alpha-number") << QString("zzAlphaSpinBox") << 2;
        QTest::newRow("alpha-channel-slider") << QString("zzColorChannelSlider3") << 2;
        QTest::newRow("alpha-spectrum-slider") << QString("zzColorAlphaSlider") << 0;
    }

    /** @brief HSV 透明度路径从取整数值重建 RGB 导致颜色漂移时失败。 */
    void hsvAlphaEditingPreservesExactRgb()
    {
        QFETCH(QString, editorName);
        QFETCH(int, page);
        ZzFluentUI::ZzColorPicker picker;
        picker.setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
        picker.setColorRepresentation(ZzFluentUI::ZzColorPicker::Hsva);
        picker.setAlphaEnabled(true);
        picker.setCurrentColor(QColor(18, 52, 86, 40));
        picker.findChild<QTabBar *>("zzColorPickerTabs")->setCurrentIndex(page);
        picker.show();
        QCoreApplication::processEvents();
        auto *editor = picker.findChild<QWidget *>(editorName);
        QVERIFY(editor);
        QVERIFY(editor->isVisible());
        QSignalSpy spy(&picker, &ZzFluentUI::ZzColorPicker::currentColorChanged);
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_Up);
        QCOMPARE(picker.currentColor(), QColor(18, 52, 86, 41));
        QCOMPARE(spy.size(), 1);
    }

    /** @brief 手工核对单通道 HSV 编辑的期望 RGBA，未编辑分量保持全精度。 */
    void hsvChannelEditingPreservesUntouchedComponents_data()
    {
        QTest::addColumn<QString>("editorName");
        QTest::addColumn<int>("editedValue");
        QTest::addColumn<QColor>("expected");
        QTest::newRow("hue") << QString("zzRedSpinBox") << 180 << QColor(18, 86, 86, 40);
        QTest::newRow("saturation") << QString("zzGreenSpinBox") << 0 << QColor(86, 86, 86, 40);
        QTest::newRow("value") << QString("zzBlueSpinBox") << 100 << QColor(53, 154, 255, 40);
    }

    /** @brief 编辑一个 HSV 分量同时量化其余分量时失败。 */
    void hsvChannelEditingPreservesUntouchedComponents()
    {
        QFETCH(QString, editorName);
        QFETCH(int, editedValue);
        QFETCH(QColor, expected);
        ZzFluentUI::ZzColorPicker picker;
        picker.setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
        picker.setColorRepresentation(ZzFluentUI::ZzColorPicker::Hsva);
        picker.setAlphaEnabled(true);
        picker.setCurrentColor(QColor(18, 52, 86, 40));
        spinBox(&picker, editorName)->setValue(editedValue);
        QCOMPARE(picker.currentColor(), expected);
    }

    /** @brief 标准 48 色板在 360px 选择器内不足八列或出现截断时失败。 */
    void fluentPaletteFitsEightColumns()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setProperty("appearance", 1);
        QList<QColor> colors;
        for (int index = 0; index < 48; ++index) {
            colors.append(QColor(index * 5, 100, 150));
        }
        picker.setPaletteColors(colors);
        picker.setFixedWidth(360);
        picker.findChild<QTabBar *>("zzColorPickerTabs")->setCurrentIndex(1);
        picker.show();
        QCoreApplication::processEvents();
        auto *view = paletteView(&picker);
        for (int width : {360, 300, 360, picker.minimumSizeHint().width()}) {
            picker.setFixedWidth(width);
            QCoreApplication::processEvents();
            QCOMPARE(view->visualRect(view->model()->index(7, 0)).top(),
                     view->visualRect(view->model()->index(0, 0)).top());
            QVERIFY(view->viewport()->rect().contains(view->visualRect(view->model()->index(47, 0))));
        }
        for (int count : {49, 256}) {
            colors.clear();
            for (int index = 0; index < count; ++index) {
                colors.append(QColor(index, 100, 150));
            }
            picker.setPaletteColors(colors);
            QCoreApplication::processEvents();
            const QModelIndex last = view->model()->index(count - 1, 0);
            view->scrollTo(last);
            QCoreApplication::processEvents();
            QVERIFY(view->viewport()->rect().contains(view->visualRect(last)));
        }
    }

    /** @brief 数值滑条忽略 RTL、端点限制或禁用状态时失败。 */
    void fluentGradientPointerKeyboardAndDisabledState()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setProperty("appearance", 1);
        picker.setAlphaEnabled(true);
        picker.resize(360, 440);
        picker.show();
        auto *tabs = picker.findChild<QTabBar *>("zzColorPickerTabs");
        QVERIFY(tabs);
        QVERIFY(picker.findChild<QAbstractSlider *>("zzColorAlphaSlider")->isVisible());
        tabs->setCurrentIndex(2);
        QCoreApplication::processEvents();
        auto *red = picker.findChild<QAbstractSlider *>("zzColorChannelSlider0");
        QVERIFY(red);
        picker.setCurrentColor(QColor(10, 20, 30, 91));
        QTest::mouseClick(red, Qt::LeftButton, Qt::NoModifier, QPoint(red->width() - 1, 12));
        QCOMPARE(picker.currentColor(), QColor(255, 20, 30, 91));
        picker.setLayoutDirection(Qt::RightToLeft);
        QTest::mouseClick(red, Qt::LeftButton, Qt::NoModifier, QPoint(red->width() - 1, 12));
        QCOMPARE(picker.currentColor(), QColor(0, 20, 30, 91));
        auto *alpha = picker.findChild<QAbstractSlider *>("zzColorChannelSlider3");
        QVERIFY(alpha);
        QTest::keyClick(alpha, Qt::Key_Up);
        QCOMPARE(picker.currentColor().alpha(), 92);
        picker.setEnabled(false);
        QTest::keyClick(alpha, Qt::Key_Up);
        QCOMPARE(picker.currentColor().alpha(), 92);
    }

    /** @brief 圆形色谱越界拖动、RTL 选点或色板点击不提交时失败。 */
    void fluentRingDragAndPaletteSelection()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setProperty("appearance", 1);
        picker.setProperty("colorSpectrumShape", 1);
        picker.resize(360, 440);
        picker.setCurrentColor(QColor(0, 255, 0, 91));
        picker.show();
        QCoreApplication::processEvents();
        auto *spectrum = picker.findChild<QWidget *>("zzColorSpectrum");
        QVERIFY(spectrum);
        QTest::mousePress(spectrum, Qt::LeftButton, Qt::NoModifier, spectrum->rect().center());
        QMouseEvent move(QEvent::MouseMove, QPointF(spectrum->width() + 400, spectrum->height() / 2.0),
                         QPointF(0, 0), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(spectrum, &move);
        QTest::mouseRelease(spectrum, Qt::LeftButton);
        QVERIFY(picker.currentColor().red() > 250);
        QVERIFY(picker.currentColor().green() < 3);
        QCOMPARE(picker.currentColor().alpha(), 91);
        picker.setLayoutDirection(Qt::RightToLeft);
        QTest::mouseClick(spectrum, Qt::LeftButton, Qt::NoModifier,
                          QPoint(0, spectrum->height() / 2));
        QVERIFY(picker.currentColor().red() > 250);
        picker.setPaletteColors({QColor("#123456"), QColor("#abcdef")});
        picker.findChild<QTabBar *>("zzColorPickerTabs")->setCurrentIndex(1);
        QCoreApplication::processEvents();
        auto *view = paletteView(&picker);
        const QRect swatch = view->visualRect(view->model()->index(1, 0));
        QVERIFY(!swatch.isEmpty());
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, swatch.center());
        QCOMPARE(picker.currentColor(), QColor("#abcdef"));
    }

    /** @brief 编辑提交信号同步删除控件后继续访问私有状态时失败。 */
    void colorEditMaySynchronouslyDeletePicker()
    {
        QPointer<ZzFluentUI::ZzColorPicker> picker = new ZzFluentUI::ZzColorPicker;
        auto *hex = hexEditor(picker);
        QObject::connect(picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
                         picker, [picker] { delete picker.data(); });
        hex->setText("#123456");
        QMetaObject::invokeMethod(hex, "editingFinished");
        QVERIFY(picker.isNull());
    }

    /** @brief 重入设置颜色导致外层信号借用被改写状态时失败。 */
    void colorSignalKeepsSnapshotAcrossReentrantSetters()
    {
        ZzFluentUI::ZzColorPicker picker;
        QList<QColor> observed;
        QObject::connect(&picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
                         &picker, [&picker](const QColor &color) {
            if (color == QColor("#123456")) {
                picker.setCurrentColor(QColor("#abcdef"));
            }
        });
        QObject::connect(&picker, &ZzFluentUI::ZzColorPicker::currentColorChanged,
                         this, [&observed](const QColor &color) { observed.append(color); });
        picker.setCurrentColor(QColor("#123456"));
        QCOMPARE(observed, QList<QColor>({QColor("#abcdef"), QColor("#123456")}));
        QCOMPARE(picker.currentColor(), QColor("#abcdef"));
    }

    /** @brief 缺失三页装配、色谱输入或键盘修改时失败。 */
    void fluentSpectrumSupportsPointerAndKeyboard()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setProperty("appearance", 1);
        picker.resize(360, 480);
        picker.show();
        QCoreApplication::processEvents();
        auto *tabs = picker.findChild<QTabBar *>("zzColorPickerTabs");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 3);
        auto *spectrum = picker.findChild<QWidget *>("zzColorSpectrum");
        QVERIFY(spectrum);
        picker.setCurrentColor(QColor(255, 0, 0, 91));
        QTest::mouseClick(spectrum, Qt::LeftButton, Qt::NoModifier,
                          QPoint(spectrum->width() / 3, 2));
        QVERIFY(picker.currentColor().green() > 240);
        QCOMPARE(picker.currentColor().alpha(), 91);
        const QColor before = picker.currentColor();
        spectrum->setFocus();
        QTest::keyClick(spectrum, Qt::Key_Right);
        QVERIFY(picker.currentColor() != before);
        picker.setEnabled(false);
        const QColor disabled = picker.currentColor();
        QTest::keyClick(spectrum, Qt::Key_Right);
        QCOMPARE(picker.currentColor(), disabled);
    }

    /** @brief 页隐藏未回退、外观切换丢值或重复分配对象时失败。 */
    void fluentVisibilityAndAppearancePreserveState()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setProperty("appearance", 1);
        picker.show();
        QCoreApplication::processEvents();
        auto *tabs = picker.findChild<QTabBar *>("zzColorPickerTabs");
        QVERIFY(tabs);
        const QList<QColor> custom{QColor("#123456"), QColor("#abcdef")};
        picker.setPaletteColors(custom);
        picker.setCurrentColor(QColor(10, 20, 30, 41));
        const auto count = picker.findChildren<QObject *>().size();
        for (int iteration = 0; iteration < 20; ++iteration) {
            picker.setProperty("appearance", iteration % 2);
        }
        picker.setProperty("appearance", 1);
        QCOMPARE(picker.findChildren<QObject *>().size(), count);
        QCOMPARE(picker.paletteColors(), custom);
        QCOMPARE(picker.currentColor(), QColor(10, 20, 30, 41));
        picker.setProperty("colorSpectrumVisible", false);
        QCOMPARE(tabs->currentIndex(), 1);
        picker.setProperty("colorPaletteVisible", false);
        QCOMPARE(tabs->currentIndex(), 2);
        picker.setProperty("colorSliderVisible", false);
        QVERIFY(tabs->isHidden());
        QVERIFY(picker.findChild<QWidget *>("zzColorSpectrum")->isVisible() == false);
    }

    /** @brief 全部页签隐藏后恢复，后续无关设置不能丢失可点击的页签几何。 */
    void restoringAllFluentPagesRestoresTabGeometry()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setAppearance(ZzFluentUI::ZzColorPicker::Fluent);
        picker.resize(360, 400);
        picker.show();
        QCoreApplication::processEvents();
        auto *tabs = picker.findChild<QTabBar *>("zzColorPickerTabs");
        QVERIFY(tabs);
        picker.setColorSpectrumVisible(false);
        picker.setColorPaletteVisible(false);
        picker.setColorSliderVisible(false);
        picker.setColorSpectrumVisible(true);
        picker.setColorPaletteVisible(true);
        picker.setColorSliderVisible(true);
        // Unrelated visibility updates must preserve a pending QTabBar relayout.
        picker.setColorPreviewVisible(false);
        picker.setColorPreviewVisible(true);
        for (int index = 0; index < 3; ++index) {
            QVERIFY(tabs->isTabVisible(index));
            QVERIFY2(!tabs->tabRect(index).isEmpty(), qPrintable(QString::number(index)));
        }
    }

    /** @brief HSV 灰黑色编辑丢 hue、透明滑条无联动或非法 HEX 不恢复时失败。 */
    void fluentChannelsPreserveHueAndAlpha()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setProperty("appearance", 1);
        picker.setAlphaEnabled(true);
        picker.show();
        auto *tabs = picker.findChild<QTabBar *>("zzColorPickerTabs");
        QVERIFY(tabs);
        tabs->setCurrentIndex(2);
        picker.setProperty("colorRepresentation", 1);
        auto *hue = spinBox(&picker, "zzRedSpinBox");
        auto *saturation = spinBox(&picker, "zzGreenSpinBox");
        auto *value = spinBox(&picker, "zzBlueSpinBox");
        QCOMPARE(hue->value(), 206);
        picker.setCurrentColor(QColor(0, 0, 255, 91));
        QCOMPARE(hue->value(), 240);
        saturation->setValue(0);
        QCOMPARE(hue->value(), 240);
        value->setValue(0);
        saturation->setValue(100);
        value->setValue(100);
        QCOMPARE(picker.currentColor(), QColor(0, 0, 255, 91));
        auto *alpha = picker.findChild<QAbstractSlider *>("zzColorAlphaSlider");
        QVERIFY(alpha);
        alpha->setValue(127);
        QCOMPARE(picker.currentColor().alpha(), 127);
        hexEditor(&picker)->setText("#broken");
        QMetaObject::invokeMethod(hexEditor(&picker), "editingFinished");
        QCOMPARE(hexEditor(&picker)->text(), QString("#7F0000FF"));
    }

    void exposesStableDefaultsAndIdempotentSetters()
    {
        ZzFluentUI::ZzColorPicker picker;
        QSignalSpy colorSpy(
            &picker,
            &ZzFluentUI::ZzColorPicker::currentColorChanged);
        QSignalSpy alphaSpy(
            &picker,
            &ZzFluentUI::ZzColorPicker::alphaEnabledChanged);
        QSignalSpy paletteSpy(
            &picker,
            &ZzFluentUI::ZzColorPicker::paletteColorsChanged);

        QCOMPARE(picker.currentColor(), QColor(QStringLiteral("#0078d4")));
        QVERIFY(!picker.isAlphaEnabled());
        QCOMPARE(picker.paletteColorCount(), 24);
        QCOMPARE(picker.paletteColors().size(), 24);

        picker.setCurrentColor(QColor());
        picker.setCurrentColor(picker.currentColor());
        picker.setAlphaEnabled(false);
        picker.resetPaletteColors();
        QCOMPARE(colorSpy.size(), 0);
        QCOMPARE(alphaSpy.size(), 0);
        QCOMPARE(paletteSpy.size(), 0);

        picker.setCurrentColor(QColor::fromRgba(qRgba(10, 20, 30, 40)));
        QCOMPARE(picker.currentColor().rgba(), qRgba(10, 20, 30, 40));
        QCOMPARE(colorSpy.size(), 1);
        picker.setAlphaEnabled(true);
        QCOMPARE(alphaSpy.size(), 1);
        QCOMPARE(picker.currentColor().alpha(), 40);
    }

    void normalizesPaletteAndEnforcesBound()
    {
        ZzFluentUI::ZzColorPicker picker;
        QSignalSpy paletteSpy(
            &picker,
            &ZzFluentUI::ZzColorPicker::paletteColorsChanged);
        picker.setPaletteColors({
            QColor(),
            QColor(QStringLiteral("#102030")),
            QColor(QStringLiteral("#102030")),
            QColor::fromRgba(qRgba(16, 32, 48, 128))});
        QCOMPARE(picker.paletteColorCount(), 2);
        QCOMPARE(paletteSpy.size(), 1);
        QCOMPARE(
            picker.paletteColors().at(1).rgba(),
            qRgba(16, 32, 48, 128));

        QList<QColor> oversized;
        oversized.reserve(300);
        for (int index = 0; index < 300; ++index) {
            oversized.append(QColor::fromRgb(
                index % 256,
                index / 256,
                17,
                255));
        }
        picker.setPaletteColors(oversized);
        QCOMPARE(picker.paletteColorCount(), 256);
        QCOMPARE(paletteSpy.size(), 2);

        picker.setPaletteColors({});
        QCOMPARE(picker.paletteColorCount(), 0);
        picker.resetPaletteColors();
        QCOMPARE(picker.paletteColorCount(), 24);
        QCOMPARE(paletteSpy.size(), 4);
    }

    void singleClickAndKeyboardUseModelColor()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.resize(520, 360);
        picker.show();
        QCoreApplication::processEvents();
        QListView *view = paletteView(&picker);
        QAbstractItemModel *model = view->model();
        QVERIFY(model != nullptr);
        const QModelIndex second = model->index(1, 0);
        const QColor secondColor = second.data(Qt::UserRole + 1).value<QColor>();
        QVERIFY(secondColor.isValid());

        QSignalSpy colorSpy(
            &picker,
            &ZzFluentUI::ZzColorPicker::currentColorChanged);
        const QRect secondRect = view->visualRect(second);
        QVERIFY(!secondRect.isEmpty());
        QTest::mouseClick(
            view->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            secondRect.center());
        QCOMPARE(picker.currentColor(), secondColor);
        QCOMPARE(colorSpy.size(), 1);

        view->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(view, Qt::Key_Right);
        QCoreApplication::processEvents();
        const QModelIndex current = view->currentIndex();
        QVERIFY(current.isValid());
        QCOMPARE(
            picker.currentColor(),
            current.data(Qt::UserRole + 1).value<QColor>());
        const qsizetype signalsBeforeEnter = colorSpy.size();
        QTest::keyClick(view, Qt::Key_Enter);
        QCoreApplication::processEvents();
        QCOMPARE(colorSpy.size(), signalsBeforeEnter);
    }

    void synchronizesChannelsHexAndAlphaWithoutLosingValue()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.setCurrentColor(QColor::fromRgba(qRgba(1, 2, 3, 64)));
        auto *red = spinBox(&picker, QStringLiteral("zzRedSpinBox"));
        auto *green = spinBox(&picker, QStringLiteral("zzGreenSpinBox"));
        auto *blue = spinBox(&picker, QStringLiteral("zzBlueSpinBox"));
        auto *alpha = spinBox(&picker, QStringLiteral("zzAlphaSpinBox"));
        auto *hex = hexEditor(&picker);

        QCOMPARE(red->value(), 1);
        QCOMPARE(green->value(), 2);
        QCOMPARE(blue->value(), 3);
        QCOMPARE(alpha->value(), 64);
        QCOMPARE(hex->text(), QStringLiteral("#010203"));
        QVERIFY(alpha->isHidden());

        red->setValue(16);
        green->setValue(32);
        blue->setValue(48);
        QCOMPARE(picker.currentColor().rgba(), qRgba(16, 32, 48, 64));
        QCOMPARE(hex->text(), QStringLiteral("#102030"));

        picker.setAlphaEnabled(true);
        QVERIFY(!alpha->isHidden());
        QCOMPARE(hex->text(), QStringLiteral("#40102030"));
        alpha->setValue(128);
        QCOMPARE(picker.currentColor().rgba(), qRgba(16, 32, 48, 128));
        QCOMPARE(hex->text(), QStringLiteral("#80102030"));

        hex->setText(QStringLiteral("#7F405060"));
        QVERIFY(QMetaObject::invokeMethod(hex, "editingFinished"));
        QCOMPARE(picker.currentColor().rgba(), qRgba(64, 80, 96, 127));
        QCOMPARE(red->value(), 64);
        QCOMPARE(green->value(), 80);
        QCOMPARE(blue->value(), 96);
        QCOMPARE(alpha->value(), 127);

        picker.setAlphaEnabled(false);
        hex->setText(QStringLiteral("#112233"));
        QVERIFY(QMetaObject::invokeMethod(hex, "editingFinished"));
        QCOMPARE(picker.currentColor().rgba(), qRgba(17, 34, 51, 127));
        hex->setText(QStringLiteral("#bad"));
        QVERIFY(QMetaObject::invokeMethod(hex, "editingFinished"));
        QCOMPARE(hex->text(), QStringLiteral("#112233"));
        QCOMPARE(picker.currentColor().alpha(), 127);
    }

    void customColorClearsDerivedPaletteSelection()
    {
        ZzFluentUI::ZzColorPicker picker;
        QListView *view = paletteView(&picker);
        QVERIFY(view->currentIndex().isValid());
        picker.setCurrentColor(QColor(QStringLiteral("#123456")));
        QVERIFY(!view->currentIndex().isValid());

        picker.setPaletteColors({QColor(QStringLiteral("#123456"))});
        QVERIFY(view->currentIndex().isValid());
        QCOMPARE(view->currentIndex().row(), 0);
    }

    void refreshesLanguageThemeAndRtlGeometry()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = createStyle(&controller);
        ZzFluentUI::ZzColorPicker picker;
        picker.setStyle(style.get());
        picker.resize(520, 360);
        picker.setLayoutDirection(Qt::RightToLeft);
        picker.show();
        QCoreApplication::processEvents();

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            QCoreApplication::processEvents();
            QImage image(
                picker.size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            picker.render(&painter);
            painter.end();
            QVERIFY(!image.isNull());
            QVERIFY(image.pixelColor(image.rect().center()).alpha() > 0);
        }

        for (QWidget *child : picker.findChildren<QWidget *>(
                 QString(), Qt::FindDirectChildrenOnly)) {
            QVERIFY(
                child->isHidden()
                || picker.rect().contains(child->geometry()));
        }

        QListView *view = paletteView(&picker);
        QCOMPARE(view->accessibleName(), QStringLiteral("颜色色板"));
        ZzColorPickerTranslator translator;
        QVERIFY(!translator.isEmpty());
        QVERIFY(QCoreApplication::installTranslator(&translator));
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&picker, &languageChange);
        QCOMPARE(
            view->accessibleName(),
            QStringLiteral("Translated color palette"));
        QCoreApplication::removeTranslator(&translator);
    }

    void exposesNativeListItemsAndSpinBoxesToAccessibility()
    {
        ZzFluentUI::ZzColorPicker picker;
        picker.resize(520, 360);
        picker.show();
        QCoreApplication::processEvents();
        QListView *view = paletteView(&picker);
        QAccessibleInterface *listInterface =
            QAccessible::queryAccessibleInterface(view);
        QVERIFY(listInterface != nullptr);
        QCOMPARE(listInterface->role(), QAccessible::List);
        QVERIFY(listInterface->childCount() > 0);
        QAccessibleInterface *itemInterface = listInterface->child(0);
        QVERIFY(itemInterface != nullptr);
        QCOMPARE(itemInterface->role(), QAccessible::ListItem);
        QVERIFY(!itemInterface->text(QAccessible::Name).isEmpty());

        auto *red = spinBox(&picker, QStringLiteral("zzRedSpinBox"));
        QAccessibleInterface *spinInterface =
            QAccessible::queryAccessibleInterface(red);
        QVERIFY(spinInterface != nullptr);
        QCOMPARE(spinInterface->role(), QAccessible::SpinBox);
        QCOMPARE(
            spinInterface->text(QAccessible::Name),
            QStringLiteral("红色"));
    }

    void repeatedUpdatesKeepFixedObjectGraph()
    {
        ZzFluentUI::ZzColorPicker picker;
        QListView *const view = paletteView(&picker);
        QAbstractItemModel *const model = view->model();
        QAbstractItemDelegate *const delegate = view->itemDelegate();
        QWidget *const preview = picker.findChild<QWidget *>(
            QStringLiteral("zzColorPreview"));
        QLineEdit *const hex = hexEditor(&picker);
        const qsizetype objectCount =
            picker.findChildren<QObject *>().size();
        const QList<QColor> custom{
            QColor(QStringLiteral("#123456")),
            QColor(QStringLiteral("#abcdef"))};

        for (int iteration = 0; iteration < 1000; ++iteration) {
            picker.setCurrentColor(QColor::fromRgb(
                iteration % 256,
                (iteration / 2) % 256,
                (iteration / 3) % 256,
                (iteration / 5) % 256));
            if (iteration % 2 == 0) {
                picker.setPaletteColors(custom);
            } else {
                picker.resetPaletteColors();
            }
        }

        QCOMPARE(paletteView(&picker), view);
        QCOMPARE(view->model(), model);
        QCOMPARE(view->itemDelegate(), delegate);
        QCOMPARE(
            picker.findChild<QWidget *>(
                QStringLiteral("zzColorPreview")),
            preview);
        QCOMPARE(hexEditor(&picker), hex);
        QCOMPARE(
            picker.findChildren<QObject *>().size(),
            objectCount);
    }
};

QTEST_MAIN(ZzColorPickerTest)

#include "ZzColorPickerTest.moc"
