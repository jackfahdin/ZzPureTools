#include <QtCore/QAbstractAnimation>
#include <QtCore/QLocale>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStyleOption>

#include <memory>

#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzThemeController.h>

namespace {

/** @brief 判断矩形为空或完全位于给定边界内。 */
bool zzContainedOrEmpty(const QRect &bounds, const QRect &candidate)
{
    return candidate.isEmpty() || bounds.contains(candidate);
}

/** @brief 判断图像是否包含任何非透明绘制结果。 */
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

/** @brief 判断指定矩形内是否存在区别于背板的绘制像素。 */
bool zzContainsNonBackgroundPixel(
    const QImage &image,
    const QColor &background,
    const QRect &region)
{
    const QRect clipped = region.intersected(image.rect());
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (image.pixelColor(x, y) != background) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 使用指定样式渲染数值输入控件。 */
QImage zzRenderSpinBox(QWidget *widget, QStyle *style)
{
    widget->setStyle(style);
    widget->setPalette(style->standardPalette());
    widget->resize(132, 36);
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    return image;
}

} // namespace

/** @brief 验证 Fluent 数值输入框的原生语义、几何和对象预算。 */
class ZzSpinBoxControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesLightweightDefaults()
    {
        ZzFluentUI::ZzSpinBox integer;
        ZzFluentUI::ZzDoubleSpinBox floating;

        QCOMPARE(
            integer.buttonSymbols(),
            QAbstractSpinBox::PlusMinus);
        QCOMPARE(
            floating.buttonSymbols(),
            QAbstractSpinBox::PlusMinus);
        QCOMPARE(integer.findChildren<QAbstractAnimation *>().size(), 0);
        QCOMPARE(floating.findChildren<QAbstractAnimation *>().size(), 0);
        QCOMPARE(integer.findChildren<QTimer *>().size(), 0);
        QCOMPARE(floating.findChildren<QTimer *>().size(), 0);
    }

    void preservesIntegerRangeAndSignalSemantics()
    {
        ZzFluentUI::ZzSpinBox spinBox;
        spinBox.setRange(-10, 10);
        spinBox.setSingleStep(2);
        QSignalSpy valueSpy(&spinBox, &QSpinBox::valueChanged);

        spinBox.setValue(4);
        QCOMPARE(spinBox.value(), 4);
        QCOMPARE(valueSpy.size(), 1);
        spinBox.stepUp();
        QCOMPARE(spinBox.value(), 6);
        QCOMPARE(valueSpy.size(), 2);
        spinBox.stepDown();
        QCOMPARE(spinBox.value(), 4);
        QCOMPARE(valueSpy.size(), 3);

        spinBox.setPrefix(QStringLiteral("0x"));
        spinBox.setSuffix(QStringLiteral(" u"));
        spinBox.setDisplayIntegerBase(16);
        QCOMPARE(spinBox.prefix(), QStringLiteral("0x"));
        QCOMPARE(spinBox.suffix(), QStringLiteral(" u"));
        QCOMPARE(spinBox.displayIntegerBase(), 16);

        spinBox.setWrapping(true);
        spinBox.setValue(spinBox.maximum());
        spinBox.stepUp();
        QCOMPARE(spinBox.value(), spinBox.minimum());
    }

    void preservesFloatingLocaleEditing()
    {
        ZzFluentUI::ZzDoubleSpinBox spinBox;
        spinBox.setLocale(QLocale(QLocale::German, QLocale::Germany));
        spinBox.setRange(-10.0, 10.0);
        spinBox.setDecimals(2);
        spinBox.setSingleStep(0.25);
        spinBox.show();
        QVERIFY(QTest::qWaitForWindowExposed(&spinBox));

        auto *editor = spinBox.findChild<QLineEdit *>();
        QVERIFY(editor != nullptr);
        editor->selectAll();
        QTest::keyClicks(editor, QStringLiteral("1,25"));
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(spinBox.value(), 1.25);
        QCOMPARE(spinBox.decimals(), 2);

        QSignalSpy valueSpy(&spinBox, &QDoubleSpinBox::valueChanged);
        spinBox.stepUp();
        QCOMPARE(spinBox.value(), 1.5);
        QCOMPARE(valueSpy.size(), 1);
    }

