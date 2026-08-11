#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzInfoBadge.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

/** @brief 验证信息徽章数值、视觉、主题和无障碍契约。 */
class ZzInfoBadgeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void clampsValuesAndEmitsOnlyForChanges()
    {
        ZzFluentUI::ZzInfoBadge badge;
        QSignalSpy kindSpy(&badge, &ZzFluentUI::ZzInfoBadge::kindChanged);
        QSignalSpy valueSpy(&badge, &ZzFluentUI::ZzInfoBadge::valueChanged);
        QSignalSpy maximumSpy(
            &badge,
            &ZzFluentUI::ZzInfoBadge::maximumValueChanged);

        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        badge.setValue(-8);
        badge.setValue(120);
        badge.setValue(120);
        badge.setMaximumValue(0);
        badge.setMaximumValue(1);

        QCOMPARE(kindSpy.count(), 1);
        QCOMPARE(valueSpy.count(), 1);
        QCOMPARE(maximumSpy.count(), 1);
        QCOMPARE(badge.value(), 120);
        QCOMPARE(badge.maximumValue(), 1);
        QCOMPARE(badge.text(), QStringLiteral("1+"));
    }

    void switchesKindsAndKeepsBoundedObjectCount()
    {
        ZzFluentUI::ZzInfoBadge badge;
        const qsizetype descendants = badge.findChildren<QObject *>().size();
        badge.setValue(8);
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        const QSize numberSize = badge.sizeHint();
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Dot);
        const QSize dotSize = badge.sizeHint();
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Icon);
        badge.setIcon(QApplication::style()->standardIcon(
            QStyle::SP_MessageBoxInformation));

        QVERIFY(numberSize.width() >= dotSize.width());
        QVERIFY(!badge.icon().isNull());
        QCOMPARE(badge.findChildren<QObject *>().size(), descendants);
        QCOMPARE(badge.findChildren<QTimer *>().size(), 0);
        QCOMPARE(badge.focusPolicy(), Qt::NoFocus);
    }

    void exposesStaticTextAccessibilityAndPreservesHostName()
    {
        ZzFluentUI::ZzInfoBadge badge;
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        badge.setSeverity(ZzFluentUI::ZzMessageSeverity::Warning);
        badge.setValue(12);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&badge);

        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::StaticText);
        QVERIFY(interface->text(QAccessible::Name).contains(
            QStringLiteral("12")));

        badge.setAccessibleName(QStringLiteral("Unread builds"));
        badge.setValue(13);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&badge, &languageChange);
        QCOMPARE(badge.accessibleName(), QStringLiteral("Unread builds"));
    }

    void consumesFluentCaptionAndRendersAllSeverities()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzInfoBadge badge;
        badge.setStyle(&style);
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        badge.setValue(7);
        badge.resize(badge.sizeHint());

        QCOMPARE(
            badge.font(),
            style.themeSnapshot()->font(
                ZzFluentUI::ZzTypographyToken::Caption));
        for (const auto severity : {
                 ZzFluentUI::ZzMessageSeverity::Information,
                 ZzFluentUI::ZzMessageSeverity::Success,
                 ZzFluentUI::ZzMessageSeverity::Warning,
                 ZzFluentUI::ZzMessageSeverity::Error}) {
            badge.setSeverity(severity);
            QImage image(badge.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            badge.render(&image);
            QVERIFY(image.pixelColor(image.width() / 2, 1).alpha() > 0);
        }
    }

    void keepsLogicalSizeAcrossLayoutDirections()
    {
        ZzFluentUI::ZzInfoBadge badge;
        badge.setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
        badge.setMaximumValue(99);
        badge.setValue(100);
        badge.setLayoutDirection(Qt::LeftToRight);
        const QSize leftToRight = badge.sizeHint();
        badge.setLayoutDirection(Qt::RightToLeft);
        QCOMPARE(badge.sizeHint(), leftToRight);
        QCOMPARE(badge.text(), QStringLiteral("99+"));
    }
};

QTEST_MAIN(ZzInfoBadgeTest)

#include "ZzInfoBadgeTest.moc"
