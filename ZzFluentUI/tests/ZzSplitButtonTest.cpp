#include <memory>

#include <QtCore/QCoreApplication>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzSplitButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 创建应用级 Fluent style 供绘制和命中测试复用。 */
std::unique_ptr<ZzFluentUI::ZzFluentStyle> zzCreateStyle(
    ZzFluentUI::ZzThemeController *controller)
{
    std::unique_ptr<QStyle> fusion(
        QStyleFactory::create(QStringLiteral("Fusion")));
    Q_ASSERT(fusion != nullptr);
    return std::make_unique<ZzFluentUI::ZzFluentStyle>(
        controller,
        fusion.release());
}

/** @brief 判断图像中是否存在接近目标颜色的不透明像素。 */
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

/** @brief 关闭菜单并处理其 aboutToHide 状态更新。 */
void zzCloseMenu(QMenu *menu)
{
    Q_ASSERT(menu != nullptr);
    menu->hide();
    QCoreApplication::processEvents();
}

} // namespace

/** @brief 验证 SplitButton 的双命中、菜单借用和原生按钮语义。 */
class ZzSplitButtonTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void propertiesAreIdempotentAndMenuIsBorrowed()
    {
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Build"));
        QMenu menu;
        QSignalSpy appearanceSpy(
            &button,
            &ZzFluentUI::ZzSplitButton::appearanceChanged);
        QSignalSpy menuSpy(
            &button,
            &ZzFluentUI::ZzSplitButton::menuChanged);

        QCOMPARE(
            button.appearance(),
            ZzFluentUI::ZzButtonAppearance::Standard);
        QCOMPARE(button.menu(), nullptr);
        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        button.setMenu(&menu);
        button.setMenu(&menu);

        QCOMPARE(
            button.appearance(),
            ZzFluentUI::ZzButtonAppearance::Accent);
        QCOMPARE(button.menu(), &menu);
        QCOMPARE(menu.parent(), nullptr);
        QCOMPARE(appearanceSpy.count(), 1);
        QCOMPARE(menuSpy.count(), 1);

        button.setMenu(nullptr);
        QCOMPARE(button.menu(), nullptr);
        QCOMPARE(menuSpy.count(), 2);
    }

    void externalMenuDestructionClearsPointerOnce()
    {
        ZzFluentUI::ZzSplitButton button;
        auto *menu = new QMenu;
        button.setMenu(menu);
        QSignalSpy menuSpy(
            &button,
            &ZzFluentUI::ZzSplitButton::menuChanged);

        delete menu;
        QCOMPARE(button.menu(), nullptr);
        QCOMPARE(menuSpy.count(), 1);
        QCOMPARE(menuSpy.at(0).at(0).value<QMenu *>(), nullptr);
    }

    void mouseRegionsKeepMainAndMenuActionsSeparate()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = zzCreateStyle(&controller);
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Build"));
        QMenu menu;
        menu.addAction(QStringLiteral("Clean build"));
        button.setStyle(style.get());
        button.setMenu(&menu);
        button.resize(180, 40);
        button.show();
        QCoreApplication::processEvents();
        QSignalSpy clickedSpy(&button, &QPushButton::clicked);
        QSignalSpy requestedSpy(
            &button,
            &ZzFluentUI::ZzSplitButton::menuRequested);

        QTest::mouseClick(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(40, 20));
        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(requestedSpy.count(), 0);

        QTest::mouseClick(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(button.width() - 12, 20));
        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(requestedSpy.count(), 1);
        QTRY_VERIFY(menu.isVisible());
        zzCloseMenu(&menu);

        QTest::mousePress(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(40, 20));
        QTest::mouseRelease(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(button.width() - 12, 20));
        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(requestedSpy.count(), 1);

        QTest::mousePress(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(button.width() - 12, 20));
        QTest::mouseRelease(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(40, 20));
        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(requestedSpy.count(), 1);

        button.setLayoutDirection(Qt::RightToLeft);
        QTest::mouseClick(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(12, 20));
        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(requestedSpy.count(), 2);
        QTRY_VERIFY(menu.isVisible());
        zzCloseMenu(&menu);

        QTest::mouseClick(
            &button,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(button.width() - 40, 20));
        QCOMPARE(clickedSpy.count(), 2);
        QCOMPARE(requestedSpy.count(), 2);
    }

    void menuRequestedCanAssembleMenuSynchronously()
    {
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Export"));
        QMenu menu;
        button.resize(160, 40);
        button.show();
        QObject::connect(
            &button,
            &ZzFluentUI::ZzSplitButton::menuRequested,
            &button,
            [&button, &menu] {
                if (button.menu() == nullptr) {
                    menu.addAction(QStringLiteral("PDF"));
                    button.setMenu(&menu);
                }
            });

        button.showMenu();
        QCOMPARE(button.menu(), &menu);
        QCOMPARE(menu.actions().size(), 1);
        QTRY_VERIFY(menu.isVisible());
        zzCloseMenu(&menu);
    }

    void keyboardKeepsMenuAndMainActivationSeparate()
    {
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Run"));
        QMenu menu;
        menu.addAction(QStringLiteral("Run with options"));
        button.setMenu(&menu);
        button.resize(160, 40);
        button.show();
        button.setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        QSignalSpy clickedSpy(&button, &QPushButton::clicked);
        QSignalSpy requestedSpy(
            &button,
            &ZzFluentUI::ZzSplitButton::menuRequested);

        QTest::keyClick(&button, Qt::Key_Down);
        QCOMPARE(requestedSpy.count(), 1);
        QCOMPARE(clickedSpy.count(), 0);
        QTRY_VERIFY(menu.isVisible());
        zzCloseMenu(&menu);

        QTest::keyClick(
            &button,
            Qt::Key_Down,
            Qt::AltModifier);
        QCOMPARE(requestedSpy.count(), 2);
        QCOMPARE(clickedSpy.count(), 0);
        QTRY_VERIFY(menu.isVisible());
        zzCloseMenu(&menu);

        QTest::keyClick(&button, Qt::Key_Space);
        QTest::keyClick(&button, Qt::Key_Return);
        QCOMPARE(clickedSpy.count(), 2);
        QCOMPARE(requestedSpy.count(), 2);

        button.setEnabled(false);
        button.showMenu();
        QCOMPARE(requestedSpy.count(), 2);
    }

    void popupAnchorFollowsLogicalLeadingEdge()
    {
        QWidget host;
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Publish"), &host);
        QMenu menu;
        menu.setFixedWidth(140);
        menu.addAction(QStringLiteral("Preview release"));
        button.setMenu(&menu);
        button.setGeometry(60, 40, 180, 40);
        host.setGeometry(240, 180, 360, 180);
        host.show();
        QCoreApplication::processEvents();

        button.setLayoutDirection(Qt::LeftToRight);
        const int ltrExpected = button.mapToGlobal(QPoint(0, 0)).x();
        button.showMenu();
        QTRY_VERIFY(menu.isVisible());
        QCOMPARE(menu.geometry().left(), ltrExpected);
        zzCloseMenu(&menu);

        button.setLayoutDirection(Qt::RightToLeft);
        const int rtlExpected = button.mapToGlobal(
            QPoint(button.width(), 0)).x() - menu.width();
        button.showMenu();
        QTRY_VERIFY(menu.isVisible());
        QCOMPARE(menu.geometry().left(), rtlExpected);
        zzCloseMenu(&menu);
    }

    void preservesDefaultFocusAndAccessibilitySemantics()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = zzCreateStyle(&controller);
        QWidget host;
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Apply"), &host);
        QMenu menu;
        menu.addAction(QStringLiteral("Apply to all"));
        button.setStyle(style.get());
        button.setMenu(&menu);
        button.setAccessibleName(QStringLiteral("Apply changes"));
        button.setDefault(true);
        button.setGeometry(20, 20, 180, 40);
        host.resize(220, 80);
        host.show();
        host.activateWindow();
        QCoreApplication::processEvents();
        QTest::keyPress(&button, Qt::Key_Tab);
        button.setFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();

        QVERIFY(button.isDefault());
        QVERIFY(button.hasFocus());
        QVERIFY(style->isFocusVisualVisible(&button));
        QAccessibleInterface *buttonInterface =
            QAccessible::queryAccessibleInterface(&button);
        QAccessibleInterface *menuInterface =
            QAccessible::queryAccessibleInterface(&menu);
        QVERIFY(buttonInterface != nullptr);
        QVERIFY(menuInterface != nullptr);
        QCOMPARE(buttonInterface->role(), QAccessible::Button);
        QCOMPARE(menuInterface->role(), QAccessible::PopupMenu);
        QCOMPARE(
            buttonInterface->text(QAccessible::Name),
            QStringLiteral("Apply changes"));

        QImage image(button.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        button.render(&painter);
        painter.end();
        QVERIFY(zzContainsColor(
            image,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::FocusStroke)));
    }

    void repeatedOpenCloseKeepsFixedButtonObjectBudget()
    {
        ZzFluentUI::ZzSplitButton button(QStringLiteral("Build"));
        QMenu menu;
        menu.addAction(QStringLiteral("Clean"));
        button.setMenu(&menu);
        button.resize(160, 40);
        button.show();
        QCoreApplication::processEvents();
        const qsizetype objectCount =
            button.findChildren<QObject *>().size();
        const bool offscreen =
            QGuiApplication::platformName() == QStringLiteral("offscreen");

        for (int iteration = 0; iteration < 1000; ++iteration) {
            if (offscreen) {
                QTest::ignoreMessage(
                    QtWarningMsg,
                    "This plugin does not support raise()");
                QTest::ignoreMessage(
                    QtWarningMsg,
                    "This plugin does not support grabbing the keyboard");
            }
            button.showMenu();
            menu.hide();
        }
        QCoreApplication::processEvents();

        QCOMPARE(button.findChildren<QObject *>().size(), objectCount);
        QCOMPARE(button.menu(), &menu);
        QVERIFY(!menu.isVisible());
    }
};

QTEST_MAIN(ZzSplitButtonTest)
#include "ZzSplitButtonTest.moc"
