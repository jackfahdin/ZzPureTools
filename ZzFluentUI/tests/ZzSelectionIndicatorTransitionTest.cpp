#include <memory>

#include <QtCore/QItemSelectionModel>
#include <QtCore/QDir>
#include <QtCore/QVariantAnimation>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>

#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzColorToken.h>

#include "../widgets/src/private/ZzSelectionIndicatorTransition.h"

/** @brief 检查共用指示条状态及原生视图的真实选择过渡。 */
class ZzSelectionIndicatorTransitionTest final : public QObject
{
    Q_OBJECT

    /** @brief 只统计指示条槽位的实色像素，排除文字、背景和焦点边框。 */
    static int indicatorPixels(QAbstractItemView *view, const QModelIndex &index,
        const QColor &accent)
    {
        const QImage image = view->viewport()->grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        QRectF strip(view->visualRect(index));
        strip.setLeft(strip.left() + 4);
        strip.setWidth(3);
        const QRect pixels(qFloor(strip.left() * dpr), qFloor(strip.top() * dpr),
            qCeil(strip.width() * dpr), qCeil(strip.height() * dpr));
        int count = 0;
        const QRect bounded = pixels.intersected(image.rect());
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                count += image.pixelColor(x, y) == accent ? 1 : 0;
            }
        }
        return count;
    }

    static void saveFrame(QAbstractItemView *view, const QString &suffix)
    {
        const QString directory = qEnvironmentVariable("ZZ_INDICATOR_REPORT_DIR");
        if (directory.isEmpty()) {
            return;
        }
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(view->viewport()->grab().save(QDir(directory).filePath(
            QString::fromLatin1(QTest::currentDataTag()) + suffix + QStringLiteral(".png"))));
    }

