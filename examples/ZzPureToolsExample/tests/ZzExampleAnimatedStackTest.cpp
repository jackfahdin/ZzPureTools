#include "../ZzExampleShowcasePagePrivate.h"
#include <QtCore/QVariantAnimation>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QVBoxLayout>
#include <QtTest/QTest>
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

/** @brief 经真实 Showcase 装配入口验证公共页面容器接入。 */
class ZzExampleAnimatedStackTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void navigationUsesPublicStack()
    {
        using Stack = ZzFluentUI::ZzAnimatedStackedWidget;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget page;
        page.setStyle(&style);
        auto *layout = new QVBoxLayout(&page);
        ZzExample::ZzExampleShowcasePagePrivate::buildTabBars(layout, &page);
        auto *stack = page.findChild<Stack *>(QStringLiteral("zzExampleNavigationPages"));
        QVERIFY(stack);
        stack->setStyle(&style);
        QCOMPARE(stack->count(), 5);
        QCOMPARE(stack->transitionMode(), Stack::ZzTransitionMode::SlideVertical);
        auto *tabs = page.findChild<QTabBar *>(QStringLiteral("zzExampleNavigationTabBar"));
        QVERIFY(tabs);
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(stack->widget(i)->objectName(), QStringLiteral("zzExampleNavigationBody%1").arg(i));
            auto *label = qobject_cast<QLabel *>(stack->widget(i));
            QVERIFY(label && !label->text().isEmpty());
        }
        page.resize(1000, 1500);
        page.show();
        QCoreApplication::processEvents();
        tabs->setCurrentIndex(1);
        QVERIFY(stack->isTransitionRunning());
        tabs->setCurrentIndex(2);
        tabs->setCurrentIndex(4);
        QCOMPARE(stack->currentIndex(), 1);
        auto *animation = stack->findChild<QVariantAnimation *>();
        animation->setCurrentTime(animation->duration());
        QCOMPARE(stack->currentIndex(), 4);
        animation->setCurrentTime(animation->duration());
        QVERIFY(stack->widget(4)->isVisible());
        tabs->setCurrentIndex(0);
        page.setLayoutDirection(Qt::RightToLeft);
        QVERIFY(!stack->isTransitionRunning());
        QEvent language(QEvent::LanguageChange);
        QApplication::sendEvent(&page, &language);
        page.hide();
        page.show();
        controller.setReducedMotion(true);
        QCoreApplication::processEvents();
        tabs->setCurrentIndex(3);
        QCOMPARE(stack->currentIndex(), 3);
        QVERIFY(!stack->isTransitionRunning());
    }
};

QTEST_MAIN(ZzExampleAnimatedStackTest)
#include "ZzExampleAnimatedStackTest.moc"
