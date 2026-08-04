#include <QtCore/QEvent>
#include <QtCore/QtGlobal>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>
#include <QtWidgets/QPushButton>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/** @brief 统计主题传播产生的 StyleChange 事件。 */
class ZzStyleChangeProbe final : public QPushButton
{
public:
    using QPushButton::QPushButton;

    /** @brief 返回已接收的 StyleChange 数量。 */
    [[nodiscard]] int styleChangeCount() const noexcept
    {
        return styleChangeCount_;
    }

protected:
    /** @brief 统计事件后保留 QPushButton 默认行为。 */
    bool event(QEvent *event) override
    {
        if (event != nullptr && event->type() == QEvent::StyleChange) {
            ++styleChangeCount_;
        }
        return QPushButton::event(event);
    }

private:
    int styleChangeCount_ = 0;
};

/**
 * @brief 验证 Fluent Widgets 样式、绘制和缓存传播行为。
 */
class ZzFluentStyleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsMetricsAndPalette()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        QCOMPARE(style.pixelMetric(QStyle::PM_ButtonMargin), 12);
        QCOMPARE(
            style.standardPalette().color(QPalette::Window),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::Surface));
    }

    void invalidatesColorCacheWithoutChangingMetric()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const int metric = style.pixelMetric(QStyle::PM_ButtonMargin);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);

        QCOMPARE(
            style.themeRevision(),
            controller.snapshot()->revision());
        QCOMPARE(style.pixelMetric(QStyle::PM_ButtonMargin), metric);
        QCOMPARE(style.iconCacheBytes(), 0);
    }

    void exposesAccessibleFocusPolicy()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);

        QVERIFY(style.styleHint(QStyle::SH_UnderlineShortcut) >= 0);
        QVERIFY(style.pixelMetric(QStyle::PM_FocusFrameHMargin) >= 2);
    }

    void drawsVisibleHighContrastFocusRing()
    {
        QImage image(
            QSize(64, 32),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QPainter painter(&image);
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::HighContrast,
            QColor(Qt::yellow),
            1,
            true);

        ZzFluentUI::ZzFluentPainter::drawFocusRing(
            &painter,
            QRectF(2, 2, 60, 28),
            snapshot,
            1.0);

        painter.end();
        QVERIFY(image.pixelColor(2, 16) != QColor(Qt::black));
    }

    void cachesTintedResourceIcons()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const ZzFluentUI::ZzIconDescriptor descriptor{
            QStringLiteral(
                ":/zzfluent/tests/ZzFluentTestSquare.svg"),
            true};

        const QPixmap first = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::LeftToRight);
        QVERIFY(!first.isNull());
        const int firstCost = style.iconCacheBytes();
        QVERIFY(firstCost > 0);

        const QPixmap second = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::LeftToRight);
        QCOMPARE(second.cacheKey(), first.cacheKey());
        QCOMPARE(style.iconCacheBytes(), firstCost);

        const QPixmap mirrored = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::RightToLeft);
        QVERIFY(!mirrored.isNull());
        QVERIFY(first.toImage() != mirrored.toImage());
        const int mirroredCost = style.iconCacheBytes();
        QVERIFY(mirroredCost > firstCost);

        controller.setReducedMotion(true);
        QCOMPARE(
            style.themeRevision(),
            controller.snapshot()->revision());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);
        const QPixmap afterMotionChange = style.iconPixmap(
            descriptor,
            QSize(16, 16),
            1.25,
            QColor(Qt::green),
            Qt::LeftToRight);
        QCOMPARE(afterMotionChange.cacheKey(), first.cacheKey());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);

        const QPixmap oversized = style.iconPixmap(
            descriptor,
            QSize(4096, 4096),
            1.0,
            QColor(Qt::green),
            Qt::LeftToRight);
        QVERIFY(oversized.isNull());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(style.iconCacheBytes(), 0);
    }

    void sendsGeometryChangesOnlyForGeometryUpdates()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzStyleChangeProbe topLevel;
        ZzStyleChangeProbe child(&topLevel);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(topLevel.styleChangeCount(), 0);
        QCOMPARE(child.styleChangeCount(), 0);

        const QFont original = QGuiApplication::font();
        QFont changed = original;
        if (changed.pointSizeF() > 0.0) {
            changed.setPointSizeF(changed.pointSizeF() + 1.0);
        } else {
            changed.setPixelSize(qMax(1, changed.pixelSize()) + 1);
        }
        QGuiApplication::setFont(changed);

        QTRY_COMPARE(topLevel.styleChangeCount(), 1);
        QTRY_COMPARE(child.styleChangeCount(), 1);
        QGuiApplication::setFont(original);
    }
};

QTEST_MAIN(ZzFluentStyleTest)

#include "ZzFluentStyleTest.moc"
