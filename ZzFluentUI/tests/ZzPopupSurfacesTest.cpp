#include <QtGui/QAction>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStyleOption>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 判断图像区域是否包含接近目标值的不透明像素。 */
bool zzContainsColor(const QImage &image, const QColor &expected)
{
    constexpr int tolerance = 8;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 将菜单项绘制到透明图像供状态颜色检查。 */
QImage zzRenderMenuItem(
    ZzFluentUI::ZzFluentStyle *style,
    QStyleOptionMenuItem option,
    const QWidget *widget = nullptr)
{
    QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    style->drawControl(
        QStyle::CE_MenuItem,
        &option,
        &painter,
        widget);
    painter.end();
    return image;
}

} // namespace

/** @brief 验证标准菜单、菜单栏和工具提示的 Fluent 绘制契约。 */
class ZzPopupSurfacesTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesBaseMeasurementsAndStableMetrics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);

        QStyleOptionMenuItem item;
        item.initFrom(&menu);
        item.menuItemType = QStyleOptionMenuItem::Normal;
        item.text = QStringLiteral("Open workspace\tCtrl+Shift+O");
        item.maxIconWidth = 20;
        item.reservedShortcutWidth = 96;
        const QSize contents(220, 18);
        const QSize base = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        const QSize fluent = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            contents,
            &menu);
        QVERIFY(fluent.width() >= base.width());
        QVERIFY(fluent.height() >= base.height());
        QVERIFY(fluent.height() >= 32);

        item.menuItemType = QStyleOptionMenuItem::Separator;
        item.text.clear();
        const QSize separatorBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            QSize(120, 1),
            &menu);
        const QSize separator = style.sizeFromContents(
            QStyle::CT_MenuItem,
            &item,
            QSize(120, 1),
            &menu);
        QVERIFY(separator.width() >= separatorBase.width());
        QVERIFY(separator.height() >= separatorBase.height());
        QVERIFY(separator.height() >= 9);
        QVERIFY(separator.height() < fluent.height());

        QMenuBar menuBar;
        menuBar.setNativeMenuBar(false);
        menuBar.setStyle(&style);
        item.menuItemType = QStyleOptionMenuItem::Normal;
        item.text = QStringLiteral("&File");
        const QSize menuBarBase = style.baseStyle()->sizeFromContents(
            QStyle::CT_MenuBarItem,
            &item,
            QSize(48, 18),
            &menuBar);
        const QSize menuBarItem = style.sizeFromContents(
            QStyle::CT_MenuBarItem,
            &item,
            QSize(48, 18),
            &menuBar);
        QVERIFY(menuBarItem.width() >= menuBarBase.width());
        QVERIFY(menuBarItem.height() >= menuBarBase.height());
        QVERIFY(menuBarItem.height() >= 32);

        QCOMPARE(style.pixelMetric(QStyle::PM_MenuPanelWidth), 1);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuHMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuVMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarHMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarVMargin), 4);
        QCOMPARE(style.pixelMetric(QStyle::PM_MenuBarItemSpacing), 2);
        QCOMPARE(style.pixelMetric(QStyle::PM_ToolTipLabelFrameWidth), 8);
    }

    void rendersEveryThemeAndInteractionState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);

        for (const ZzFluentUI::ZzThemeMode mode : {
                 ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark,
                 ZzFluentUI::ZzThemeMode::HighContrast}) {
            controller.setMode(mode);
            const auto snapshot = controller.snapshot();
            const QPalette palette = style.standardPalette();

            QStyleOption panel;
            panel.rect = QRect(0, 0, 220, 140);
            panel.state = QStyle::State_Enabled;
            panel.palette = palette;
            QImage image(panel.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            style.drawPrimitive(
                QStyle::PE_PanelMenu,
                &panel,
                &painter,
                &menu);
            painter.end();
            QCOMPARE(
                image.pixelColor(panel.rect.center()),
                snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

            QStyleOptionMenuItem item;
            item.rect = QRect(0, 0, 220, 32);
            item.state = QStyle::State_Enabled | QStyle::State_Selected;
            item.palette = palette;
            item.menuItemType = QStyleOptionMenuItem::Normal;
            item.checkType = QStyleOptionMenuItem::NonExclusive;
            item.checked = true;
            item.menuHasCheckableItems = true;
            item.maxIconWidth = 20;
            item.text = QStringLiteral("Checked\tCtrl+C");
            item.reservedShortcutWidth = 64;
            image = zzRenderMenuItem(&style, item, &menu);
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlFillHover)));
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::Accent)));

            item.state = QStyle::State_Enabled | QStyle::State_Sunken;
            item.checked = false;
            image = zzRenderMenuItem(&style, item, &menu);
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlFillPressed)));

            item.state = QStyle::State_None
                | QStyle::State_Selected;
            item.text.clear();
            image = zzRenderMenuItem(&style, item, &menu);
            if (mode != ZzFluentUI::ZzThemeMode::HighContrast) {
                QVERIFY(!zzContainsColor(
                    image,
                    snapshot->color(
                        ZzFluentUI::ZzColorToken::ControlFillHover)));
            }

            item.menuItemType = QStyleOptionMenuItem::Separator;
            item.text.clear();
            item.rect = QRect(0, 0, 220, 9);
            image = zzRenderMenuItem(&style, item, &menu);
            QVERIFY(zzContainsColor(
                image,
                snapshot->color(ZzFluentUI::ZzColorToken::ControlStroke)));

            QStyleOptionMenuItem menuBarItem;
            menuBarItem.rect = QRect(0, 0, 90, 32);
            menuBarItem.state = QStyle::State_Enabled
                | QStyle::State_Selected;
            menuBarItem.palette = palette;
            menuBarItem.menuItemType = QStyleOptionMenuItem::Normal;
            QImage menuBarImage(
                menuBarItem.rect.size(),
                QImage::Format_ARGB32_Premultiplied);
            menuBarImage.fill(Qt::transparent);
            painter.begin(&menuBarImage);
            style.drawControl(
                QStyle::CE_MenuBarItem,
                &menuBarItem,
                &painter);
            painter.end();
            QCOMPARE(
                menuBarImage.pixelColor(menuBarItem.rect.center()),
                snapshot->color(ZzFluentUI::ZzColorToken::ControlFillHover));

            QStyleOption toolTip;
            toolTip.rect = QRect(0, 0, 180, 40);
            toolTip.state = QStyle::State_Enabled;
            toolTip.palette = palette;
            QImage toolTipImage(
                toolTip.rect.size(),
                QImage::Format_ARGB32_Premultiplied);
            toolTipImage.fill(Qt::transparent);
            painter.begin(&toolTipImage);
            style.drawPrimitive(
                QStyle::PE_PanelTipLabel,
                &toolTip,
                &painter);
            painter.end();
            QCOMPARE(
                toolTipImage.pixelColor(toolTip.rect.center()),
                snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
            QCOMPARE(
                palette.color(QPalette::ToolTipBase),
                snapshot->color(ZzFluentUI::ZzColorToken::SurfaceSecondary));
        }
    }

    void mirrorsSubmenuChevronWithoutChangingActionState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QMenu menu;
        menu.setStyle(&style);
        const QColor textColor = style.standardPalette().color(
            QPalette::Text);

        QStyleOptionMenuItem item;
        item.rect = QRect(0, 0, 220, 32);
        item.state = QStyle::State_Enabled;
        item.palette = style.standardPalette();
        item.menuItemType = QStyleOptionMenuItem::SubMenu;
        item.text = QStringLiteral("Export");
        item.direction = Qt::LeftToRight;
        const QImage ltr = zzRenderMenuItem(&style, item, &menu);
        QVERIFY(zzContainsColor(ltr.copy(QRect(192, 6, 24, 20)), textColor));

        item.direction = Qt::RightToLeft;
        const QImage rtl = zzRenderMenuItem(&style, item, &menu);
        QVERIFY(zzContainsColor(rtl.copy(QRect(4, 6, 24, 20)), textColor));

        QAction action(QStringLiteral("Export"), &menu);
        action.setCheckable(true);
        action.setChecked(true);
        action.setData(42);
        action.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
        QSignalSpy toggledSpy(&action, &QAction::toggled);
        QSignalSpy triggeredSpy(&action, &QAction::triggered);
        action.trigger();
        QCOMPARE(action.text(), QStringLiteral("Export"));
        QCOMPARE(action.data().toInt(), 42);
        QCOMPARE(action.shortcut(), QKeySequence(Qt::CTRL | Qt::Key_E));
        QVERIFY(!action.isChecked());
        QCOMPARE(toggledSpy.count(), 1);
        QCOMPARE(triggeredSpy.count(), 1);
    }
};

QTEST_MAIN(ZzPopupSurfacesTest)

#include "ZzPopupSurfacesTest.moc"
