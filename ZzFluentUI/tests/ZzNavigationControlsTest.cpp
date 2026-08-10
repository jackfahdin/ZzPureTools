#include <QtCore/QCoreApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzThemeController.h>

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

} // namespace

/** @brief 验证导航控件只消费展示模型并保留逻辑索引意图。 */
class ZzNavigationControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
        view.resize(240, 48);
        view.show();
        QCoreApplication::processEvents();

        QImage regular(view.size(), QImage::Format_ARGB32_Premultiplied);
        regular.fill(Qt::transparent);
        QPainter regularPainter(&regular);
        view.render(&regularPainter);
        regularPainter.end();
        QVERIFY(style.iconCacheBytes() > 0);
        QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Workspace"));
        QCOMPARE(
            model.index(0, 0)
                .data(static_cast<int>(
                    ZzFluentUI::ZzNavigationItemRole::Badge))
                .toString(),
            QStringLiteral("9"));

        view.setCompact(true);
        QImage compact(view.size(), QImage::Format_ARGB32_Premultiplied);
        compact.fill(Qt::transparent);
        QPainter compactPainter(&compact);
        view.render(&compactPainter);
        compactPainter.end();
        QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Workspace"));
        QVERIFY(regular != compact);
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
