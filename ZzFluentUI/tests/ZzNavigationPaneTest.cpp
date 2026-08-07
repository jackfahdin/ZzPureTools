#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzNavigationDisplayMode.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>
#include <ZzFluentUI/ZzNavigationView.h>

namespace {

/** @brief 创建带可选分区、徽标和位置的本地导航项。 */
QStandardItem *zzNavigationItem(
    const QString &title,
    const QString &section = {},
    const QString &badge = {},
    ZzFluentUI::ZzNavigationPlacement placement =
        ZzFluentUI::ZzNavigationPlacement::Primary)
{
    auto *item = new QStandardItem(title);
    item->setData(
        section,
        static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Section));
    item->setData(
        badge,
        static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge));
    item->setData(
        QVariant::fromValue(placement),
        static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Placement));
    return item;
}

/** @brief 按 footer placement 区分面板拥有的两个固定 view。 */
struct ZzNavigationViews final
{
    ZzFluentUI::ZzNavigationView *primary = nullptr;
    ZzFluentUI::ZzNavigationView *footer = nullptr;
};

/** @brief 从固定双视图中识别主区和页脚区。 */
ZzNavigationViews zzNavigationViews(ZzFluentUI::ZzNavigationPane *pane)
{
    ZzNavigationViews result;
    const auto views = pane->findChildren<ZzFluentUI::ZzNavigationView *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (ZzFluentUI::ZzNavigationView *view : views) {
        if (view->model()->rowCount() == 1) {
            result.footer = view;
        } else {
            result.primary = view;
        }
    }
    return result;
}

} // namespace

