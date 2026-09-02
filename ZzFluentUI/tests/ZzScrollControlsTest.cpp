#include <algorithm>
#include <array>
#include <limits>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QEnterEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QFrame>
#include <QtWidgets/QStyleOptionSlider>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzThemeController.h>

namespace {

/** @brief 返回图像是否包含不透明绘制结果。 */
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

/** @brief 把滚动条 style option 绘制到同尺寸透明图像。 */
QImage zzRenderScrollOption(
    const ZzFluentUI::ZzFluentStyle &style,
    const QStyleOptionSlider &option)
{
    QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style.drawComplexControl(
        QStyle::CC_ScrollBar,
        &option,
        &painter);
    painter.end();
    return image;
}

/** @brief 构造可独立查询滚动条几何的完整 style option。 */
QStyleOptionSlider zzScrollOption(
    Qt::Orientation orientation,
    QRect rect,
    int minimum,
    int maximum,
    int pageStep,
    int sliderPosition,
    bool upsideDown = false)
{
    QStyleOptionSlider option;
    option.rect = rect;
    option.state = QStyle::State_Enabled;
    option.subControls = QStyle::SC_All;
    option.orientation = orientation;
    option.minimum = minimum;
    option.maximum = maximum;
    option.pageStep = pageStep;
    option.singleStep = 1;
    option.sliderPosition = sliderPosition;
    option.sliderValue = sliderPosition;
    option.upsideDown = upsideDown;
    return option;
}

/** @brief 向控件发送确定坐标的进入事件。 */
void zzSendEnter(QWidget *widget)
{
    const QPointF center = QRectF(widget->rect()).center();
    QEnterEvent event(center, center, center);
    QCoreApplication::sendEvent(widget, &event);
}

/** @brief 向控件发送离开事件。 */
void zzSendLeave(QWidget *widget)
{
    QEvent event(QEvent::Leave);
    QCoreApplication::sendEvent(widget, &event);
}

} // namespace

