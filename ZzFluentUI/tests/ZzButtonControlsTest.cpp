#include <QtCore/QCoreApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>

/** @brief 验证 Fluent 按钮的外观、图标缓存和 Qt 原生激活语义。 */
class ZzButtonControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pushButtonPreservesQtActivation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzPushButton button(QStringLiteral("Apply"));
        button.setStyle(&style);
        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        button.setAccessibleName(QStringLiteral("Apply changes"));
        button.resize(120, 36);
        button.show();
        QCoreApplication::processEvents();

        QSignalSpy clickedSpy(&button, &QPushButton::clicked);
        button.setFocus();
        QTest::keyClick(&button, Qt::Key_Space);

        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(
            button.appearance(),
            ZzFluentUI::ZzButtonAppearance::Accent);
        QCOMPARE(
            button.accessibleName(),
            QStringLiteral("Apply changes"));

        QImage image(
            button.size(),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        button.render(&painter);
        painter.end();
        QCOMPARE(
            image.pixelColor(10, button.height() / 2),
            button.palette().color(QPalette::Highlight));

        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
        QCOMPARE(
            button.appearance(),
            ZzFluentUI::ZzButtonAppearance::Subtle);
    }

    void iconButtonUsesStableToolButtonSemantics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzIconButton button;
        button.setStyle(&style);
        button.setAccessibleName(QStringLiteral("Refresh"));
        button.resize(32, 32);
        button.setIconDescriptor({
            QStringLiteral(
                ":/zzfluent/buttons/ZzFluentTestSquare.svg"),
            true});
        button.show();
        QCoreApplication::processEvents();

        QSignalSpy clickedSpy(&button, &QToolButton::clicked);
        QTest::mouseClick(&button, Qt::LeftButton);

        QCOMPARE(clickedSpy.count(), 1);
        QVERIFY(button.autoRaise());
        QCOMPARE(button.toolButtonStyle(), Qt::ToolButtonIconOnly);
        QCOMPARE(button.focusPolicy(), Qt::StrongFocus);
        QCOMPARE(button.accessibleName(), QStringLiteral("Refresh"));
        QVERIFY(!button.icon().isNull());
        QCOMPARE(button.iconSize(), QSize(20, 20));
        QVERIFY(style.iconCacheBytes() > 0);
    }

    void iconButtonRefreshesVisualInputs()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzIconButton button;
        button.setStyle(&style);
        button.resize(40, 40);
        button.setIconDescriptor({
            QStringLiteral(
                ":/zzfluent/buttons/ZzFluentTestSquare.svg"),
            true});
        button.show();
        QCoreApplication::processEvents();
        const QSize initialSize = button.iconSize();
        QVERIFY(!button.icon().isNull());

        button.resize(48, 48);
        QCoreApplication::processEvents();
        QVERIFY(button.iconSize().width() > initialSize.width());
        button.setLayoutDirection(Qt::RightToLeft);
        QVERIFY(!button.icon().isNull());
        button.setEnabled(false);
        QVERIFY(!button.icon().isNull());
    }

    void iconButtonSupportsExplicitSvgColor()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzIconButton button;
        button.setStyle(&style);
        button.resize(40, 40);
        button.setIconDescriptor(
            ZzFluentUI::ZzIconDescriptor::fromSvgResource(
                QStringLiteral(
                    ":/zzfluent/buttons/ZzFluentTestSquare.svg")));
        button.setIconColor(QColor(Qt::red));
        button.show();
        QCoreApplication::processEvents();

        QCOMPARE(button.iconColor(), QColor(Qt::red));
        const QImage redImage = button.icon()
            .pixmap(button.iconSize())
            .toImage();
        QCOMPARE(
            redImage.pixelColor(
                redImage.width() / 2,
                redImage.height() / 2),
            QColor(Qt::red));

        button.resetIconColor();
        QVERIFY(!button.iconColor().isValid());
        QVERIFY(!button.icon().isNull());
    }
};

QTEST_MAIN(ZzButtonControlsTest)

#include "ZzButtonControlsTest.moc"
