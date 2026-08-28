#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzDrawer.h>
#include <ZzFluentUI/ZzDrawerEdge.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

/** @brief 验证 Drawer 的所有权、几何、输入、焦点和固定动画契约。 */
class ZzDrawerTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 返回固定面板内容宿主。 */
    static QWidget *panelHost(ZzFluentUI::ZzDrawer *drawer)
    {
        auto *panel = drawer->findChild<QWidget *>(
            QStringLiteral("zzDrawerPanelHost"));
        Q_ASSERT(panel != nullptr);
        return panel;
    }

    /** @brief 显示固定尺寸宿主并完成初始布局。 */
    static void showHost(QWidget *host, const QSize &size = {600, 400})
    {
        host->resize(size);
        host->show();
        host->activateWindow();
        QCoreApplication::processEvents();
    }

private Q_SLOTS:
    void defaultsNormalizePropertiesAndIgnoreOpenWithoutHost()
    {
        ZzFluentUI::ZzDrawer drawer;
        QSignalSpy edgeSpy(&drawer, &ZzFluentUI::ZzDrawer::edgeChanged);
        QSignalSpy modalSpy(&drawer, &ZzFluentUI::ZzDrawer::modalChanged);
        QSignalSpy widthSpy(
            &drawer, &ZzFluentUI::ZzDrawer::widthHintChanged);
        QSignalSpy openSpy(&drawer, &ZzFluentUI::ZzDrawer::openChanged);

        QCOMPARE(drawer.edge(), ZzFluentUI::ZzDrawerEdge::Left);
        QVERIFY(drawer.isModal());
        QCOMPARE(drawer.widthHint(), 0);
        QVERIFY(!drawer.isOpen());
        QVERIFY(!drawer.isVisible());

        drawer.setEdge(ZzFluentUI::ZzDrawerEdge::Right);
        drawer.setEdge(ZzFluentUI::ZzDrawerEdge::Right);
        drawer.setModal(false);
        drawer.setModal(false);
        drawer.setWidthHint(-12);
        drawer.setWidthHint(5000);
        drawer.setWidthHint(5000);
        drawer.openDrawer();

        QCOMPARE(edgeSpy.count(), 1);
        QCOMPARE(modalSpy.count(), 1);
        QCOMPARE(widthSpy.count(), 1);
        QCOMPARE(drawer.widthHint(), 4096);
        QCOMPARE(openSpy.count(), 0);
        QVERIFY(!drawer.isOpen());
        QVERIFY(!drawer.isVisible());
    }

    // Qt parent 接管嵌套控件，QPointer 断言覆盖真实析构路径。
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    void transfersReplacesAndObservesContentOwnership()
    {
        QPointer<QWidget> ownedGuard;
        {
            QWidget host;
            ZzFluentUI::ZzDrawer drawer(&host);
            QSignalSpy contentSpy(
                &drawer, &ZzFluentUI::ZzDrawer::contentWidgetChanged);
            auto *first = new QWidget;
            auto *nested = new QLabel(QStringLiteral("Nested"), first);
            QPointer<QWidget> firstGuard(first);
            drawer.setContentWidget(first);
            drawer.setContentWidget(first);
            QCOMPARE(contentSpy.count(), 1);

            drawer.setContentWidget(nested);
            QVERIFY(firstGuard.isNull());
            QCOMPARE(drawer.contentWidget(), nested);
            QCOMPARE(nested->parentWidget(), panelHost(&drawer));
            QCOMPARE(contentSpy.count(), 2);

            std::unique_ptr<QWidget> taken(drawer.takeContentWidget());
            QCOMPARE(taken.get(), nested);
            QCOMPARE(taken->parentWidget(), nullptr);
            QCOMPARE(contentSpy.count(), 3);

            auto *externallyDestroyed = new QLabel(QStringLiteral("Owned"));
            drawer.setContentWidget(externallyDestroyed);
            delete externallyDestroyed;
            QCOMPARE(drawer.contentWidget(), nullptr);
            QCOMPARE(contentSpy.count(), 5);

            auto *owned = new QLabel(QStringLiteral("Destroyed with owner"));
            ownedGuard = owned;
            drawer.setContentWidget(owned);
        }
        QVERIFY(ownedGuard.isNull());
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void followsPhysicalEdgesHostResizeAndThemeDefaultWidth()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget host;
        ZzFluentUI::ZzDrawer drawer(&host);
        drawer.setStyle(&style);
        drawer.setContentWidget(new QLabel(QStringLiteral("Content")));
        drawer.setWidthHint(180);
        showHost(&host);

        drawer.openDrawer();
        QCOMPARE(drawer.geometry(), host.rect());
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(0, 0, 180, 400));
        drawer.closeDrawer();
        QVERIFY(!drawer.isVisible());

        drawer.setEdge(ZzFluentUI::ZzDrawerEdge::Right);
        drawer.openDrawer();
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(420, 0, 180, 400));
        host.resize(700, 420);
        QCoreApplication::processEvents();
        QCOMPARE(drawer.geometry(), host.rect());
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(520, 0, 180, 420));

        drawer.setWidthHint(0);
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(380, 0, 320, 420));
        host.resize(200, 300);
        QCoreApplication::processEvents();
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(0, 0, 200, 300));

        drawer.setLayoutDirection(Qt::RightToLeft);
        drawer.setEdge(ZzFluentUI::ZzDrawerEdge::Left);
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(0, 0, 200, 300));
    }

    void paintsModalScrimAndUsesPanelOnlyNonModalMask()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget host;
        auto *underlying = new QPushButton(QStringLiteral("Underlying"), &host);
        underlying->setGeometry(420, 160, 120, 40);
        ZzFluentUI::ZzDrawer drawer(&host);
        drawer.setStyle(&style);
        drawer.setWidthHint(180);
        drawer.setContentWidget(new QLabel(QStringLiteral("Panel")));
        showHost(&host);

        drawer.openDrawer();
        QImage image(drawer.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        drawer.render(&painter);
        painter.end();
        QVERIFY(image.pixelColor(500, 200).alpha() > 0);

        QTest::mouseClick(
            &drawer, Qt::LeftButton, Qt::NoModifier, QPoint(500, 200));
        QVERIFY(!drawer.isOpen());
        QVERIFY(!drawer.isVisible());

        drawer.setModal(false);
        drawer.openDrawer();
        QVERIFY(drawer.mask().contains(QPoint(90, 200)));
        QVERIFY(!drawer.mask().contains(QPoint(500, 200)));
        QSignalSpy clickSpy(underlying, &QPushButton::clicked);
        QTest::mouseClick(underlying, Qt::LeftButton);
        QCOMPARE(clickSpy.count(), 1);
        QVERIFY(drawer.isOpen());
    }

    void trapsModalTabFocusClosesOnEscapeAndRestoresFocus()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget host;
        auto *previous = new QPushButton(QStringLiteral("Previous"), &host);
        previous->setGeometry(400, 40, 120, 40);
        ZzFluentUI::ZzDrawer drawer(&host);
        drawer.setStyle(&style);
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        auto *first = new QLineEdit(content);
        auto *second = new QLineEdit(content);
        layout->addWidget(first);
        layout->addWidget(second);
        drawer.setContentWidget(content);
        showHost(&host);
        previous->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::focusWidget(), previous);

        drawer.openDrawer();
        QCOMPARE(QApplication::focusWidget(), first);
        QTest::keyClick(first, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), second);
        QTest::keyClick(second, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), first);
        QTest::keyClick(first, Qt::Key_Backtab);
        QCOMPARE(QApplication::focusWidget(), second);

        QTest::keyClick(second, Qt::Key_Escape);
        QVERIFY(!drawer.isOpen());
        QVERIFY(!drawer.isVisible());
        QCOMPARE(QApplication::focusWidget(), previous);
    }

    void reversesAnimationAndSettlesWhenReducedMotionChanges()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget host;
        ZzFluentUI::ZzDrawer drawer(&host);
        drawer.setStyle(&style);
        drawer.setWidthHint(200);
        drawer.setContentWidget(new QLabel(QStringLiteral("Animated")));
        showHost(&host);
        auto *animation = drawer.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        drawer.openDrawer();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QVERIFY(QTest::qWaitFor([&drawer] {
            const int x = panelHost(&drawer)->x();
            return x >= -160 && x < 0;
        }));
        const int openingX = panelHost(&drawer)->x();

        drawer.closeDrawer();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(panelHost(&drawer)->x(), openingX);
        QVERIFY(QTest::qWaitFor([&drawer, openingX] {
            const int x = panelHost(&drawer)->x();
            return x < openingX && x > -200;
        }));
        const int closingX = panelHost(&drawer)->x();
        drawer.openDrawer();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QCOMPARE(panelHost(&drawer)->x(), closingX);
        ZZ_COMPARE_EVENTUALLY(animation->state(), QAbstractAnimation::Stopped);
        QCOMPARE(panelHost(&drawer)->geometry(), QRect(0, 0, 200, 400));

        drawer.closeDrawer();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QCoreApplication::processEvents();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QVERIFY(!drawer.isOpen());
        QVERIFY(!drawer.isVisible());
    }

    void handlesExternalHideAndHostDestructionDuringAnimation()
    {
        QWidget host;
        auto *previous = new QPushButton(QStringLiteral("Previous"), &host);
        ZzFluentUI::ZzDrawer drawer(&host);
        drawer.setContentWidget(new QLabel(QStringLiteral("Content")));
        showHost(&host);
        previous->setFocus(Qt::OtherFocusReason);
        drawer.openDrawer();
        auto *animation = drawer.findChild<QVariantAnimation *>();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        drawer.hide();
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QVERIFY(!drawer.isOpen());
        QCOMPARE(QApplication::focusWidget(), previous);

        auto *ownedHost = new QWidget;
        auto *ownedDrawer = new ZzFluentUI::ZzDrawer(ownedHost);
        auto *ownedContent = new QLabel(QStringLiteral("Owned"));
        QPointer<ZzFluentUI::ZzDrawer> drawerGuard(ownedDrawer);
        QPointer<QWidget> contentGuard(ownedContent);
        ownedDrawer->setContentWidget(ownedContent);
        showHost(ownedHost);
        ownedDrawer->openDrawer();
        QCOMPARE(
            ownedDrawer->findChild<QVariantAnimation *>()->state(),
            QAbstractAnimation::Running);
        delete ownedHost;
        QVERIFY(drawerGuard.isNull());
        QVERIFY(contentGuard.isNull());
    }

    void keepsAnimationAndObjectBudgetsFixedAndAccessible()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        QWidget host;
        ZzFluentUI::ZzDrawer drawer(&host);
        drawer.setStyle(&style);
        drawer.setContentWidget(new QLabel(QStringLiteral("Content")));
        showHost(&host);
        QVariantAnimation *const animation =
            drawer.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);
        const qsizetype animationCount =
            drawer.findChildren<QAbstractAnimation *>().size();
        const qsizetype timerCount = drawer.findChildren<QTimer *>().size();
        const qsizetype objectCount = drawer.findChildren<QObject *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            drawer.openDrawer();
            drawer.closeDrawer();
        }

        QCOMPARE(drawer.findChild<QVariantAnimation *>(), animation);
        QCOMPARE(
            drawer.findChildren<QAbstractAnimation *>().size(),
            animationCount);
        QCOMPARE(drawer.findChildren<QTimer *>().size(), timerCount);
        QCOMPARE(drawer.findChildren<QObject *>().size(), objectCount);
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&drawer);
        QVERIFY(interface != nullptr);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("边缘抽屉"));
    }
};

QTEST_MAIN(ZzDrawerTest)

#include "ZzDrawerTest.moc"