/** @brief 验证 Fluent 滚动条和滚动区域的公共契约。 */
class ZzScrollControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void constructsWithQtDefaults()
    {
        ZzFluentUI::ZzScrollBar vertical;
        ZzFluentUI::ZzScrollBar horizontal(Qt::Horizontal);

        QCOMPARE(vertical.orientation(), Qt::Vertical);
        QCOMPARE(horizontal.orientation(), Qt::Horizontal);
        QCOMPARE(vertical.minimum(), 0);
        QCOMPARE(vertical.maximum(), 99);
        QCOMPARE(vertical.value(), 0);
        QCOMPARE(vertical.findChildren<QAbstractAnimation *>().size(), 1);
        QCOMPARE(vertical.findChildren<QTimer *>().size(), 0);
    }

    void areaOwnsDirectFluentScrollBars()
    {
        ZzFluentUI::ZzScrollArea area;
        auto *horizontal = area.fluentHorizontalScrollBar();
        auto *vertical = area.fluentVerticalScrollBar();

        QVERIFY(horizontal != nullptr);
        QVERIFY(vertical != nullptr);
        QCOMPARE(horizontal, area.horizontalScrollBar());
        QCOMPARE(vertical, area.verticalScrollBar());
        QCOMPARE(horizontal->orientation(), Qt::Horizontal);
        QCOMPARE(vertical->orientation(), Qt::Vertical);
        QCOMPARE(area.frameShape(), QFrame::NoFrame);

        QPointer<ZzFluentUI::ZzScrollBar> replaced = vertical;
        area.setVerticalScrollBar(new QScrollBar(Qt::Vertical));
        QCOMPARE(area.fluentVerticalScrollBar(), nullptr);
        QVERIFY(replaced.isNull());
    }

    void preservesRangeActionsAndKeyboard()
    {
        ZzFluentUI::ZzScrollBar bar;
        bar.setRange(10, 90);
        bar.setSingleStep(3);
        bar.setPageStep(20);
        bar.setValue(40);
        QSignalSpy valueSpy(&bar, &QScrollBar::valueChanged);
        bar.resize(16, 240);
        bar.show();
        bar.setFocus();

        QTest::keyClick(&bar, Qt::Key_Down);
        QCOMPARE(bar.value(), 43);
        QTest::keyClick(&bar, Qt::Key_PageDown);
        QCOMPARE(bar.value(), 63);
        QTest::keyClick(&bar, Qt::Key_Home);
        QCOMPARE(bar.value(), 10);
        QTest::keyClick(&bar, Qt::Key_End);
        QCOMPARE(bar.value(), 90);
        bar.triggerAction(QAbstractSlider::SliderPageStepSub);
        QCOMPARE(bar.value(), 70);
        QVERIFY(valueSpy.size() >= 5);
    }

    void preservesMouseSliderInteraction()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzScrollBar bar(Qt::Horizontal);
        bar.setStyle(&style);
        bar.setRange(0, 100);
        bar.setPageStep(20);
        bar.setValue(40);
        bar.resize(240, 12);
        bar.show();
        QCoreApplication::processEvents();

        QStyleOptionSlider option = zzScrollOption(
            Qt::Horizontal,
            bar.rect(),
            bar.minimum(),
            bar.maximum(),
            bar.pageStep(),
            bar.sliderPosition());
        option.subControls = QStyle::SC_None;
        const QRect slider = style.subControlRect(
            QStyle::CC_ScrollBar,
            &option,
            QStyle::SC_ScrollBarSlider,
            &bar);
        QVERIFY(!slider.isEmpty());
        QCOMPARE(
            style.hitTestComplexControl(
                QStyle::CC_ScrollBar,
                &option,
                slider.center(),
                &bar),
            QStyle::SC_ScrollBarSlider);

        QTest::mousePress(
            &bar,
            Qt::LeftButton,
            Qt::NoModifier,
            slider.center());
        QVERIFY(bar.isSliderDown());
        QTest::mouseMove(
            &bar,
            slider.center() + QPoint(36, 0));
        QVERIFY(bar.value() > 40);
        QTest::mouseRelease(
            &bar,
            Qt::LeftButton,
            Qt::NoModifier,
            slider.center() + QPoint(36, 0));
        QVERIFY(!bar.isSliderDown());
    }

    void exposesStableGeometryAndHitTesting()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QCOMPARE(style.pixelMetric(QStyle::PM_ScrollBarExtent), 12);
        QCOMPARE(style.pixelMetric(QStyle::PM_ScrollBarSliderMin), 24);

        for (const Qt::Orientation orientation : {
                 Qt::Horizontal,
                 Qt::Vertical}) {
            const QRect rect = orientation == Qt::Horizontal
                ? QRect(0, 0, 240, 12)
                : QRect(0, 0, 12, 240);
            QStyleOptionSlider option = zzScrollOption(
                orientation,
                rect,
                0,
                100,
                20,
                45);
            const QRect groove = style.subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarGroove);
            const QRect slider = style.subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarSlider);
            const QRect subPage = style.subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarSubPage);
            const QRect addPage = style.subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarAddPage);

            QCOMPARE(groove, rect);
            QVERIFY(rect.contains(slider));
            QVERIFY(!slider.isEmpty());
            QVERIFY(!subPage.intersects(slider));
            QVERIFY(!addPage.intersects(slider));
            QVERIFY(!subPage.intersects(addPage));
            QVERIFY(style.subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarSubLine).isEmpty());
            QVERIFY(style.subControlRect(
                QStyle::CC_ScrollBar,
                &option,
                QStyle::SC_ScrollBarAddLine).isEmpty());
            QCOMPARE(
                style.hitTestComplexControl(
                    QStyle::CC_ScrollBar,
                    &option,
                    slider.center()),
                QStyle::SC_ScrollBarSlider);
            if (!subPage.isEmpty()) {
                QCOMPARE(
                    style.hitTestComplexControl(
                        QStyle::CC_ScrollBar,
                        &option,
                        subPage.center()),
                    QStyle::SC_ScrollBarSubPage);
            }
            if (!addPage.isEmpty()) {
                QCOMPARE(
                    style.hitTestComplexControl(
                        QStyle::CC_ScrollBar,
                        &option,
                        addPage.center()),
                    QStyle::SC_ScrollBarAddPage);
            }
        }
    }

    void handlesRtlZeroRangeAndLargeRange()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionSlider leftToRight = zzScrollOption(
            Qt::Horizontal,
            QRect(0, 0, 240, 12),
            0,
            100,
            0,
            0,
            false);
        QStyleOptionSlider rightToLeft = leftToRight;
        rightToLeft.upsideDown = true;
        const QRect leftSlider = style.subControlRect(
            QStyle::CC_ScrollBar,
            &leftToRight,
            QStyle::SC_ScrollBarSlider);
        const QRect rightSlider = style.subControlRect(
            QStyle::CC_ScrollBar,
            &rightToLeft,
            QStyle::SC_ScrollBarSlider);
        QVERIFY(leftSlider.left() < rightSlider.left());
        QVERIFY(leftSlider.width() >= 24);
        QCOMPARE(leftSlider.width(), rightSlider.width());

        QStyleOptionSlider zero = zzScrollOption(
            Qt::Vertical,
            QRect(0, 0, 12, 80),
            5,
            5,
            0,
            5);
        QCOMPARE(
            style.subControlRect(
                QStyle::CC_ScrollBar,
                &zero,
                QStyle::SC_ScrollBarSlider),
            zero.rect);

        QStyleOptionSlider large = zzScrollOption(
            Qt::Vertical,
            QRect(0, 0, 12, 1000),
            0,
            std::numeric_limits<int>::max(),
            std::numeric_limits<int>::max(),
            std::numeric_limits<int>::max() / 2);
        const QRect largeSlider = style.subControlRect(
            QStyle::CC_ScrollBar,
            &large,
            QStyle::SC_ScrollBarSlider);
        QVERIFY(large.rect.contains(largeSlider));
        QVERIFY(largeSlider.height() >= 24);
    }

    void rendersStandardAndAnimatedScrollBars()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QScrollBar standard(Qt::Horizontal);
        ZzFluentUI::ZzScrollBar fluent(Qt::Horizontal);

        const std::array<QScrollBar *, 2> scrollBars{
            &standard,
            &fluent};
        for (QScrollBar *bar : scrollBars) {
            bar->setStyle(&style);
            bar->setRange(0, 100);
            bar->setPageStep(25);
            bar->setValue(50);
            bar->resize(240, 12);
            QImage image(
                bar->size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            bar->render(&painter);
            painter.end();
            QVERIFY(zzContainsOpaquePixel(image));
        }
    }

    void distinguishesPressedStateAndFillsAreaCorner()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionSlider hovered = zzScrollOption(
            Qt::Horizontal,
            QRect(0, 0, 240, 12),
            0,
            100,
            25,
            50);
        hovered.palette = style.standardPalette();
        hovered.state |= QStyle::State_MouseOver;
        hovered.activeSubControls = QStyle::SC_ScrollBarSlider;
        QStyleOptionSlider pressed = hovered;
        pressed.state |= QStyle::State_Sunken;
        const QImage hoveredImage = zzRenderScrollOption(style, hovered);
        const QImage pressedImage = zzRenderScrollOption(style, pressed);
        const QPoint sliderCenter = style.subControlRect(
            QStyle::CC_ScrollBar,
            &pressed,
            QStyle::SC_ScrollBarSlider).center();

        QCOMPARE(
            hoveredImage.pixelColor(sliderCenter),
            hovered.palette.color(QPalette::Text));
        QCOMPARE(
            pressedImage.pixelColor(sliderCenter),
            pressed.palette.color(QPalette::Highlight));
        QVERIFY(hoveredImage != pressedImage);

        QStyleOption corner;
        corner.rect = QRect(0, 0, 12, 12);
        corner.palette = style.standardPalette();
        QImage cornerImage(
            corner.rect.size(),
            QImage::Format_ARGB32_Premultiplied);
        cornerImage.fill(Qt::magenta);
        QPainter painter(&cornerImage);
        style.drawPrimitive(
            QStyle::PE_PanelScrollAreaCorner,
            &corner,
            &painter);
        painter.end();
        QCOMPARE(
            cornerImage.pixelColor(corner.rect.center()),
            corner.palette.color(QPalette::Window));
    }

    void boundsAnimationAndStopsWhenInactive()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzScrollBar bar;
        bar.setStyle(&style);
        bar.resize(12, 240);
        bar.show();
        auto *animation = bar.findChild<QAbstractAnimation *>();
        if (animation == nullptr) {
            QTest::qFail(
                "未找到滚动条悬停动画",
                __FILE__,
                __LINE__);
            return;
        }

        zzSendEnter(&bar);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));
        zzSendLeave(&bar);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Stopped;
        }));

        zzSendEnter(&bar);
        controller.setReducedMotion(true);
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Stopped;
        }));
        controller.setReducedMotion(false);
        zzSendEnter(&bar);
        bar.setEnabled(false);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        bar.setEnabled(true);
        zzSendEnter(&bar);
        bar.hide();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    }

    void keepsObjectCountsStable()
    {
        ZzFluentUI::ZzScrollBar bar;
        bar.resize(12, 240);
        bar.show();
        const qsizetype descendants = bar.findChildren<QObject *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            bar.setRange(0, 100 + iteration);
            bar.setValue(iteration % 101);
            bar.setOrientation(
                (iteration % 2) == 0
                    ? Qt::Horizontal
                    : Qt::Vertical);
            if ((iteration % 2) == 0) {
                zzSendEnter(&bar);
            } else {
                zzSendLeave(&bar);
            }
        }
        bar.hide();

        QCOMPARE(bar.findChildren<QObject *>().size(), descendants);
        QCOMPARE(bar.findChildren<QAbstractAnimation *>().size(), 1);
        QCOMPARE(bar.findChildren<QTimer *>().size(), 0);
        QCOMPARE(
            bar.findChild<QAbstractAnimation *>()->state(),
            QAbstractAnimation::Stopped);
    }

    void exposesScrollBarAccessibility()
    {
        ZzFluentUI::ZzScrollBar bar;
        bar.setAccessibleName(QStringLiteral("Document position"));
        bar.setRange(10, 90);
        bar.setValue(40);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&bar);

        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::ScrollBar);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Document position"));
        QAccessibleValueInterface *valueInterface =
            interface->valueInterface();
        QVERIFY(valueInterface != nullptr);
        QCOMPARE(valueInterface->minimumValue().toInt(), 10);
        QCOMPARE(valueInterface->maximumValue().toInt(), 90);
        QCOMPARE(valueInterface->currentValue().toInt(), 40);
    }

    void destroysRunningAnimationWithoutDeferredCallbacks()
    {
        auto *bar = new ZzFluentUI::ZzScrollBar;
        bar->resize(12, 240);
        bar->show();
        zzSendEnter(bar);
        auto *animation = bar->findChild<QAbstractAnimation *>();
        if (animation == nullptr) {
            delete bar;
            QFAIL("未找到滚动条悬停动画");
        }
        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Running;
        }));

        bar->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(ZzScrollControlsTest)

#include "ZzScrollControlsTest.moc"
