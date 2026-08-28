#include <cstring>
#include <memory>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QTranslator>
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
