#include <algorithm>
#include <cmath>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QButtonGroup>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {
void zzMovePressedMouse(QWidget *widget, QPoint position)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(position),
        QPointF(widget->mapToGlobal(position)), Qt::NoButton,
        Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}
} // namespace

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
    void thumbPressFreezesAnExistingAnimation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, new ZzAnimationEnabledBaseStyle);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        toggle.setChecked(true);
        auto *animation = toggle.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        animation->setCurrentTime(animation->duration() / 2);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        const QPoint thumb(28, toggle.rect().center().y());
        QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier, thumb);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, thumb);
        QVERIFY(!toggle.isChecked());
    }

    void commitsDragPositionOnlyOnRelease_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("initial");
        QTest::addColumn<bool>("target");
        for (bool rtl : {false, true}) {
            for (bool initial : {false, true}) {
                for (bool target : {false, true}) {
                    const QByteArray name = QByteArray::number(rtl) + '-'
                        + QByteArray::number(initial) + '-' + QByteArray::number(target);
                    QTest::newRow(name.constData()) << rtl << initial << target;
                }
            }
        }
    }

    void canceledClickResumesFrozenAnimation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, new ZzAnimationEnabledBaseStyle);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        toggle.setChecked(true);
        auto *animation = toggle.findChild<QVariantAnimation *>();
        animation->setCurrentTime(animation->duration() / 2);
        QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier,
            QPoint(28, toggle.rect().center().y()));
        zzMovePressedMouse(&toggle, QPoint(28, -30));
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(28, -30));
        QVERIFY(toggle.isChecked());
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(animation->endValue().toReal(), 1.0);
    }

    void commitsDragPositionOnlyOnRelease()
    {
        QFETCH(bool, rtl);
        QFETCH(bool, initial);
        QFETCH(bool, target);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        toggle.setChecked(initial);
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        QSignalSpy toggled(&toggle, &QCheckBox::toggled);
        QSignalSpy pressed(&toggle, &QCheckBox::pressed);
        QSignalSpy released(&toggle, &QCheckBox::released);
        QSignalSpy clicked(&toggle, &QCheckBox::clicked);
        QButtonGroup group;
        group.setExclusive(false);
        group.addButton(&toggle, 7);
        QSignalSpy groupClicked(&group, &QButtonGroup::idClicked);
        QSignalSpy groupReleased(&group, &QButtonGroup::idReleased);
        const int y = toggle.rect().center().y();
        const auto position = [rtl, y](bool checked) {
            return QPoint((checked != rtl) ? 30 : 9, y);
        };
        QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier, position(initial));
        zzMovePressedMouse(&toggle, position(!initial));
        QCOMPARE(toggle.isChecked(), initial);
        QCOMPARE(toggled.count(), 0);
        // Returning to the original side is not an ordinary click. Releasing
        // beyond the track must still commit the selected side under capture.
        const QPoint end((target != rtl) ? 70 : -30, y);
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(toggle.isChecked(), target);
        QCOMPARE(toggled.count(), initial == target ? 0 : 1);
        QCOMPARE(pressed.count(), 1);
        QCOMPARE(released.count(), 1);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.first().first().toBool(), target);
        QCOMPARE(groupClicked.count(), 1);
        QCOMPARE(groupReleased.count(), 1);
        QCOMPARE(groupClicked.first().first().toInt(), 7);
        QVERIFY(!toggle.isDown());
        QCOMPARE(toggle.findChildren<QVariantAnimation *>().size(), 1);
    }

    void cancelsInterruptedDrag_data()
    {
        QTest::addColumn<int>("interruption");
        QTest::newRow("hide") << 0;
        QTest::newRow("disable") << 1;
        QTest::newRow("ungrab") << 2;
        QTest::newRow("deactivate") << 3;
        QTest::newRow("resize") << 4;
        QTest::newRow("margins") << 5;
        QTest::newRow("rtl") << 6;
        QTest::newRow("focus-out") << 7;
        QTest::newRow("programmatic-check") << 8;
    }

    void cancelsInterruptedDrag()
    {
        QFETCH(int, interruption);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        QSignalSpy clicked(&toggle, &QCheckBox::clicked);
        const int y = toggle.rect().center().y();
        QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(9, y));
        zzMovePressedMouse(&toggle, QPoint(30, y));
        if (interruption == 0) {
            toggle.hide();
            toggle.show();
        } else if (interruption == 1) {
            toggle.setEnabled(false);
            toggle.setEnabled(true);
        } else if (interruption == 4) {
            toggle.resize(toggle.width() + 20, toggle.height());
        } else if (interruption == 5) {
            toggle.setContentsMargins(3, 0, 0, 0);
        } else if (interruption == 6) {
            toggle.setLayoutDirection(Qt::RightToLeft);
        } else if (interruption == 7) {
            toggle.clearFocus();
        } else if (interruption == 8) {
            toggle.setChecked(true);
        } else {
            QEvent event(interruption == 2 ? QEvent::UngrabMouse : QEvent::WindowDeactivate);
            QCoreApplication::sendEvent(&toggle, &event);
        }
        zzMovePressedMouse(&toggle, QPoint(25, y));
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(30, y));
        QCOMPARE(toggle.isChecked(), interruption == 8);
        QCOMPARE(clicked.count(), 0);
        QVERIFY(!toggle.isDown());
    }

    void dragReceiverCanDeleteControl()
    {
        auto *toggle = new ZzFluentUI::ZzToggleSwitch;
        QPointer<ZzFluentUI::ZzToggleSwitch> guard(toggle);
        toggle->resize(toggle->sizeHint());
        toggle->show();
        QCoreApplication::processEvents();
        connect(toggle, &QCheckBox::toggled, this, [toggle] { delete toggle; });
        const int y = toggle->rect().center().y();
        QTest::mousePress(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(9, y));
        zzMovePressedMouse(toggle, QPoint(70, y));
        QTest::mouseRelease(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(70, y));
        QVERIFY(guard.isNull());
    }

    void disableDuringThumbPressCancelsWithoutRelease()
    {
        auto *toggle = new ZzFluentUI::ZzToggleSwitch;
        const QPointer<ZzFluentUI::ZzToggleSwitch> guard(toggle);
        toggle->resize(toggle->sizeHint());
        toggle->show();
        QCoreApplication::processEvents();
        connect(toggle, &QCheckBox::released, this, [toggle] { delete toggle; });
        QTest::mousePress(toggle, Qt::LeftButton, Qt::NoModifier,
            QPoint(9, toggle->rect().center().y()));
        toggle->setEnabled(false);
        // Cancellation must not invoke release callbacks within Qt's focus
        // transition. Real mouse releases still deliver native notifications.
        QVERIFY(!guard.isNull());
        QVERIFY(!toggle->isDown());
        QVERIFY(!toggle->isChecked());
        delete toggle;
    }

    void exclusiveGroupKeepsItsCheckedButton()
    {
        ZzFluentUI::ZzToggleSwitch toggle;
        QButtonGroup group;
        group.addButton(&toggle);
        toggle.setChecked(true);
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        QSignalSpy toggled(&toggle, &QCheckBox::toggled);
        const int y = toggle.rect().center().y();
        QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(30, y));
        zzMovePressedMouse(&toggle, QPoint(-30, y));
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(-30, y));
        QVERIFY(toggle.isChecked());
        QCOMPARE(group.checkedButton(), &toggle);
        QCOMPARE(toggled.count(), 0);
    }

    void groupReleaseReceiverCanDeleteControl()
    {
        auto *toggle = new ZzFluentUI::ZzToggleSwitch;
        const QPointer<ZzFluentUI::ZzToggleSwitch> guard(toggle);
        QButtonGroup group;
        group.setExclusive(false);
        group.addButton(toggle);
        toggle->resize(toggle->sizeHint());
        toggle->show();
        QCoreApplication::processEvents();
        connect(&group, &QButtonGroup::idReleased, this, [toggle] { delete toggle; });
        const int y = toggle->rect().center().y();
        QTest::mousePress(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(9, y));
        zzMovePressedMouse(toggle, QPoint(70, y));
        QTest::mouseRelease(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(70, y));
        QVERIFY(guard.isNull());
    }

    void releaseReceiverCanStartAnotherDrag()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.resize(toggle.sizeHint());
        toggle.show();
        QCoreApplication::processEvents();
        const int y = toggle.rect().center().y();
        const auto connection = connect(&toggle, &QCheckBox::clicked, this, [&] {
            QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(30, y));
        });
        QTest::mousePress(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(9, y));
        zzMovePressedMouse(&toggle, QPoint(70, y));
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(70, y));
        disconnect(connection);
        QVERIFY(toggle.isChecked());
        zzMovePressedMouse(&toggle, QPoint(-30, y));
        QTest::mouseRelease(&toggle, Qt::LeftButton, Qt::NoModifier, QPoint(-30, y));
        QVERIFY(!toggle.isChecked());
    }

    /** @brief 浅色开启圆点保持无描边白色，深色保留对比适配，交互不改变圆点颜色。 */
    void checkedThumbFollowsThemeWithoutOutline()
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
            for (const QColor accent : {QColor("#0067c0"), QColor("#36a6ff"), QColor("#8752b5"),
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
                    // 全局与局部强调色使用相同的纯色圆点，边缘仅允许抗锯齿混色。
                    const double edgeLuminance = luminance(image.pixelColor(36, toggle.rect().center().y()));
                    QVERIFY(edgeLuminance <= std::max(thumbLuminance, trackLuminance) + 0.02);
                    QVERIFY(edgeLuminance >= std::min(thumbLuminance, trackLuminance) - 0.02);
                    QVERIFY2(mode == ZzFluentUI::ZzThemeMode::Light || fillContrast >= 3.0,
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
