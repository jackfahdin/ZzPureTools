#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 按稳定逻辑索引属性查找面包屑按钮。 */
QToolButton *zzBreadcrumbButton(
    ZzFluentUI::ZzBreadcrumbBar *bar,
    int logicalIndex)
{
    const auto buttons = bar->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button->property("zzBreadcrumbIndex").toInt()
            == logicalIndex) {
            return button;
        }
    }
    return nullptr;
}

/** @brief 返回指定区域内与目标颜色完全匹配像素的最小边界。 */
QRect zzColorBounds(
    const QImage &image,
    const QRect &area,
    const QColor &color)
{
    QRect result;
    const QRect bounded = area.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (image.pixelColor(x, y) == color) {
                result = result.united(QRect(x, y, 1, 1));
            }
        }
    }
    return result;
}

} // namespace

/** @brief 验证导航控件只消费展示模型并保留逻辑索引意图。 */
class ZzNavigationControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void breadcrumbOverflowPreservesLogicalNavigation_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::newRow("ltr") << false;
        QTest::newRow("rtl") << true;
    }

    void breadcrumbOverflowPreservesLogicalNavigation()
    {
        QFETCH(bool, rtl);
        ZzFluentUI::ZzBreadcrumbBar bar;
        bar.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        bar.setItems({QStringLiteral("Root directory"), QStringLiteral("Documents"),
            QStringLiteral("Project workspace"), QStringLiteral("Current page")});
        bar.setCurrentIndex(3);
        bar.resize(190, 40);
        bar.show();
        QCoreApplication::processEvents();
        auto *overflow = bar.findChild<QToolButton *>(QStringLiteral("zzBreadcrumbOverflow"));
        QVERIFY(overflow != nullptr);
        QVERIFY(overflow->isVisible());
        QVERIFY(!overflow->accessibleName().isEmpty());
        QMenu *const menu = overflow->menu();
        QVERIFY(menu != nullptr);
        QVERIFY(!menu->actions().isEmpty());
        QToolButton *const last = zzBreadcrumbButton(&bar, 3);
        QVERIFY(last != nullptr && last->isVisible());
        QVERIFY(bar.rect().contains(last->geometry()));
        QVERIFY(!last->geometry().intersects(overflow->geometry()));
        QSignalSpy requested(&bar, &ZzFluentUI::ZzBreadcrumbBar::indexRequested);
        for (QAction *action : menu->actions()) {
            const int index = action->data().toInt();
            QVERIFY(!zzBreadcrumbButton(&bar, index)->isVisible());
            QCOMPARE(action->text(), bar.items().at(index));
        }
        menu->actions().first()->trigger();
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.first().first().toInt(), 0);
        QCOMPARE(bar.currentIndex(), 3);
        overflow->setFocus();
        QTest::keyClick(overflow, Qt::Key_End);
        QVERIFY(last->hasFocus());
        QTest::keyClick(last, Qt::Key_Home);
        QVERIFY(overflow->hasFocus());
        QTest::keyClick(overflow, rtl ? Qt::Key_Left : Qt::Key_Right);
        QVERIFY(!overflow->hasFocus());
        bar.resize(1000, 40);
        QCoreApplication::processEvents();
        QVERIFY(overflow->isHidden());
        for (int index = 0; index < 4; ++index) {
            QVERIFY(zzBreadcrumbButton(&bar, index)->isVisible());
        }
        auto *first = zzBreadcrumbButton(&bar, 0);
        first->setFocus();
        bar.resize(120, 40);
        QCoreApplication::processEvents();
        QVERIFY(overflow->hasFocus());
    }

    void breadcrumbOverflowReceiverCanRebuildOrDelete_data()
    {
        QTest::addColumn<bool>("destroy");
        QTest::newRow("rebuild") << false;
        QTest::newRow("destroy") << true;
    }

    void breadcrumbHandlesTinyWidthsAndFontChanges()
    {
        ZzFluentUI::ZzBreadcrumbBar bar;
        bar.setItems({QStringLiteral("Root"), QStringLiteral("A very long last component")});
        bar.resize(20, 40);
        bar.show();
        QCoreApplication::processEvents();
        auto *overflow = bar.findChild<QToolButton *>(QStringLiteral("zzBreadcrumbOverflow"));
        QVERIFY(overflow && overflow->isVisible());
        QCOMPARE(overflow->menu()->actions().size(), 2);
        QVERIFY(bar.rect().contains(overflow->geometry()));
        QVERIFY(!zzBreadcrumbButton(&bar, 1)->isVisible());
        bar.resize(500, 40);
        QCoreApplication::processEvents();
        QVERIFY(overflow->isHidden());
        QFont large = bar.font();
        large.setPointSize(40);
        bar.setFont(large);
        QCoreApplication::processEvents();
        QVERIFY(overflow->isVisible());
        auto *last = zzBreadcrumbButton(&bar, 1);
        QVERIFY(last->isVisible());
        QVERIFY(bar.rect().contains(last->geometry()));
        QCOMPARE(last->accessibleName(), bar.items().last());
        bar.setItems({QStringLiteral("One extremely long component which must remain accessible")});
        bar.resize(30, 40);
        QCoreApplication::processEvents();
        auto *only = zzBreadcrumbButton(&bar, 0);
        QVERIFY(only->isVisible());
        QVERIFY(bar.rect().contains(only->geometry()));
        QCOMPARE(only->accessibleName(), bar.items().first());
        QVERIFY(overflow->isHidden());
        bar.setItems({});
        QVERIFY(overflow->isHidden());
        QVERIFY(overflow->menu()->actions().isEmpty());
    }

    void breadcrumbKeyboardActivationUsesLogicalIndex()
    {
        ZzFluentUI::ZzBreadcrumbBar bar;
        bar.setItems({QStringLiteral("Root"), QStringLiteral("Page")});
        bar.resize(400, 40);
        bar.show();
        QCoreApplication::processEvents();
        QSignalSpy requested(&bar, &ZzFluentUI::ZzBreadcrumbBar::indexRequested);
        auto *last = zzBreadcrumbButton(&bar, 1);
        last->setFocus();
        QTest::keyClick(last, Qt::Key_Return);
        QTest::keyClick(last, Qt::Key_Space);
        QCOMPARE(requested.count(), 2);
        QCOMPARE(requested.at(0).first().toInt(), 1);
        QCOMPARE(requested.at(1).first().toInt(), 1);
        QCOMPARE(bar.currentIndex(), -1);
        bar.resize(65, 40);
        QCoreApplication::processEvents();
        auto *overflow = bar.findChild<QToolButton *>(QStringLiteral("zzBreadcrumbOverflow"));
        QVERIFY(overflow && overflow->isVisible());
        auto *menu = overflow->menu();
        QVERIFY(menu && !menu->actions().isEmpty());
        QTimer::singleShot(0, menu, [menu] {
            menu->setActiveAction(menu->actions().first());
            QTest::keyClick(menu, Qt::Key_Return);
        });
        overflow->setFocus();
        QTest::keyClick(overflow, Qt::Key_Return);
        QCOMPARE(requested.count(), 3);
        QCOMPARE(requested.at(2).first().toInt(), 0);
    }

    void breadcrumbOverflowReceiverCanRebuildOrDelete()
    {
        QFETCH(bool, destroy);
        auto *bar = new ZzFluentUI::ZzBreadcrumbBar;
        const QPointer<ZzFluentUI::ZzBreadcrumbBar> guard(bar);
        bar->setItems({QStringLiteral("A long root name"), QStringLiteral("Nested path"),
            QStringLiteral("Page")});
        bar->resize(100, 40);
        bar->show();
        QCoreApplication::processEvents();
        auto *overflow = bar->findChild<QToolButton *>(QStringLiteral("zzBreadcrumbOverflow"));
        QVERIFY(overflow && overflow->menu() && !overflow->menu()->actions().isEmpty());
        connect(bar, &ZzFluentUI::ZzBreadcrumbBar::indexRequested, this, [bar, destroy] {
            if (destroy) {
                delete bar;
            } else {
                bar->setItems({QStringLiteral("Replacement")});
                bar->setCurrentIndex(0);
            }
        });
        overflow->menu()->actions().first()->trigger();
        if (destroy) {
            QVERIFY(guard.isNull());
        } else {
            QCOMPARE(bar->items(), QStringList{QStringLiteral("Replacement")});
            QCOMPARE(bar->currentIndex(), 0);
            QVERIFY(overflow->isHidden());
            delete bar;
        }
    }

    void configuresBoundedLargeModelLayout()
    {
        ZzFluentUI::ZzNavigationView view;
        QStandardItemModel model;
        model.setRowCount(100000);
        model.setData(model.index(0, 0), QStringLiteral("Home"));

        view.setModel(&model);

        QCOMPARE(view.model(), &model);
        QVERIFY(view.uniformItemSizes());
        QCOMPARE(view.layoutMode(), QListView::Batched);
        QVERIFY(view.batchSize() <= 128);
        QCOMPARE(view.batchSize(), 64);
        QVERIFY(view.hasMouseTracking());
        QVERIFY(view.viewport()->hasMouseTracking());
        QVERIFY(view.viewport()->testAttribute(Qt::WA_Hover));
        QCOMPARE(view.findChildren<QStackedWidget *>().size(), 0);
        QCOMPARE(view.findChildren<QVariantAnimation *>().size(), 1);
    }

    void keepsTwoPhaseTransitionContinuousAndBounded()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzNavigationView view;
        QStandardItemModel model(3, 1);
        for (int row = 0; row < model.rowCount(); ++row) {
            model.setData(
                model.index(row, 0),
                QStringLiteral("Item %1").arg(row));
        }
        view.setStyle(&style);
        view.setModel(&model);
        view.resize(240, 140);
        view.show();
        QCoreApplication::processEvents();
        view.selectionModel()->select(
            model.index(0, 0),
            QItemSelectionModel::ClearAndSelect);
        view.selectionModel()->select(
            model.index(1, 0),
            QItemSelectionModel::ClearAndSelect);

        const auto animations = view.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 1);
        QVariantAnimation *const animation = animations.constFirst();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        const int duration = animation->duration();
        QVERIFY(duration > 0);
        const QColor accent = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::Accent);
        const auto renderViewport = [&view] {
            QImage image(
                view.viewport()->size(),
                QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            view.viewport()->render(&painter);
            return image;
        };

        animation->setCurrentTime(duration / 4);
        const QImage firstPhase = renderViewport();
        QVERIFY(!zzColorBounds(
            firstPhase, view.visualRect(model.index(0, 0)), accent).isEmpty());
        QVERIFY(zzColorBounds(
            firstPhase, view.visualRect(model.index(1, 0)), accent).isEmpty());

        animation->setCurrentTime((duration * 3) / 4);
        const QImage secondPhase = renderViewport();
        QVERIFY(zzColorBounds(
            secondPhase, view.visualRect(model.index(0, 0)), accent).isEmpty());
        const QRect incomingBeforeReverse = zzColorBounds(
            secondPhase, view.visualRect(model.index(1, 0)), accent);
        QVERIFY(!incomingBeforeReverse.isEmpty());

        view.selectionModel()->select(
            model.index(0, 0),
            QItemSelectionModel::ClearAndSelect);
        const QImage reversed = renderViewport();
        QCOMPARE(
            zzColorBounds(
                reversed, view.visualRect(model.index(1, 0)), accent).height(),
            incomingBeforeReverse.height());
        QCOMPARE(view.findChildren<QVariantAnimation *>().size(), 1);

        for (int iteration = 0; iteration < 1000; ++iteration) {
            view.selectionModel()->select(
                model.index(iteration % model.rowCount(), 0),
                QItemSelectionModel::ClearAndSelect);
            animation->setCurrentTime((duration * 7) / 10);
        }
        QCOMPARE(view.findChildren<QVariantAnimation *>().size(), 1);
    }

    void honorsReducedMotionWithoutRemovingFixedAnimation()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzNavigationView view;
        QStandardItemModel model(2, 1);
        model.setData(model.index(0, 0), QStringLiteral("First"));
        model.setData(model.index(1, 0), QStringLiteral("Second"));
        view.setStyle(&style);
        view.setModel(&model);
        controller.setReducedMotion(true);

        view.selectionModel()->select(
            model.index(0, 0),
            QItemSelectionModel::ClearAndSelect);
        view.selectionModel()->select(
            model.index(1, 0),
            QItemSelectionModel::ClearAndSelect);

        const auto animations = view.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations.constFirst()->state(), QAbstractAnimation::Stopped);

        view.selectionModel()->select(
            model.index(0, 0),
            QItemSelectionModel::ClearAndSelect);
        QCOMPARE(animations.constFirst()->state(), QAbstractAnimation::Stopped);
    }

    void preservesKeyboardSelectionAndEmitsIntent()
    {
        ZzFluentUI::ZzNavigationView view;
        QStandardItemModel model;
        for (int row = 0; row < 3; ++row) {
            model.appendRow(new QStandardItem(
                QStringLiteral("Item %1").arg(row)));
        }
        view.setModel(&model);
        view.show();
        QCoreApplication::processEvents();
        view.setCurrentIndex(model.index(0, 0));
        view.setFocus();
        QSignalSpy navigationSpy(
            &view,
            &ZzFluentUI::ZzNavigationView::navigationRequested);

        QTest::keyClick(&view, Qt::Key_Down);
        QCOMPARE(view.currentIndex().row(), 1);
        QTest::keyClick(&view, Qt::Key_Up);
        QCOMPARE(view.currentIndex().row(), 0);
        QTest::keyClick(&view, Qt::Key_Return);
        QCOMPARE(navigationSpy.count(), 1);
        QCOMPARE(
            navigationSpy.takeFirst().at(0).value<QModelIndex>(),
            model.index(0, 0));

        auto *disabled = new QStandardItem(QStringLiteral("Disabled"));
        disabled->setEnabled(false);
        model.appendRow(disabled);
        Q_EMIT view.clicked(model.index(3, 0));
        QCOMPARE(navigationSpy.count(), 0);
        auto *nonSelectable = new QStandardItem(
            QStringLiteral("Section-like row"));
        nonSelectable->setFlags(Qt::ItemIsEnabled);
        model.appendRow(nonSelectable);
        Q_EMIT view.clicked(model.index(4, 0));
        QCOMPARE(navigationSpy.count(), 0);
        view.setCurrentIndex(QModelIndex());
        QTest::keyClick(&view, Qt::Key_Enter);
        QCOMPARE(navigationSpy.count(), 0);
    }

    void changesCompactPresentationWithoutTouchingModel()
    {
        ZzFluentUI::ZzNavigationView view;
        QStandardItemModel model;
        model.setRowCount(2);
        view.setModel(&model);
        QSignalSpy compactSpy(
            &view,
            &ZzFluentUI::ZzNavigationView::compactChanged);

        QCOMPARE(view.width(), 240);
        view.setCompact(true);
        view.setCompact(true);
        QVERIFY(view.isCompact());
        QCOMPARE(view.width(), 48);
        QCOMPARE(view.model(), &model);
        QCOMPARE(compactSpy.count(), 1);

        view.setCompact(false);
        QCOMPARE(view.width(), 240);
        QCOMPARE(compactSpy.count(), 2);
    }

    void rendersDescriptorAndBadgeWithoutMutatingModel()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzFluentUI::ZzNavigationView view;
        QStandardItemModel model;
        auto *item = new QStandardItem(QStringLiteral("Workspace"));
        const ZzFluentUI::ZzIconDescriptor descriptor{
            QStringLiteral(
                ":/zzfluent/navigation/ZzFluentTestSquare.svg"),
            true};
        item->setData(
            QVariant::fromValue(descriptor),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Icon));
        item->setData(
            QStringLiteral("9"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge));
        model.appendRow(item);
        view.setStyle(&style);
        view.setModel(&model);
        view.setCurrentIndex(model.index(0, 0));
        view.selectionModel()->select(
            model.index(0, 0),
            QItemSelectionModel::ClearAndSelect);
        view.resize(240, 48);
        view.show();
        QCoreApplication::processEvents();

        QImage regular(
            view.viewport()->size(),
            QImage::Format_ARGB32_Premultiplied);
        regular.fill(Qt::transparent);
        QPainter regularPainter(&regular);
        view.viewport()->render(&regularPainter);
        regularPainter.end();
        QVERIFY(style.iconCacheBytes() > 0);
        QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Workspace"));
        QCOMPARE(
            model.index(0, 0)
                .data(static_cast<int>(
                    ZzFluentUI::ZzNavigationItemRole::Badge))
                .toString(),
            QStringLiteral("9"));

        const QRect itemRect = view.visualRect(model.index(0, 0));
        const QRect accentBounds = zzColorBounds(
            regular,
            itemRect,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::Accent));
        const QRect foregroundBounds = zzColorBounds(
            regular,
            itemRect,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::TextPrimary));
        QVERIFY(!accentBounds.isEmpty());
        QVERIFY(accentBounds.width() <= 3);
        QVERIFY(accentBounds.height() <= 16);
        QVERIFY(!foregroundBounds.isEmpty());
        QVERIFY(accentBounds.right() < foregroundBounds.left());

        view.setCompact(true);
        QCoreApplication::processEvents();
        QImage compact(
            view.viewport()->size(),
            QImage::Format_ARGB32_Premultiplied);
        compact.fill(Qt::transparent);
        QPainter compactPainter(&compact);
        view.viewport()->render(&compactPainter);
        compactPainter.end();
        QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Workspace"));
        QVERIFY(regular != compact);
    }

    void breadcrumbCanBeDestroyedByNavigationReceiver()
    {
        auto *bar = new ZzFluentUI::ZzBreadcrumbBar;
        const QPointer<ZzFluentUI::ZzBreadcrumbBar> guard(bar);
        bar->setItems({QStringLiteral("Root"), QStringLiteral("Page")});
        QToolButton *const button = zzBreadcrumbButton(bar, 0);
        QVERIFY(button != nullptr);
        connect(bar, &ZzFluentUI::ZzBreadcrumbBar::indexRequested,
            this, [bar](int) { delete bar; });

        button->click();

        QVERIFY(guard.isNull());
    }

    void breadcrumbCanRebuildDuringNavigation_data()
    {
        QTest::addColumn<bool>("changeDirection");
        QTest::newRow("replace-path") << false;
        QTest::newRow("change-direction") << true;
    }

    void breadcrumbCanRebuildDuringNavigation()
    {
        QFETCH(bool, changeDirection);
        ZzFluentUI::ZzBreadcrumbBar bar;
        bar.setItems({QStringLiteral("Root"), QStringLiteral("Page")});
        const QPointer<QToolButton> oldButton(zzBreadcrumbButton(&bar, 0));
        QVERIFY(oldButton != nullptr);
        int requestCount = 0;
        connect(&bar, &ZzFluentUI::ZzBreadcrumbBar::indexRequested,
            this, [&](int index) {
                ++requestCount;
                QCOMPARE(index, 0);
                if (changeDirection) {
                    bar.setLayoutDirection(Qt::RightToLeft);
                } else {
                    bar.setItems({QStringLiteral("New root"), QStringLiteral("New page")});
                }
                bar.setCurrentIndex(1);
            });

        oldButton->click();

        QCOMPARE(requestCount, 1);
        // An input-event sender must survive until its event has unwound.
        QVERIFY(oldButton != nullptr);
        if (changeDirection) {
            // Direction changes now update presentation in place.
            QCOMPARE(zzBreadcrumbButton(&bar, 0), oldButton.data());
            QCOMPARE(bar.layoutDirection(), Qt::RightToLeft);
        } else {
            QVERIFY(oldButton->isHidden());
            oldButton->click();
            QCOMPARE(requestCount, 1);
        }
        QToolButton *const current = zzBreadcrumbButton(&bar, 1);
        QVERIFY(current != nullptr);
        QVERIFY(current->property("zzBreadcrumbCurrent").toBool());
        QCOMPARE(bar.currentIndex(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(oldButton.isNull(), !changeDirection);
        QCOMPARE(bar.findChildren<QToolButton *>().size(), 3);
    }

    void keepsBreadcrumbLogicalIndexesAcrossRtl()
    {
        ZzFluentUI::ZzBreadcrumbBar bar;
        const QStringList items{
            QStringLiteral("根目录与很长的中文展示文本"),
            QStringLiteral("设置"),
            QStringLiteral("网络")};
        bar.setItems(items);
        bar.setCurrentIndex(1);
        bar.resize(360, 40);
        bar.show();
        QCoreApplication::processEvents();

        QToolButton *logicalZero = zzBreadcrumbButton(&bar, 0);
        QToolButton *logicalOne = zzBreadcrumbButton(&bar, 1);
        QToolButton *logicalTwo = zzBreadcrumbButton(&bar, 2);
        QVERIFY(logicalZero != nullptr);
        QVERIFY(logicalOne != nullptr);
        QVERIFY(logicalTwo != nullptr);
        if (logicalZero == nullptr || logicalOne == nullptr
            || logicalTwo == nullptr) {
            return;
        }
        QCOMPARE(logicalZero->accessibleName(), items.at(0));
        QVERIFY(logicalOne->property("zzBreadcrumbCurrent").toBool());
        QVERIFY(logicalZero->geometry().right() < logicalTwo->geometry().left());
        QVERIFY(bar.rect().contains(logicalZero->geometry().center()));

        bar.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        logicalZero = zzBreadcrumbButton(&bar, 0);
        logicalOne = zzBreadcrumbButton(&bar, 1);
        logicalTwo = zzBreadcrumbButton(&bar, 2);
        QVERIFY(logicalZero != nullptr);
        QVERIFY(logicalOne != nullptr);
        QVERIFY(logicalTwo != nullptr);
        if (logicalZero == nullptr || logicalOne == nullptr
            || logicalTwo == nullptr) {
            return;
        }
        QVERIFY(logicalZero->geometry().left() > logicalTwo->geometry().right());
        QVERIFY(logicalOne->property("zzBreadcrumbCurrent").toBool());

        QSignalSpy indexSpy(
            &bar,
            &ZzFluentUI::ZzBreadcrumbBar::indexRequested);
        logicalZero->click();
        QCOMPARE(indexSpy.count(), 1);
        QCOMPARE(indexSpy.takeFirst().at(0).toInt(), 0);
        QVERIFY(!logicalZero->property("zzBreadcrumbCurrent").toBool());
        QVERIFY(logicalOne->property("zzBreadcrumbCurrent").toBool());
        QCOMPARE(bar.items(), items);

        bar.setCurrentIndex(99);
        QCOMPARE(bar.currentIndex(), -1);
        bar.setItems({});
        QCOMPARE(bar.currentIndex(), -1);
    }
};

QTEST_MAIN(ZzNavigationControlsTest)

#include "ZzNavigationControlsTest.moc"
