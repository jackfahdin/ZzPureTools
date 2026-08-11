#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/** @brief 验证 Pivot 的项 API、原生输入语义、绘制和固定动画预算。 */
class ZzPivotTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 显示固定尺寸 Pivot 并完成布局。 */
    static void showPivot(ZzFluentUI::ZzPivot *pivot, int width = 520)
    {
        pivot->resize(width, pivot->sizeHint().height());
        pivot->show();
        QCoreApplication::processEvents();
    }

private Q_SLOTS:
    void managesItemsAndEmitsCountOnlyForEffectiveChanges()
    {
        ZzFluentUI::ZzPivot pivot;
        QSignalSpy countSpy(
            &pivot, &ZzFluentUI::ZzPivot::itemCountChanged);

        QCOMPARE(pivot.count(), 0);
        QCOMPARE(pivot.currentIndex(), -1);
        QVERIFY(!pivot.isMovable());
        QVERIFY(!pivot.tabsClosable());
        QVERIFY(!pivot.expanding());
        QVERIFY(pivot.usesScrollButtons());
        QVERIFY(!pivot.drawBase());
        QCOMPARE(pivot.shape(), QTabBar::RoundedNorth);

        QCOMPARE(pivot.addItem(QStringLiteral("Overview")), 0);
        QCOMPARE(pivot.addItem(QStringLiteral("Reports")), 1);
        QCOMPARE(pivot.insertItem(1, QStringLiteral("Settings")), 1);
        QCOMPARE(pivot.count(), 3);
        QCOMPARE(countSpy.count(), 3);
        QCOMPARE(pivot.itemText(0), QStringLiteral("Overview"));
        QCOMPARE(pivot.itemText(1), QStringLiteral("Settings"));
        QCOMPARE(pivot.itemText(2), QStringLiteral("Reports"));
        QCOMPARE(pivot.itemText(-1), QString());

        pivot.setItemText(1, QStringLiteral("Preferences"));
        pivot.setItemText(1, QStringLiteral("Preferences"));
        pivot.setItemText(8, QStringLiteral("Ignored"));
        QCOMPARE(pivot.itemText(1), QStringLiteral("Preferences"));
        QCOMPARE(countSpy.count(), 3);

        pivot.removeItem(-1);
        pivot.removeItem(8);
        QCOMPARE(countSpy.count(), 3);
        pivot.removeItem(1);
        QCOMPARE(pivot.count(), 2);
        QCOMPARE(countSpy.count(), 4);
        QCOMPARE(countSpy.at(3).at(0).toInt(), 2);
    }

    void switchesOnceByMouseKeyboardAndMnemonic()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.addItem(QStringLiteral("&Overview"));
        pivot.addItem(QStringLiteral("&Settings"));
        pivot.addItem(QStringLiteral("&About"));
        showPivot(&pivot);
        QSignalSpy currentSpy(&pivot, &QTabBar::currentChanged);

        QTest::mouseClick(
            &pivot,
            Qt::LeftButton,
            Qt::NoModifier,
            pivot.tabRect(1).center());
        QCOMPARE(pivot.currentIndex(), 1);
        QCOMPARE(currentSpy.count(), 1);

        pivot.setFocus(Qt::OtherFocusReason);
        QTest::keyClick(&pivot, Qt::Key_Right);
        QCOMPARE(pivot.currentIndex(), 2);
        QTest::keyClick(&pivot, Qt::Key_Home);
        QCOMPARE(pivot.currentIndex(), 0);
        QTest::keyClick(&pivot, Qt::Key_End);
        QCOMPARE(pivot.currentIndex(), 2);

        pivot.setTabEnabled(0, false);
        QTest::keyClick(&pivot, Qt::Key_Home);
        QCOMPARE(pivot.currentIndex(), 1);
        pivot.setTabEnabled(0, true);
        pivot.setTabVisible(2, false);
        QTest::keyClick(&pivot, Qt::Key_End);
        QCOMPARE(pivot.currentIndex(), 1);
        pivot.setTabVisible(2, true);

        QTest::keyClick(&pivot, Qt::Key_S, Qt::AltModifier);
        QCOMPARE(pivot.currentIndex(), 1);
    }

    void preservesRtlGeometryOverflowAndAccessibility()
    {
        ZzFluentUI::ZzPivot pivot;
        pivot.setLayoutDirection(Qt::RightToLeft);
        for (int index = 0; index < 12; ++index) {
            pivot.addItem(QStringLiteral("Long destination %1").arg(index));
        }
        showPivot(&pivot, 260);

        QVERIFY(pivot.tabRect(0).center().x() > pivot.tabRect(1).center().x());
        const auto scrollButtons = pivot.findChildren<QToolButton *>();
        QCOMPARE(scrollButtons.size(), 2);
        QVERIFY(scrollButtons.at(0)->isVisible()
                || scrollButtons.at(1)->isVisible());

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&pivot);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::PageTabList);
        int pageTabCount = 0;
        bool foundFirstItem = false;
        for (int index = 0; index < interface->childCount(); ++index) {
            QAccessibleInterface *child = interface->child(index);
            if (child == nullptr || child->role() != QAccessible::PageTab) {
                continue;
            }
            ++pageTabCount;
            foundFirstItem = foundFirstItem
                || child->text(QAccessible::Name).contains(
                    QStringLiteral("Long destination 0"));
        }
        QCOMPARE(pageTabCount, pivot.count());
        QVERIFY(foundFirstItem);
    }

    void animatesContinuouslyAndDrawsOnlyBottomIndicator()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        pivot.addItem(QStringLiteral("Overview"));
        pivot.addItem(QStringLiteral("Build output"));
        pivot.addItem(QStringLiteral("History"));
        showPivot(&pivot);
        auto *animation = pivot.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        pivot.setCurrentIndex(1);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QTest::qWait(24);
        pivot.setCurrentIndex(2);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);

        pivot.clearFocus();
        QTest::mouseMove(
            &pivot,
            QPoint(pivot.width() - 1, pivot.height() - 1));
        QCoreApplication::processEvents();
        QImage image(pivot.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(pivot.palette().color(QPalette::Window));
        QPainter painter(&image);
        pivot.render(&painter);
        painter.end();
        const auto snapshot = style.themeSnapshot();
        const QColor accent = snapshot->color(ZzFluentUI::ZzColorToken::Accent);
        const QRect currentTab = pivot.tabRect(pivot.currentIndex());
        const int thickness = qCeil(snapshot->metric(
            ZzFluentUI::ZzMetricToken::SelectionIndicatorThickness));
        int bottomAccentPixels = 0;
        int upperAccentPixels = 0;
        for (int y = currentTab.top(); y <= currentTab.bottom(); ++y) {
            for (int x = currentTab.left(); x <= currentTab.right(); ++x) {
                if (image.pixelColor(x, y) != accent) {
                    continue;
                }
                if (y >= currentTab.bottom() - thickness) {
                    ++bottomAccentPixels;
                } else {
                    ++upperAccentPixels;
                }
            }
        }
        QVERIFY(bottomAccentPixels > 0);
        QCOMPARE(upperAccentPixels, 0);
        QCOMPARE(
            image.pixelColor(
                pivot.tabRect(0).left() + 1,
                pivot.tabRect(0).top() + 1),
            image.pixelColor(
                currentTab.left() + 1,
                currentTab.top() + 1));
    }

    void reducedMotionStopsRunningAnimationAndKeepsObjectBudget()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzPivot pivot;
        pivot.setStyle(&style);
        for (const QString &text : {
                 QStringLiteral("Overview"),
                 QStringLiteral("Settings"),
                 QStringLiteral("About"),
                 QStringLiteral("History")}) {
            pivot.addItem(text);
        }
        showPivot(&pivot);
        QVariantAnimation *const animation =
            pivot.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        pivot.setCurrentIndex(1);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);

        const qsizetype animationCount =
            pivot.findChildren<QAbstractAnimation *>().size();
        const qsizetype timerCount = pivot.findChildren<QTimer *>().size();
        const qsizetype objectCount = pivot.findChildren<QObject *>().size();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            pivot.setCurrentIndex(iteration % pivot.count());
        }
        QCOMPARE(pivot.findChild<QVariantAnimation *>(), animation);
        QCOMPARE(
            pivot.findChildren<QAbstractAnimation *>().size(),
            animationCount);
        QCOMPARE(pivot.findChildren<QTimer *>().size(), timerCount);
        QCOMPARE(pivot.findChildren<QObject *>().size(), objectCount);
    }
};

QTEST_MAIN(ZzPivotTest)

#include "ZzPivotTest.moc"
