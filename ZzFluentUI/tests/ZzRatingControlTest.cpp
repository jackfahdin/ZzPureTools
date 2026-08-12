#include <cmath>
#include <limits>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzRatingControl.h>
#include <ZzFluentUI/ZzRatingPrecision.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 创建应用级 Fluent style 供评分绘制测试复用。 */
std::unique_ptr<ZzFluentUI::ZzFluentStyle> zzCreateStyle(
    ZzFluentUI::ZzThemeController *controller)
{
    std::unique_ptr<QStyle> fusion(
        QStyleFactory::create(QStringLiteral("Fusion")));
    Q_ASSERT(fusion != nullptr);
    return std::make_unique<ZzFluentUI::ZzFluentStyle>(
        controller,
        fusion.release());
}

/** @brief 渲染评分控件以验证固定缓存和半星裁剪。 */
QImage zzRenderRating(ZzFluentUI::ZzRatingControl *control)
{
    Q_ASSERT(control != nullptr);
    QImage image(control->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    control->render(&painter);
    return image;
}

/** @brief 统计图像指定矩形中接近强调色的像素。 */
int zzAccentPixelCount(
    const QImage &image,
    const QRect &rect,
    const QColor &accent)
{
    int count = 0;
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() > 32
                && qAbs(actual.red() - accent.red()) <= 12
                && qAbs(actual.green() - accent.green()) <= 12
                && qAbs(actual.blue() - accent.blue()) <= 12) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

/** @brief 验证评分控件的量化输入、RTL、绘制和浮点无障碍值。 */
class ZzRatingControlTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableDefaultsAndBounds()
    {
        ZzFluentUI::ZzRatingControl control;
        QSignalSpy maximumSpy(
            &control,
            &ZzFluentUI::ZzRatingControl::maximumRatingChanged);

        QCOMPARE(control.rating(), 0.0);
        QCOMPARE(control.maximumRating(), 5);
        QCOMPARE(
            control.precision(),
            ZzFluentUI::ZzRatingPrecision::Whole);
        QVERIFY(!control.isReadOnly());
        QCOMPARE(control.focusPolicy(), Qt::StrongFocus);
        QVERIFY(control.sizeHint().width() > control.sizeHint().height());
        QVERIFY(control.minimumSizeHint().width() > 0);
        QVERIFY(control.findChildren<QObject *>().isEmpty());

        control.setMaximumRating(0);
        QCOMPARE(control.maximumRating(), 1);
        control.setMaximumRating(-20);
        QCOMPARE(maximumSpy.count(), 1);
        control.setMaximumRating(100);
        QCOMPARE(control.maximumRating(), 10);
        QCOMPARE(maximumSpy.count(), 2);
    }

    void quantizesFiniteValuesAndRejectsInvalidInput()
    {
        ZzFluentUI::ZzRatingControl control;
        QSignalSpy ratingSpy(
            &control,
            &ZzFluentUI::ZzRatingControl::ratingChanged);
        QSignalSpy precisionSpy(
            &control,
            &ZzFluentUI::ZzRatingControl::precisionChanged);

        control.setRating(2.4);
        QCOMPARE(control.rating(), 2.0);
        control.setRating(2.6);
        QCOMPARE(control.rating(), 3.0);
        control.setRating(3.0);
        QCOMPARE(ratingSpy.count(), 2);
        control.setRating(std::numeric_limits<qreal>::quiet_NaN());
        control.setRating(std::numeric_limits<qreal>::infinity());
        QCOMPARE(control.rating(), 3.0);
        QCOMPARE(ratingSpy.count(), 2);

        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        QCOMPARE(precisionSpy.count(), 1);
        control.setRating(3.24);
        QCOMPARE(control.rating(), 3.0);
        control.setRating(3.26);
        QCOMPARE(control.rating(), 3.5);
        control.setRating(-8.0);
        QCOMPARE(control.rating(), 0.0);
        control.setRating(20.0);
        QCOMPARE(control.rating(), 5.0);

        control.setMaximumRating(3);
        QCOMPARE(control.rating(), 3.0);
    }

    void mouseSupportsHalfCellsDraggingAndHoverPreview()
    {
        ZzFluentUI::ZzRatingControl control;
        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        control.resize(control.sizeHint());
        control.show();
        QCoreApplication::processEvents();
        QSignalSpy ratingSpy(
            &control,
            &ZzFluentUI::ZzRatingControl::ratingChanged);

        QTest::mouseMove(&control, QPoint(8, control.height() / 2));
        QCOMPARE(control.rating(), 0.0);
        QCOMPARE(ratingSpy.count(), 0);
        QTest::mouseClick(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(8, control.height() / 2));
        QCOMPARE(control.rating(), 0.5);

        QTest::mousePress(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(18, control.height() / 2));
        QTest::mouseMove(
            &control,
            QPoint(control.width() - 8, control.height() / 2),
            1);
        QTest::mouseRelease(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(control.width() - 8, control.height() / 2));
        QCOMPARE(control.rating(), 5.0);
        QVERIFY(ratingSpy.count() >= 2);

        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(&control, &leave);
        QCOMPARE(control.rating(), 5.0);
    }

    void readOnlyAndDisabledRejectUserInput()
    {
        ZzFluentUI::ZzRatingControl control;
        control.setRating(2.0);
        control.resize(control.sizeHint());
        control.show();
        control.setFocus();
        QCoreApplication::processEvents();

        control.setReadOnly(true);
        QTest::mouseClick(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(control.width() - 8, control.height() / 2));
        QTest::keyClick(&control, Qt::Key_End);
        QCOMPARE(control.rating(), 2.0);

        control.setReadOnly(false);
        control.setEnabled(false);
        QTest::mouseClick(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(control.width() - 8, control.height() / 2));
        QTest::keyClick(&control, Qt::Key_End);
        QCOMPARE(control.rating(), 2.0);
    }

    void keyboardUsesPrecisionAndVisualDirection()
    {
        ZzFluentUI::ZzRatingControl control;
        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        control.setRating(2.0);
        control.resize(control.sizeHint());
        control.show();
        control.setFocus();
        QCoreApplication::processEvents();

        QTest::keyClick(&control, Qt::Key_Right);
        QCOMPARE(control.rating(), 2.5);
        QTest::keyClick(&control, Qt::Key_Up);
        QCOMPARE(control.rating(), 3.0);
        QTest::keyClick(&control, Qt::Key_Left);
        QCOMPARE(control.rating(), 2.5);
        QTest::keyClick(&control, Qt::Key_Down);
        QCOMPARE(control.rating(), 2.0);
        QTest::keyClick(&control, Qt::Key_Home);
        QCOMPARE(control.rating(), 0.0);
        QTest::keyClick(&control, Qt::Key_End);
        QCOMPARE(control.rating(), 5.0);

        control.setLayoutDirection(Qt::RightToLeft);
        QTest::keyClick(&control, Qt::Key_Left);
        QCOMPARE(control.rating(), 5.0);
        QTest::keyClick(&control, Qt::Key_Right);
        QCOMPARE(control.rating(), 4.5);
    }

    void rightToLeftMouseKeepsFirstValueAtLeadingEdge()
    {
        ZzFluentUI::ZzRatingControl control;
        control.setLayoutDirection(Qt::RightToLeft);
        control.resize(control.sizeHint());
        control.show();
        QCoreApplication::processEvents();

        QTest::mouseClick(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(control.width() - 8, control.height() / 2));
        QCOMPARE(control.rating(), 1.0);
        QTest::mouseClick(
            &control,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(8, control.height() / 2));
        QCOMPARE(control.rating(), 5.0);
    }

    void halfStarClipDoesNotFillAdjacentCell()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = zzCreateStyle(&controller);
        ZzFluentUI::ZzRatingControl control;
        control.setStyle(style.get());
        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        control.setRating(0.5);
        control.resize(control.sizeHint());
        control.show();
        QCoreApplication::processEvents();

        const QImage image = zzRenderRating(&control);
        const QColor accent = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::Accent);
        const int firstLeft = zzAccentPixelCount(
            image,
            QRect(2, 2, 12, 24),
            accent);
        const int firstRight = zzAccentPixelCount(
            image,
            QRect(14, 2, 12, 24),
            accent);
        const int secondCell = zzAccentPixelCount(
            image,
            QRect(30, 2, 24, 24),
            accent);
        QVERIFY(firstLeft > 0);
        QCOMPARE(firstRight, 0);
        QCOMPARE(secondCell, 0);
    }

    void exposesSliderWithFloatingPointValueInterface()
    {
        ZzFluentUI::ZzRatingControl control;
        control.setAccessibleName(QStringLiteral("Product rating"));
        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        control.setRating(2.5);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&control);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::Slider);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Product rating"));
        QAccessibleValueInterface *valueInterface =
            interface->valueInterface();
        QVERIFY(valueInterface != nullptr);
        QCOMPARE(valueInterface->currentValue().toDouble(), 2.5);
        QCOMPARE(valueInterface->minimumValue().toDouble(), 0.0);
        QCOMPARE(valueInterface->maximumValue().toDouble(), 5.0);
        QCOMPARE(valueInterface->minimumStepSize().toDouble(), 0.5);

        valueInterface->setCurrentValue(3.5);
        QCOMPARE(control.rating(), 3.5);
        control.setReadOnly(true);
        QVERIFY(interface->state().readOnly);
        valueInterface->setCurrentValue(4.5);
        QCOMPARE(control.rating(), 3.5);
        QVERIFY(!interface->text(QAccessible::Description).isEmpty());
    }

    void repeatedUpdatesKeepObjectAndStyleCachesBounded()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = zzCreateStyle(&controller);
        ZzFluentUI::ZzRatingControl control;
        control.setStyle(style.get());
        control.setPrecision(ZzFluentUI::ZzRatingPrecision::Half);
        control.resize(control.sizeHint());
        control.show();
        QCoreApplication::processEvents();
        const qsizetype childCount =
            control.findChildren<QObject *>().size();

        for (int index = 0; index < 1000; ++index) {
            control.setRating(static_cast<qreal>(index % 11) / 2.0);
            QTest::mouseMove(
                &control,
                QPoint(
                    2 + (index % std::max(1, control.width() - 4)),
                    control.height() / 2));
            static_cast<void>(zzRenderRating(&control));
        }
        QCOMPARE(control.findChildren<QObject *>().size(), childCount);
        QVERIFY(style->iconCacheBytes() > 0);
        QVERIFY(style->iconCacheBytes() <= 8 * 1024 * 1024);
    }
};

QTEST_MAIN(ZzRatingControlTest)

#include "ZzRatingControlTest.moc"