    void preservesKeyboardStepping()
    {
        ZzFluentUI::ZzSpinBox spinBox;
        spinBox.setRange(0, 100);
        spinBox.setValue(50);
        spinBox.show();
        QVERIFY(QTest::qWaitForWindowExposed(&spinBox));
        spinBox.setFocus();

        QTest::keyClick(&spinBox, Qt::Key_Up);
        QCOMPARE(spinBox.value(), 51);
        QTest::keyClick(&spinBox, Qt::Key_Down);
        QCOMPARE(spinBox.value(), 50);
        QTest::keyClick(&spinBox, Qt::Key_PageUp);
        QCOMPARE(spinBox.value(), 60);
        QTest::keyClick(&spinBox, Qt::Key_PageDown);
        QCOMPARE(spinBox.value(), 50);
    }

    void providesStableGeometry_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::addColumn<QAbstractSpinBox::ButtonSymbols>("symbols");
        QTest::addColumn<QSize>("size");

        for (const Qt::LayoutDirection direction : {
                 Qt::LeftToRight,
                 Qt::RightToLeft}) {
            for (const QAbstractSpinBox::ButtonSymbols symbols : {
                     QAbstractSpinBox::UpDownArrows,
                     QAbstractSpinBox::PlusMinus,
                     QAbstractSpinBox::NoButtons}) {
                const QByteArray prefix = direction == Qt::LeftToRight
                    ? QByteArray("ltr")
                    : QByteArray("rtl");
                const QByteArray symbolName = QByteArray::number(
                    static_cast<int>(symbols));
                QTest::newRow(
                    (prefix + '-' + symbolName + "-normal").constData())
                    << direction << symbols << QSize(120, 33);
                QTest::newRow(
                    (prefix + '-' + symbolName + "-tiny").constData())
                    << direction << symbols << QSize(18, 9);
            }
        }
    }

    void providesStableGeometry()
    {
        QFETCH(Qt::LayoutDirection, direction);
        QFETCH(QAbstractSpinBox::ButtonSymbols, symbols);
        QFETCH(QSize, size);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionSpinBox option;
        option.rect = QRect(QPoint(0, 0), size);
        option.direction = direction;
        option.buttonSymbols = symbols;
        option.subControls = QStyle::SC_All;

        const QRect frame = style.subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxFrame);
        const QRect edit = style.subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxEditField);
        const QRect up = style.subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxUp);
        const QRect down = style.subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxDown);

        QCOMPARE(frame, option.rect);
        QVERIFY(zzContainedOrEmpty(option.rect, edit));
        QVERIFY(zzContainedOrEmpty(option.rect, up));
        QVERIFY(zzContainedOrEmpty(option.rect, down));
        QVERIFY(!edit.intersects(up));
        QVERIFY(!edit.intersects(down));
        QVERIFY(!up.intersects(down));
        if (symbols == QAbstractSpinBox::NoButtons) {
            QVERIFY(up.isEmpty());
            QVERIFY(down.isEmpty());
        } else {
            QVERIFY(!up.isEmpty());
            QVERIFY(!down.isEmpty());
            if (up.width() < option.rect.width()) {
                if (direction == Qt::LeftToRight) {
                    QVERIFY(up.center().x() > option.rect.center().x());
                } else {
                    QVERIFY(up.center().x() < option.rect.center().x());
                }
            }
            QCOMPARE(
                style.hitTestComplexControl(
                    QStyle::CC_SpinBox,
                    &option,
                    up.center()),
                QStyle::SC_SpinBoxUp);
            QCOMPARE(
                style.hitTestComplexControl(
                    QStyle::CC_SpinBox,
                    &option,
                    down.center()),
                QStyle::SC_SpinBoxDown);
        }
        if (!edit.isEmpty()) {
            QCOMPARE(
                style.hitTestComplexControl(
                    QStyle::CC_SpinBox,
                    &option,
                    edit.center()),
                QStyle::SC_SpinBoxEditField);
        }
    }

    void rendersStandardAndZzControls()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QSpinBox standardInteger;
        QDoubleSpinBox standardFloating;
        ZzFluentUI::ZzSpinBox fluentInteger;
        ZzFluentUI::ZzDoubleSpinBox fluentFloating;

        for (QWidget *widget : {
                 static_cast<QWidget *>(&standardInteger),
                 static_cast<QWidget *>(&standardFloating),
                 static_cast<QWidget *>(&fluentInteger),
                 static_cast<QWidget *>(&fluentFloating)}) {
            const QImage image = zzRenderSpinBox(widget, &style);
            QVERIFY(zzContainsOpaquePixel(image));
        }
        QVERIFY(style.sizeFromContents(
            QStyle::CT_SpinBox,
            nullptr,
            QSize(1, 1)).width() >= 96);
        QVERIFY(style.sizeFromContents(
            QStyle::CT_SpinBox,
            nullptr,
            QSize(1, 1)).height() >= 32);
    }

    void unifiesValueSurfaceStates()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const QSize size(132, 36);

        for (QAbstractSpinBox *spinBox : {
                 static_cast<QAbstractSpinBox *>(
                     new ZzFluentUI::ZzSpinBox),
                 static_cast<QAbstractSpinBox *>(
                     new ZzFluentUI::ZzDoubleSpinBox)}) {
            std::unique_ptr<QAbstractSpinBox> owner(spinBox);
            spinBox->resize(size);
            spinBox->setStyle(&style);
            spinBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows);

            QStyleOptionSpinBox option;
            option.initFrom(spinBox);
            option.rect = QRect(QPoint(0, 0), size);
            option.subControls = QStyle::SC_All;
            option.buttonSymbols = spinBox->buttonSymbols();
            option.stepEnabled = QAbstractSpinBox::StepUpEnabled
                | QAbstractSpinBox::StepDownEnabled;

            const QRect edit = style.subControlRect(
                QStyle::CC_SpinBox, &option,
                QStyle::SC_SpinBoxEditField);
            const QRect up = style.subControlRect(
                QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp);
            const QRect down = style.subControlRect(
                QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
            QVERIFY(!edit.intersects(up));
            QVERIFY(!edit.intersects(down));
            QVERIFY(up.width() <= 28);
            QVERIFY(down.width() <= 28);
            const auto verifyHitBoundary = [&style, &option](
                                               const QRect &rect,
                                               QStyle::SubControl expected) {
                const QList<QPoint> points{
                    rect.topLeft(),
                    rect.topRight(),
                    rect.bottomLeft(),
                    rect.bottomRight(),
                    QPoint(rect.center().x(), rect.top()),
                    QPoint(rect.center().x(), rect.bottom()),
                    QPoint(rect.left(), rect.center().y()),
                    QPoint(rect.right(), rect.center().y())};
                for (const QPoint &point : points) {
                    if (rect.contains(point)) {
                        QCOMPARE(style.hitTestComplexControl(
                                     QStyle::CC_SpinBox,
                                     &option,
                                     point),
                                 expected);
                    }
                }
            };
            verifyHitBoundary(up, QStyle::SC_SpinBoxUp);
            verifyHitBoundary(down, QStyle::SC_SpinBoxDown);

            option.direction = Qt::RightToLeft;
            const QRect rtlUp = style.subControlRect(
                QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp);
            const QRect rtlDown = style.subControlRect(
                QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown);
            QVERIFY(rtlUp.center().x() < option.rect.center().x());
            verifyHitBoundary(rtlUp, QStyle::SC_SpinBoxUp);
            verifyHitBoundary(rtlDown, QStyle::SC_SpinBoxDown);

            const QSize unfocusedHint = spinBox->sizeHint();
            QImage unfocused(size, QImage::Format_ARGB32_Premultiplied);
            const QColor background = option.palette.color(QPalette::Base);
            unfocused.fill(background);
            QPainter unfocusedPainter(&unfocused);
            option.direction = Qt::LeftToRight;
            option.state &= ~QStyle::State_HasFocus;
            style.drawComplexControl(
                QStyle::CC_SpinBox, &option, &unfocusedPainter, spinBox);
            unfocusedPainter.end();

            QImage focused(size, QImage::Format_ARGB32_Premultiplied);
            focused.fill(background);
            QPainter focusedPainter(&focused);
            option.state |= QStyle::State_HasFocus;
            style.drawComplexControl(
                QStyle::CC_SpinBox, &option, &focusedPainter, spinBox);
            focusedPainter.end();
            QVERIFY(unfocused != focused);
            QCOMPARE(spinBox->sizeHint(), unfocusedHint);

            option.subControls = QStyle::SC_SpinBoxUp
                | QStyle::SC_SpinBoxDown;
            QImage buttonsOnlyFocused(
                size, QImage::Format_ARGB32_Premultiplied);
            buttonsOnlyFocused.fill(background);
            QPainter buttonsOnlyFocusedPainter(&buttonsOnlyFocused);
            style.drawComplexControl(
                QStyle::CC_SpinBox,
                &option,
                &buttonsOnlyFocusedPainter,
                spinBox);
            buttonsOnlyFocusedPainter.end();
            option.state &= ~QStyle::State_HasFocus;
            QImage buttonsOnlyUnfocused(
                size, QImage::Format_ARGB32_Premultiplied);
            buttonsOnlyUnfocused.fill(background);
            QPainter buttonsOnlyUnfocusedPainter(&buttonsOnlyUnfocused);
            style.drawComplexControl(
                QStyle::CC_SpinBox,
                &option,
                &buttonsOnlyUnfocusedPainter,
                spinBox);
            buttonsOnlyUnfocusedPainter.end();
            QVERIFY(buttonsOnlyFocused == buttonsOnlyUnfocused);
            option.state |= QStyle::State_HasFocus;
            option.subControls = QStyle::SC_All;

            for (const ZzFluentUI::ZzThemeMode mode : {
                     ZzFluentUI::ZzThemeMode::Light,
                     ZzFluentUI::ZzThemeMode::Dark,
                     ZzFluentUI::ZzThemeMode::HighContrast}) {
                controller.setMode(mode);
                option.palette = style.standardPalette();
                QImage image(size, QImage::Format_ARGB32_Premultiplied);
                const QColor themeBackground = option.palette.color(
                    QPalette::Base);
                image.fill(themeBackground);
                QPainter painter(&image);
                style.drawComplexControl(
                    QStyle::CC_SpinBox, &option, &painter, spinBox);
                QVERIFY(zzContainsNonBackgroundPixel(
                    image,
                    themeBackground,
                    option.rect));
            }

            for (const qreal dpr : {1.0, 2.0}) {
                const QColor dprBackground = option.palette.color(
                    QPalette::Base);
                QImage image(
                    size * static_cast<int>(dpr),
                    QImage::Format_ARGB32_Premultiplied);
                image.setDevicePixelRatio(dpr);
                image.fill(dprBackground);
                QPainter painter(&image);
                style.drawComplexControl(
                    QStyle::CC_SpinBox, &option, &painter, spinBox);
                painter.end();
                QVERIFY(zzContainsNonBackgroundPixel(
                    image,
                    dprBackground,
                    image.rect()));
                QCOMPARE(spinBox->sizeHint(), unfocusedHint);
            }
        }
    }

    void preservesAccessibleValueInterface()
    {
        ZzFluentUI::ZzSpinBox spinBox;
        spinBox.setRange(-20, 80);
        spinBox.setValue(24);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&spinBox);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::SpinBox);
        QAccessibleValueInterface *valueInterface = interface->valueInterface();
        QVERIFY(valueInterface != nullptr);
        QCOMPARE(valueInterface->currentValue().toInt(), 24);
        QCOMPARE(valueInterface->minimumValue().toInt(), -20);
        QCOMPARE(valueInterface->maximumValue().toInt(), 80);
    }

    void keepsObjectBudgetStable()
    {
        ZzFluentUI::ZzSpinBox spinBox;
        const qsizetype descendantCount = spinBox.findChildren<
            QObject *>().size();
        const qsizetype animationCount = spinBox.findChildren<
            QAbstractAnimation *>().size();
        const qsizetype timerCount = spinBox.findChildren<
            QTimer *>().size();

        for (int index = 0; index < 1000; ++index) {
            spinBox.setRange(-index, index + 1);
            spinBox.setValue(index % 17);
            spinBox.setButtonSymbols(
                index % 3 == 0
                    ? QAbstractSpinBox::NoButtons
                    : (index % 2 == 0
                           ? QAbstractSpinBox::PlusMinus
                           : QAbstractSpinBox::UpDownArrows));
            spinBox.setLayoutDirection(
                index % 2 == 0
                    ? Qt::LeftToRight
                    : Qt::RightToLeft);
        }

        QCOMPARE(spinBox.findChildren<QObject *>().size(), descendantCount);
        QCOMPARE(
            spinBox.findChildren<QAbstractAnimation *>().size(),
            animationCount);
        QCOMPARE(spinBox.findChildren<QTimer *>().size(), timerCount);
    }
};

QTEST_MAIN(ZzSpinBoxControlsTest)

#include "ZzSpinBoxControlsTest.moc"