/** @brief 验证导航面板的固定投影、索引映射和自适应生命周期。 */
class ZzNavigationPaneTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsSectionAndFooterWithoutOwningSourceModel()
    {
        QStandardItemModel model;
        model.appendRow(zzNavigationItem(
            QStringLiteral("Home"), QStringLiteral("Workspace")));
        model.appendRow(zzNavigationItem(QStringLiteral("Projects")));
        model.appendRow(zzNavigationItem(
            QStringLiteral("Settings"), {}, QStringLiteral("3"),
            ZzFluentUI::ZzNavigationPlacement::Footer));
        ZzFluentUI::ZzNavigationPane pane;
        pane.setDisplayMode(ZzFluentUI::ZzNavigationDisplayMode::Regular);
        pane.setModel(&model);
        pane.resize(240, 320);
        pane.show();
        QCoreApplication::processEvents();

        QCOMPARE(pane.model(), &model);
        const ZzNavigationViews views = zzNavigationViews(&pane);
        QVERIFY(views.primary != nullptr);
        QVERIFY(views.footer != nullptr);
        if (views.primary == nullptr || views.footer == nullptr) {
            return;
        }
        QCOMPARE(views.primary->model()->rowCount(), 3);
        QCOMPARE(views.footer->model()->rowCount(), 1);
        QCOMPARE(
            views.primary->model()->index(0, 0).data().toString(),
            QStringLiteral("Workspace"));
        QCOMPARE(
            views.primary->model()->flags(
                views.primary->model()->index(0, 0)),
            Qt::NoItemFlags);

        QSignalSpy navigationSpy(
            &pane, &ZzFluentUI::ZzNavigationPane::navigationRequested);
        const QModelIndex sectionIndex =
            views.primary->model()->index(0, 0);
        const QModelIndex primaryIndex =
            views.primary->model()->index(1, 0);
        const QModelIndex footerIndex =
            views.footer->model()->index(0, 0);

        QTest::mouseClick(
            views.primary->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            views.primary->visualRect(sectionIndex).center());
        QCOMPARE(navigationSpy.count(), 0);
        QTest::mouseClick(
            views.primary->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            views.primary->visualRect(primaryIndex).center());
        QCOMPARE(navigationSpy.count(), 1);
        QCOMPARE(
            navigationSpy.takeFirst().at(0).value<QModelIndex>(),
            model.index(0, 0));
        QTest::mouseClick(
            views.footer->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            views.footer->visualRect(footerIndex).center());
        QCOMPARE(navigationSpy.count(), 1);
        QCOMPARE(
            navigationSpy.takeFirst().at(0).value<QModelIndex>(),
            model.index(2, 0));

        views.primary->setCurrentIndex(primaryIndex);
        QTest::keyClick(views.primary, Qt::Key_Return);
        QTest::keyClick(views.primary, Qt::Key_Space);
        QCOMPARE(navigationSpy.count(), 2);
    }

    void mapsProgrammaticSelectionAcrossBothViews()
    {
        QStandardItemModel model;
        model.appendRow(zzNavigationItem(
            QStringLiteral("Home"), QStringLiteral("Workspace")));
        model.appendRow(zzNavigationItem(QStringLiteral("Projects")));
        model.appendRow(zzNavigationItem(
            QStringLiteral("Settings"), {}, {},
            ZzFluentUI::ZzNavigationPlacement::Footer));
        ZzFluentUI::ZzNavigationPane pane;
        pane.setModel(&model);
        const ZzNavigationViews views = zzNavigationViews(&pane);
        QVERIFY(views.primary != nullptr);
        QVERIFY(views.footer != nullptr);
        if (views.primary == nullptr || views.footer == nullptr) {
            return;
        }

        pane.setCurrentSourceIndex(model.index(1, 0));
        QCOMPARE(pane.currentSourceIndex(), model.index(1, 0));
        QCOMPARE(views.primary->currentIndex().row(), 2);
        QVERIFY(!views.footer->currentIndex().isValid());

        pane.setCurrentSourceIndex(model.index(2, 0));
        QCOMPARE(pane.currentSourceIndex(), model.index(2, 0));
        QVERIFY(!views.primary->currentIndex().isValid());
        QCOMPARE(views.footer->currentIndex().row(), 0);

        pane.setCurrentSourceIndex({});
        QVERIFY(!pane.currentSourceIndex().isValid());
        QVERIFY(!views.primary->currentIndex().isValid());
        QVERIFY(!views.footer->currentIndex().isValid());
    }

    void rebuildsOnlyForProjectionMetadataChanges()
    {
        QStandardItemModel model;
        model.appendRow(zzNavigationItem(QStringLiteral("Home")));
        model.appendRow(zzNavigationItem(QStringLiteral("Settings")));
        ZzFluentUI::ZzNavigationPane pane;
        pane.setModel(&model);
        auto views = pane.findChildren<ZzFluentUI::ZzNavigationView *>(
            QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(views.size(), 2);
        QAbstractItemModel *primaryModel = nullptr;
        QAbstractItemModel *footerModel = nullptr;
        for (auto *view : views) {
            if (view->model()->rowCount() == 2) {
                primaryModel = view->model();
            } else {
                footerModel = view->model();
            }
        }
        QVERIFY(primaryModel != nullptr);
        QVERIFY(footerModel != nullptr);
        if (primaryModel == nullptr || footerModel == nullptr) {
            return;
        }
        QSignalSpy primaryResetSpy(
            primaryModel, &QAbstractItemModel::modelReset);
        QSignalSpy footerResetSpy(
            footerModel, &QAbstractItemModel::modelReset);

        QVERIFY(model.setData(
            model.index(0, 0),
            QStringLiteral("9"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge)));
        QCOMPARE(primaryResetSpy.count(), 0);
        QCOMPARE(footerResetSpy.count(), 0);

        QVERIFY(model.setData(
            model.index(1, 0),
            QVariant::fromValue(ZzFluentUI::ZzNavigationPlacement::Footer),
            static_cast<int>(
                ZzFluentUI::ZzNavigationItemRole::Placement)));
        QVERIFY(primaryResetSpy.count() >= 1);
        QVERIFY(footerResetSpy.count() >= 1);
        QCOMPARE(primaryModel->rowCount(), 1);
        QCOMPARE(footerModel->rowCount(), 1);
    }

    void tracksAdaptiveTopLevelWidthWithoutAnimation()
    {
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *pane = new ZzFluentUI::ZzNavigationPane(&window);
        layout->addWidget(pane);
        pane->setAdaptiveThreshold(900);
        window.resize(1000, 600);
        window.show();
        QCoreApplication::processEvents();

        QCOMPARE(
            pane->displayMode(),
            ZzFluentUI::ZzNavigationDisplayMode::Adaptive);
        QVERIFY(!pane->isCompact());
        QCOMPARE(pane->width(), 240);
        QCOMPARE(pane->findChildren<QAbstractAnimation *>().size(), 0);

        window.resize(800, 600);
        QCoreApplication::processEvents();
        QVERIFY(pane->isCompact());
        QCOMPARE(pane->width(), 48);

        pane->setDisplayMode(ZzFluentUI::ZzNavigationDisplayMode::Regular);
        QVERIFY(!pane->isCompact());
        QCOMPARE(pane->width(), 240);
        pane->setDisplayMode(ZzFluentUI::ZzNavigationDisplayMode::Compact);
        QVERIFY(pane->isCompact());
        QCOMPARE(pane->width(), 48);
        QCOMPARE(pane->findChildren<QAbstractAnimation *>().size(), 0);
    }

    void clearsProjectionsWhenSourceModelIsDestroyed()
    {
        ZzFluentUI::ZzNavigationPane pane;
        QSignalSpy modelSpy(
            &pane, &ZzFluentUI::ZzNavigationPane::modelChanged);
        auto model = std::make_unique<QStandardItemModel>();
        model->appendRow(zzNavigationItem(QStringLiteral("Home")));
        pane.setModel(model.get());
        QCOMPARE(modelSpy.count(), 1);

        model.reset();
        QCOMPARE(pane.model(), nullptr);
        QCOMPARE(modelSpy.count(), 2);
        const auto views = pane.findChildren<ZzFluentUI::ZzNavigationView *>(
            QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(views.size(), 2);
        for (const auto *view : views) {
            QCOMPARE(view->model()->rowCount(), 0);
        }
    }

    void keepsLargeModelLayoutAndObjectCountBounded()
    {
        QStandardItemModel model;
        model.setRowCount(100000);
        model.setData(model.index(0, 0), QStringLiteral("First"));
        model.setData(
            model.index(0, 0),
            QStringLiteral("Section"),
            static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Section));
        model.setData(
            model.index(99999, 0),
            QVariant::fromValue(ZzFluentUI::ZzNavigationPlacement::Footer),
            static_cast<int>(
                ZzFluentUI::ZzNavigationItemRole::Placement));
        ZzFluentUI::ZzNavigationPane pane;
        pane.setModel(&model);
        const qsizetype initialDescendants =
            pane.findChildren<QObject *>().size();
        const auto views = pane.findChildren<ZzFluentUI::ZzNavigationView *>(
            QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(views.size(), 2);
        for (const auto *view : views) {
            QVERIFY(view->uniformItemSizes());
            QCOMPARE(view->layoutMode(), QListView::Batched);
            QCOMPARE(view->batchSize(), 64);
        }

        for (int iteration = 0; iteration < 1000; ++iteration) {
            pane.setDisplayMode((iteration % 2) == 0
                ? ZzFluentUI::ZzNavigationDisplayMode::Regular
                : ZzFluentUI::ZzNavigationDisplayMode::Compact);
            pane.setLayoutDirection((iteration % 2) == 0
                ? Qt::LeftToRight : Qt::RightToLeft);
            pane.setCurrentSourceIndex(model.index(iteration % 99999, 0));
        }
        QCOMPARE(pane.findChildren<QObject *>().size(), initialDescendants);
        QCOMPARE(pane.findChildren<QAbstractAnimation *>().size(), 0);
    }
};

QTEST_MAIN(ZzNavigationPaneTest)

#include "ZzNavigationPaneTest.moc"
