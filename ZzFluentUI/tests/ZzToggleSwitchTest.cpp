#include <QtCore/QCoreApplication>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QProxyStyle>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

/** @brief 为动效单元测试提供确定启用动画的基础样式。 */
class ZzAnimationEnabledBaseStyle final : public QProxyStyle
{
public:
    /** @brief 对 Widget 动效返回启用，其余提示委托平台样式。 */
    [[nodiscard]] int styleHint(
        StyleHint hint,
        const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr,
        QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == SH_Widget_Animate) {
            return 1;
        }
        return QProxyStyle::styleHint(
            hint,
            option,
            widget,
            returnData);
    }
};

/** @brief 验证 Fluent 开关的语义、绘制和单动画复用。 */
class ZzToggleSwitchTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void spaceTogglesExactlyOnce()
    {
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setText(QStringLiteral("Wi-Fi"));
        toggle.show();
        QCoreApplication::processEvents();
        toggle.setFocus();
        QSignalSpy spy(&toggle, &QCheckBox::toggled);

        QTest::keyClick(&toggle, Qt::Key_Space);

        QVERIFY(toggle.isChecked());
        QCOMPARE(spy.count(), 1);
    }

    void exposesCheckBoxAccessibility()
    {
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setAccessibleName(QStringLiteral("Wi-Fi"));
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&toggle);

        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::CheckBox);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Wi-Fi"));
    }

    void reusesOneAnimationObject()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzAnimationEnabledBaseStyle);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.show();
        QCoreApplication::processEvents();
        const int before = toggle.findChildren<QVariantAnimation *>().size();

        for (int index = 0; index < 20; ++index) {
            toggle.setChecked(!toggle.isChecked());
        }

        QCOMPARE(toggle.findChildren<QVariantAnimation *>().size(), before);
        QCOMPARE(before, 1);
    }

    void stopsRunningAnimationForReducedMotion()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller,
            new ZzAnimationEnabledBaseStyle);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.show();
        QCoreApplication::processEvents();
        auto *animation = toggle.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        toggle.setChecked(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);

        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
    }

    void keepsStableLogicalGeometryAndRtlRendering()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzToggleSwitch toggle(QStringLiteral("Bluetooth"));
        toggle.setStyle(&style);
        toggle.setLayoutDirection(Qt::RightToLeft);
        toggle.setChecked(true);
        toggle.resize(toggle.sizeHint());

        QVERIFY(toggle.sizeHint().width() > 48);
        QVERIFY(toggle.sizeHint().height() >= 20);
        QImage image(
            toggle.size(),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        toggle.render(&painter);
        painter.end();

        bool hasOpaquePixel = false;
        for (int y = 0; y < image.height() && !hasOpaquePixel; ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y).alpha() > 0) {
                    hasOpaquePixel = true;
                    break;
                }
            }
        }
        QVERIFY(hasOpaquePixel);
    }
};

QTEST_MAIN(ZzToggleSwitchTest)

#include "ZzToggleSwitchTest.moc"
