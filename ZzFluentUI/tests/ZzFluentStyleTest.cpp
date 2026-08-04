#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/**
 * @brief 验证 Fluent Widgets 绘制原语和样式缓存行为。
 */
class ZzFluentStyleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
};

QTEST_MAIN(ZzFluentStyleTest)

#include "ZzFluentStyleTest.moc"
