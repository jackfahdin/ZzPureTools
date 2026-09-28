#include <array>

#include <QtCore/QCoreApplication>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStyleOptionToolButton>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzControlAppearance.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzSplitButton.h>
#include <ZzFluentUI/ZzThemeController.h>

/** @brief 验证 Fluent 按钮的外观、图标缓存和 Qt 原生激活语义。 */
class ZzButtonControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void typedAppearanceCanResetDynamicAccent()
    {
        using ZzFluentUI::ZzButtonAppearance;
        using ZzFluentUI::ZzControlAppearance;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzPushButton push;
        ZzFluentUI::ZzSplitButton split;
        ZzFluentUI::ZzIconButton icon;
        const std::array<QAbstractButton *, 3> buttons{&push, &split, &icon};
        for (QAbstractButton *button : buttons) {
            button->setStyle(&style);
            button->resize(120, 36);
            button->setProperty("accent", true);
            QCOMPARE(ZzControlAppearance::buttonAppearance(button), ZzButtonAppearance::Accent);
            ZzControlAppearance::setButtonAppearance(button, ZzButtonAppearance::Standard);
            QCOMPARE(ZzControlAppearance::buttonAppearance(button), ZzButtonAppearance::Standard);
            QVERIFY(!button->property("accent").toBool());
            ZzControlAppearance::setButtonAppearance(button, ZzButtonAppearance::Accent);
            button->setProperty("accent", false);
            QCOMPARE(ZzControlAppearance::buttonAppearance(button), ZzButtonAppearance::Standard);
        }
    }

    void accentToolButtonIncludesMenuSurfaceAndArrow()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QToolButton button;
        button.setStyle(&style);
        button.setProperty("accent", true);
        const QColor accent(QStringLiteral("#402060"));
        ZzFluentUI::ZzControlAppearance::setAccentColor(&button, accent);
        QStyleOptionToolButton option;
        option.initFrom(&button);
        option.rect = QRect(0, 0, 120, 36);
        option.state = QStyle::State_Enabled;
        option.subControls = QStyle::SC_ToolButton | QStyle::SC_ToolButtonMenu;
        option.features = QStyleOptionToolButton::MenuButtonPopup | QStyleOptionToolButton::HasMenu;
        const QRect menu = style.subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButtonMenu, &button);
        QVERIFY(!menu.isEmpty());
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawComplexControl(QStyle::CC_ToolButton, &option, &painter, &button);
        painter.end();
        QCOMPARE(image.pixelColor(menu.center().x(), menu.top() + 6), accent);
        bool hasLightArrow = false;
        for (int y = menu.center().y() - 4; y <= menu.center().y() + 4; ++y) {
            for (int x = menu.left() + 3; x <= menu.right() - 3; ++x) {
                // Fusion 对箭头应用 160/255 的透明度；验证混合后的浅色箭头，
                // 不要求输出为不透明白色。
                hasLightArrow |= image.pixelColor(x, y).lightness() > 150;
            }
        }
        QVERIFY(hasLightArrow);
    }

    void localAccentSurvivesThemeChangeAndResetsOnlyItsRoles()
    {
        using ZzFluentUI::ZzControlAppearance;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget parent;
        parent.setStyle(&style);
        QPushButton button(&parent);
        QPushButton sibling(&parent);
        QPalette palette = button.palette();
        palette.setColor(QPalette::Base, Qt::cyan);
        button.setPalette(palette);
        const QColor local(QStringLiteral("#743ab5"));
        ZzControlAppearance::setAccentColor(&button, local);
        QCOMPARE(ZzControlAppearance::accentColor(&button), local);
        QCOMPARE(button.palette().color(QPalette::Highlight), local);
        QCOMPARE(button.palette().color(QPalette::HighlightedText), QColor(Qt::white));
        QVERIFY(ZzControlAppearance::accentColor(&sibling) != local);
        controller.setAccentColor(QColor(QStringLiteral("#148240")));
        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCoreApplication::processEvents();
        QCOMPARE(ZzControlAppearance::accentColor(&button), local);
        QCOMPARE(ZzControlAppearance::accentColor(&sibling), controller.accentColor());
        ZzControlAppearance::resetAccentColor(&button);
        QCOMPARE(ZzControlAppearance::accentColor(&button), controller.accentColor());
        QCOMPARE(button.palette().color(QPalette::Base), QColor(Qt::cyan));
        QCOMPARE(button.palette().color(QPalette::HighlightedText), sibling.palette().color(QPalette::HighlightedText));
        ZzControlAppearance::setAccentColor(&parent, Qt::yellow);
        QCOMPARE(ZzControlAppearance::accentColor(&button), QColor(Qt::yellow));
        ZzControlAppearance::setAccentColor(&button, Qt::red);
        ZzControlAppearance::setAccentColor(&button, {});
        QCOMPARE(ZzControlAppearance::accentColor(&button), QColor(Qt::yellow));
    }

    void accentIconButtonPaintsIdleSurfaceAndContrastingIcon()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzIconButton button;
        button.setStyle(&style);
        button.resize(36, 36);
        button.setIconDescriptor({QStringLiteral(":/zzfluent/buttons/ZzFluentTestSquare.svg"), true});
        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        ZzFluentUI::ZzControlAppearance::setAccentColor(&button, QColor(Qt::yellow));
        const auto render = [&] {
            QImage image(button.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            button.render(&painter);
            return image;
        };
        const QImage yellow = render();
        QCOMPARE(yellow.pixelColor(3, 18), QColor(Qt::yellow));
        QCOMPARE(yellow.pixelColor(18, 18), QColor(Qt::black));
        ZzFluentUI::ZzControlAppearance::setAccentColor(&button, QColor(Qt::blue));
        const QImage blue = render();
        QCOMPARE(blue.pixelColor(3, 18), QColor(Qt::blue));
        QCOMPARE(blue.pixelColor(18, 18), QColor(Qt::white));
        button.setIconColor(Qt::green);
        QCOMPARE(render().pixelColor(18, 18), QColor(Qt::green));
        button.resetIconColor();
        button.setEnabled(false);
        QVERIFY(render().pixelColor(3, 18) != QColor(Qt::blue));
    }

    void nativeButtonsUseAccentProperty_data()
    {
        QTest::addColumn<bool>("toolButton");
        QTest::addColumn<QColor>("accent");
        QTest::newRow("push-purple") << false << QColor(QStringLiteral("#8752b5"));
        QTest::newRow("tool-purple") << true << QColor(QStringLiteral("#8752b5"));
        QTest::newRow("push-black") << false << QColor(Qt::black);
        QTest::newRow("tool-white") << true << QColor(Qt::white);
    }

    void nativeButtonsUseAccentProperty()
    {
        QFETCH(bool, toolButton);
        QFETCH(QColor, accent);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QPushButton push;
        QToolButton tool;
        QWidget *button = toolButton ? static_cast<QWidget *>(&tool) : &push;
        button->setStyle(&style);
        button->setProperty("accent", true);
        button->resize(120, 36);
        QPalette palette = button->palette();
        palette.setColor(QPalette::Accent, accent);
        button->setPalette(palette);
        const auto sample = [&](QStyle::State state) {
            QImage image(button->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            QStyleOptionButton option;
            option.initFrom(button);
            option.state = state;
            if (toolButton) {
                style.drawPrimitive(QStyle::PE_PanelButtonTool, &option, &painter, button);
            } else {
                style.drawControl(QStyle::CE_PushButton, &option, &painter, button);
            }
            painter.end();
            return image.pixelColor(10, 18);
        };
        const QColor normal = sample(QStyle::State_Enabled);
        const QColor hover = sample(QStyle::State_Enabled | QStyle::State_MouseOver);
        const QColor pressed = sample(QStyle::State_Enabled | QStyle::State_Sunken);
        QCOMPARE(normal, accent);
        QVERIFY(hover != normal);
        QVERIFY(pressed != normal);
        QVERIFY(pressed != hover);
        QVERIFY(sample(QStyle::State_None) != accent);
    }

    void checkablePushButtonPreservesToggleSemantics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QDialog dialog;
        dialog.setStyle(&style);
        ZzFluentUI::ZzPushButton button(
            QStringLiteral("Pin preview"),
            &dialog);
        button.setStyle(&style);
        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Subtle);
        button.setCheckable(true);
        button.setAccessibleName(QStringLiteral("Pin preview"));
        button.resize(140, 36);
        button.setDefault(true);
        dialog.resize(180, 80);
        dialog.show();
        button.show();
        QCoreApplication::processEvents();

        QSignalSpy toggledSpy(&button, &QAbstractButton::toggled);
        QSignalSpy clickedSpy(&button, &QAbstractButton::clicked);
        QCOMPARE(button.isCheckable(), true);
        QCOMPARE(button.isChecked(), false);

        button.setChecked(true);
        QCOMPARE(button.isChecked(), true);
        QCOMPARE(toggledSpy.count(), 1);
        button.setChecked(true);
        QCOMPARE(toggledSpy.count(), 1);

        button.setFocus();
        QTest::keyClick(&button, Qt::Key_Space);
        QCOMPARE(button.isChecked(), false);
        QCOMPARE(toggledSpy.count(), 2);
        QCOMPARE(clickedSpy.count(), 1);
        QTest::keyClick(&button, Qt::Key_Return);
        QCOMPARE(button.isChecked(), true);
        QCOMPARE(toggledSpy.count(), 3);
        QCOMPARE(clickedSpy.count(), 2);

        button.setEnabled(false);
        QTest::keyClick(&button, Qt::Key_Space);
        QCOMPARE(button.isChecked(), true);
        QCOMPARE(clickedSpy.count(), 2);
        button.setEnabled(true);

        const qsizetype childCount = button.children().size();
        const QSize stableSize = button.sizeHint();
        for (int index = 0; index < 1000; ++index) {
            button.setChecked((index % 2) == 0);
        }
        QCOMPARE(button.children().size(), childCount);
        QCOMPARE(button.sizeHint(), stableSize);

        QAccessibleInterface *accessible =
            QAccessible::queryAccessibleInterface(&button);
        QVERIFY(accessible != nullptr);
        if (accessible != nullptr) {
            QCOMPARE(accessible->role(), QAccessible::CheckBox);
            QCOMPARE(
                accessible->text(QAccessible::Name),
                QStringLiteral("Pin preview"));
        }
    }

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

        // 本断言验证静止填充，避免离屏平台把默认光标放在按钮内而命中 hover。
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(&button, &leave);

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
