#include <algorithm>
#include <cmath>

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
    /** @brief 切换主题时圆点响应深浅，同时在自定义强调色及按压状态中保持可辨识。 */
    void checkedThumbFollowsThemeAndKeepsContrast()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.setChecked(true);
        toggle.resize(toggle.sizeHint());
        const auto luminance = [](const QColor &color) {
            const auto linear = [](double value) {
                return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF())
                + 0.0722 * linear(color.blueF());
        };
        for (const auto mode : {ZzFluentUI::ZzThemeMode::Light, ZzFluentUI::ZzThemeMode::Dark}) {
            controller.setMode(mode);
            for (const QColor accent : {QColor("#0067c0"), QColor("#36a6ff"),
                     QColor("#ffff00"), QColor("#080808"), QColor("#000000"),
                     QColor("#ffffff"), QColor("#40000000"), QColor("#40ffffff"),
                     QColor("#6e000000"), QColor("#60ffffff")}) {
                controller.setAccentColor(accent);
                toggle.setPalette(style.standardPalette());
                QColor normalThumb;
                for (int state = 0; state < 3; ++state) {
                    toggle.setAttribute(Qt::WA_UnderMouse, state != 0);
                    toggle.setDown(state == 2);
                    QImage image(toggle.size(), QImage::Format_ARGB32_Premultiplied);
                    image.fill(Qt::transparent);
                    toggle.render(&image);
                    const QColor thumb = image.pixelColor(30, toggle.rect().center().y());
                    const QColor track = image.pixelColor(8, toggle.rect().center().y());
                    const double thumbLuminance = luminance(thumb);
                    const double trackLuminance = luminance(track);
                    const double fillContrast = (std::max(thumbLuminance, trackLuminance) + 0.05)
                        / (std::min(thumbLuminance, trackLuminance) + 0.05);
                    if (mode == ZzFluentUI::ZzThemeMode::Light) {
                        QCOMPARE(thumb, QColor(Qt::white));
                    }
                    // 明亮强调色上的白色圆点以轮廓保证辨识，不反转填充色。
                    const double edgeLuminance = luminance(image.pixelColor(36, toggle.rect().center().y()));
                    const double edgeContrast = (std::max(edgeLuminance, trackLuminance) + 0.05)
                        / (std::min(edgeLuminance, trackLuminance) + 0.05);
                    if (state == 0 && fillContrast >= 3.0) {
                        // 无需描边时，边缘只允许圆点与轨道的抗锯齿混色。
                        QVERIFY(edgeLuminance <= std::max(thumbLuminance, trackLuminance) + 0.02);
                        QVERIFY(edgeLuminance >= std::min(thumbLuminance, trackLuminance) - 0.02);
                    }
                    QVERIFY2(fillContrast >= 3.0
                            || (mode == ZzFluentUI::ZzThemeMode::Light && edgeContrast >= 3.0),
                        qPrintable(QStringLiteral("mode=%1 accent=%2 state=%3")
                            .arg(static_cast<int>(mode)).arg(accent.name(QColor::HexArgb)).arg(state)));
                    if (state == 0) {
                        normalThumb = thumb;
                    } else {
                        QCOMPARE(thumb, normalThumb);
                    }
                    if (accent == QColor("#0067c0")) {
                        QCOMPARE(thumb, mode == ZzFluentUI::ZzThemeMode::Light
                            ? QColor(Qt::white) : QColor(Qt::black));
                    }
                    if (mode == ZzFluentUI::ZzThemeMode::Dark && accent == QColor("#36a6ff")) {
                        QCOMPARE(thumb, toggle.palette().color(QPalette::Window));
                    }
                }
            }
        }
    }

    /** @brief 按下提供视觉反馈，单独改变按下状态不修改选中真值。 */
    void pressedThumbHasVisibleFeedback()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.resize(toggle.sizeHint());
        const auto render = [&] {
            QImage image(toggle.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            toggle.render(&painter);
            return image;
        };
        const auto normal = render();
        toggle.setDown(true);
        QVERIFY(normal != render());
        toggle.setDown(false);
        QCOMPARE(render(), normal);
        QVERIFY(!toggle.isChecked());
    }

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

    void entireTrackAndLabelAreClickable()
    {
        ZzFluentUI::ZzToggleSwitch toggle(QStringLiteral("Wi-Fi"));
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        QSignalSpy spy(&toggle, &QCheckBox::toggled);

        QTest::mouseClick(
            &toggle,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(38, toggle.rect().center().y()));
        QVERIFY(toggle.isChecked());

        QTest::mouseClick(
            &toggle,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(toggle.width() - 2, toggle.rect().center().y()));
        QVERIFY(!toggle.isChecked());
        QCOMPARE(spy.count(), 2);
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
        const qsizetype before =
            toggle.findChildren<QVariantAnimation *>().size();

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

        QVERIFY(QTest::qWaitFor([animation] {
            return animation->state() == QAbstractAnimation::Stopped;
        }));
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
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