private Q_SLOTS:
    void themeKeepsExistingDimensionsAndExplicitGap()
    {
        ZzFluentUI::ZzThemeController controller;
        const auto snapshot = controller.snapshot();
        using ZzFluentUI::ZzMetricToken;
        QCOMPARE(snapshot->metric(ZzMetricToken::SelectionIndicatorLeading), 4.0);
        QCOMPARE(snapshot->metric(ZzMetricToken::SelectionIndicatorContentGap), 3.0);
        QCOMPARE(snapshot->metric(ZzMetricToken::SelectionIndicatorThickness), 3.0);
        QCOMPARE(snapshot->metric(ZzMetricToken::SelectionIndicatorExtent), 16.0);
    }

    void sharedDelegateKeepsViewsAndSelectionModelsIndependent()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStandardItemModel model(4, 1);
        ZzFluentUI::ZzFluentItemDelegate delegate;
        QListView first;
        QListView second;
        for (auto *view : {&first, &second}) {
            view->setStyle(&style);
            view->setModel(&model);
            view->setItemDelegate(&delegate);
            view->show();
            QVERIFY(QTest::qWaitForWindowExposed(view));
            (void)view->viewport()->grab();
        }
        const auto choose = [&model](QItemSelectionModel *selection, int row) {
            selection->select(model.index(row, 0), QItemSelectionModel::ClearAndSelect);
        };
        choose(first.selectionModel(), 0);
        choose(first.selectionModel(), 1);
        const auto animations = style.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 2);
        QCOMPARE(animations.at(0)->state(), QAbstractAnimation::Running);
        QCOMPARE(animations.at(1)->state(), QAbstractAnimation::Stopped);
        QItemSelectionModel replacement(&model);
        first.setSelectionModel(&replacement);
        (void)first.viewport()->grab();
        choose(&replacement, 2);
        choose(&replacement, 3);
        QCOMPARE(style.findChildren<QVariantAnimation *>(), animations);
        QCOMPARE(animations.at(0)->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QCOMPARE(animations.at(0)->state(), QAbstractAnimation::Stopped);
        QCOMPARE(second.selectionModel()->hasSelection(), false);
        model.clear();
        QCOMPARE(animations.at(0)->state(), QAbstractAnimation::Stopped);
    }

    void multiSelectionDoesNotAnimateOrChangeTheSelectionSet()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStandardItemModel model(100000, 1);
        QListView view;
        view.setStyle(&style);
        view.setModel(&model);
        view.setUniformItemSizes(true);
        view.setSelectionMode(QAbstractItemView::ExtendedSelection);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        (void)view.viewport()->grab();
        auto *selection = view.selectionModel();
        selection->select(model.index(0, 0), QItemSelectionModel::ClearAndSelect);
        selection->select(QItemSelection(model.index(0, 0), model.index(99999, 0)),
            QItemSelectionModel::ClearAndSelect);
        const auto animations = style.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations.first()->state(), QAbstractAnimation::Stopped);
        QCOMPARE(selection->selection().size(), 1);
        QCOMPARE(selection->selection().first().height(), 100000);
        QVERIFY(selection->isSelected(model.index(99999, 0)));
    }

    void nativeTabRetargetsAndStopsForReducedMotion()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QTabBar bar;
        bar.setStyle(&style);
        bar.addTab(QStringLiteral("First"));
        bar.addTab(QStringLiteral("Second"));
        bar.addTab(QStringLiteral("Third"));
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        (void)bar.grab();
        bar.setCurrentIndex(1);
        const auto animations = style.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 1);
        auto *animation = animations.first();
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        animation->setCurrentTime(40);
        const auto displayed = animation->currentValue();
        bar.setCurrentIndex(2);
        QCOMPARE(animation->startValue(), displayed);
        controller.setReducedMotion(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
        QCOMPARE(bar.currentIndex(), 2);
    }

    void nativeIconGridDoesNotAcquireRowIndicators()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStandardItemModel model(2, 1);
        model.setData(model.index(0, 0), QStringLiteral("First"));
        model.setData(model.index(1, 0), QStringLiteral("Second"));
        QListView view;
        view.setStyle(&style);
        view.setModel(&model);
        view.setViewMode(QListView::IconMode);
        view.setCurrentIndex(model.index(0, 0));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        (void)view.viewport()->grab();
        view.setCurrentIndex(model.index(1, 0));
        QCOMPARE(style.findChildren<QVariantAnimation *>().size(), 0);
    }

    void selectionPaintBenchmark()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStandardItemModel model(100000, 1);
        QListView view;
        view.setStyle(&style);
        view.setModel(&model);
        view.setUniformItemSizes(true);
        view.resize(320, 240);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        (void)view.viewport()->grab();
        const auto first = model.index(0, 0);
        const auto second = model.index(1, 0);
        QBENCHMARK {
            view.selectionModel()->select(first, QItemSelectionModel::ClearAndSelect);
            view.viewport()->repaint();
            view.selectionModel()->select(second, QItemSelectionModel::ClearAndSelect);
            view.viewport()->repaint();
        }
        QVERIFY(style.findChildren<QVariantAnimation *>().size() <= 1);
        QCOMPARE(view.selectionModel()->selectedIndexes().size(), 1);
        QCOMPARE(view.selectionModel()->selectedIndexes().first(), second);
    }

    void visualMatrix_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<bool>("rtl");
        for (auto mode : {ZzFluentUI::ZzThemeMode::Light,
                 ZzFluentUI::ZzThemeMode::Dark, ZzFluentUI::ZzThemeMode::HighContrast}) {
            for (bool rtl : {false, true}) {
                QTest::newRow(qPrintable(QStringLiteral("theme-%1-rtl-%2")
                    .arg(static_cast<int>(mode)).arg(rtl))) << static_cast<int>(mode) << rtl;
            }
        }
    }

    void visualMatrix()
    {
        QFETCH(int, mode);
        QFETCH(bool, rtl);
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(static_cast<ZzFluentUI::ZzThemeMode>(mode));
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStandardItemModel model(4, 2);
        for (int row = 0; row < 4; ++row) {
            model.setData(model.index(row, 0), QStringLiteral("Item %1").arg(row));
            model.setData(model.index(row, 1), QStringLiteral("Value"));
        }
        QWidget root;
        root.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        auto *layout = new QHBoxLayout(&root);
        auto *list = new QListView(&root);
        auto *tree = new QTreeView(&root);
        auto *table = new QTableView(&root);
        for (QAbstractItemView *view : {static_cast<QAbstractItemView *>(list),
                 static_cast<QAbstractItemView *>(tree), static_cast<QAbstractItemView *>(table)}) {
            view->setStyle(&style);
            view->setModel(&model);
            view->setSelectionBehavior(QAbstractItemView::SelectRows);
            view->setCurrentIndex(model.index(1, 0));
            layout->addWidget(view);
        }
        table->horizontalHeader()->moveSection(0, 1);
        root.resize(900, 220);
        root.show();
        QVERIFY(QTest::qWaitForWindowExposed(&root));
        const QPixmap image = root.grab();
        QVERIFY(!image.isNull());
        const QString directory = qEnvironmentVariable("ZZ_INDICATOR_REPORT_DIR");
        if (!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(image.save(QDir(directory).filePath(
                QString::fromLatin1(QTest::currentDataTag()) + QStringLiteral(".png"))));
        }
        QCOMPARE(table->selectionModel()->selectedRows().size(), 1);
        QCOMPARE(table->selectionModel()->selectedRows().first().row(), 1);
    }

    void clearingSelectionDoesNotAnimateAnInvalidIncomingIndex()
    {
        QObject owner;
        QStandardItemModel model(3, 1);
        ZzFluentUI::ZzSelectionIndicatorTransition transition(&owner);
        transition.transitionTo(model.index(0, 0), 0);
        transition.transitionTo({}, 200);
        transition.animation()->setCurrentTime(150);
        QCOMPARE(transition.scaleFor({}, false), 0.0);
        QCOMPARE(transition.scaleFor(model.index(0, 0), false), 0.0);
    }

    void rapidRetargetPreservesDisplayedScaleAndAnimation()
    {
        QObject owner;
        QStandardItemModel model(3, 1);
        ZzFluentUI::ZzSelectionIndicatorTransition transition(&owner);
        auto *animation = transition.animation();
        const auto first = model.index(0, 0);
        const auto second = model.index(1, 0);
        transition.transitionTo(first, 0);
        transition.transitionTo(second, 200);
        animation->setCurrentTime(50);
        const qreal visible = transition.scaleFor(first, false);
        QVERIFY(visible > 0.0 && visible < 1.0);
        transition.transitionTo(first, 200);
        QCOMPARE(transition.scaleFor(first, true), visible);
        QCOMPARE(transition.animation(), animation);
        transition.finish();
        QCOMPARE(transition.scaleFor(first, true), 1.0);
        QCOMPARE(transition.scaleFor(second, false), 0.0);
        QCOMPARE(owner.findChildren<QVariantAnimation *>().size(), 1);
    }

    void nativeViewsAnimateSelection_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("fluentDelegate");
        for (int kind = 0; kind < 3; ++kind) {
            for (bool fluent : {false, true}) {
                QTest::newRow(qPrintable(QStringLiteral("view-%1-delegate-%2")
                    .arg(kind).arg(fluent))) << kind << fluent;
            }
        }
    }

    void nativeViewsAnimateSelection()
    {
        QFETCH(int, kind);
        QFETCH(bool, fluentDelegate);
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStandardItemModel model(3, 2);
        std::unique_ptr<QAbstractItemView> view;
        if (kind == 0) {
            view = std::make_unique<QListView>();
        } else if (kind == 1) {
            view = std::make_unique<QTreeView>();
        } else {
            view = std::make_unique<QTableView>();
        }
        view->setStyle(&style);
        view->setModel(&model);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        if (fluentDelegate) {
            view->setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(view.get()));
        }
        view->resize(320, 180);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        (void)view->viewport()->grab();
        auto *selection = view->selectionModel();
        selection->select(model.index(0, 0),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        selection->select(model.index(1, 0),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const auto animations = style.findChildren<QVariantAnimation *>();
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations.first()->state(), QAbstractAnimation::Running);
        QCOMPARE(animations.first()->duration(), 167);
        auto *animation = animations.first();
        const QColor accent = controller.snapshot()->color(ZzFluentUI::ZzColorToken::Accent);
        animation->setCurrentTime(0);
        const int oldPixels = indicatorPixels(view.get(), model.index(0, 0), accent);
        QVERIFY(oldPixels > 0);
        QCOMPARE(indicatorPixels(view.get(), model.index(1, 0), accent), 0);
        saveFrame(view.get(), QStringLiteral("-start"));
        animations.first()->setCurrentTime(83);
        QVERIFY(indicatorPixels(view.get(), model.index(0, 0), accent) < oldPixels);
        QCOMPARE(indicatorPixels(view.get(), model.index(1, 0), accent), 0);
        saveFrame(view.get(), QStringLiteral("-middle"));
        animation->setCurrentTime(167);
        QCOMPARE(indicatorPixels(view.get(), model.index(0, 0), accent), 0);
        QVERIFY(indicatorPixels(view.get(), model.index(1, 0), accent) > 0);
        saveFrame(view.get(), QStringLiteral("-end"));
        QCOMPARE(selection->selectedRows().size(), 1);
        QCOMPARE(selection->selectedRows().first().row(), 1);
        view->edit(model.index(1, 0));
        auto *editor = view->findChild<QLineEdit *>();
        QVERIFY(editor != nullptr);
        QVERIFY(editor->geometry().left() >= view->visualRect(model.index(1, 0)).left() + 10);
        view->hide();
        QCOMPARE(animations.first()->state(), QAbstractAnimation::Stopped);
        view.reset();
        QCOMPARE(style.findChildren<QVariantAnimation *>().size(), 0);
    }
};

QTEST_MAIN(ZzSelectionIndicatorTransitionTest)
#include "ZzSelectionIndicatorTransitionTest.moc"
