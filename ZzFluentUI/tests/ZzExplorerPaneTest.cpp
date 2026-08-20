#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <QtCore/QTimer>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeView>

#include <ZzFluentUI/ZzExplorerPane.h>

class ZzExplorerPaneTest final : public QObject
{
    Q_OBJECT
private:
    static void populateTree(QStandardItemModel &model)
    {
        auto *folder = new QStandardItem(QStringLiteral("Folder"));
        folder->appendRow(new QStandardItem(QStringLiteral("needle.txt")));
        folder->appendRow(new QStandardItem(QStringLiteral("prefix-item")));
        model.appendRow(folder);
        model.appendRow(new QStandardItem(QStringLiteral("other")));
    }
private slots:
    void replacingModelThenDestroyingOldModelKeepsCurrentModel()
    {
        auto *first = new QStandardItemModel;
        first->appendRow(new QStandardItem(QStringLiteral("first")));
        auto *second = new QStandardItemModel;
        second->appendRow(new QStandardItem(QStringLiteral("second")));
        ZzFluentUI::ZzExplorerPane pane;
        pane.setModel(first);
        pane.setModel(second);
        delete first;
        QCOMPARE(pane.model(), second);
        QCOMPARE(pane.treeView()->model()->rowCount(), 1);
        QCOMPARE(pane.treeView()->model()->index(0, 0).data().toString(), QStringLiteral("second"));
        delete second;
    }

    void recursiveExactPrefixContainsAndSourceMapping()
    {
        QStandardItemModel model;
        populateTree(model);
        ZzFluentUI::ZzExplorerPane pane;
        pane.setModel(&model);
        pane.setSearchDelay(0);
        for (const QString &query : {QStringLiteral("needle.txt"), QStringLiteral("prefix"), QStringLiteral("fix-it")}) {
            pane.setSearchText(query);
            QTRY_COMPARE(pane.treeView()->model()->rowCount(), 1);
            const QModelIndex proxy = pane.treeView()->model()->index(0, 0);
            QCOMPARE(pane.sourceIndex(proxy).data().toString(), QStringLiteral("Folder"));
            QVERIFY(pane.proxyIndex(model.index(0, 0)).isValid());
        }
        pane.setSearchText({});
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 2);
    }

    void persistentSixtyMillisecondDebounceAndModelLifecycle()
    {
        QStandardItemModel model;
        populateTree(model);
        ZzFluentUI::ZzExplorerPane pane;
        pane.setModel(&model);
        QCOMPARE(pane.searchDelay(), 60);
        QCOMPARE(pane.findChildren<QTimer *>().size(), 1);
        QSignalSpy spy(&pane, &ZzFluentUI::ZzExplorerPane::searchTextChanged);
        pane.setSearchText(QStringLiteral("n"));
        pane.setSearchText(QStringLiteral("ne"));
        QTRY_COMPARE_WITH_TIMEOUT(pane.treeView()->model()->rowCount(), 1, 250);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(pane.findChildren<QTimer *>().size(), 1);
        model.appendRow(new QStandardItem(QStringLiteral("new needle")));
        pane.setSearchDelay(0);
        pane.setSearchText(QStringLiteral("needle"));
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 2);
        model.removeRow(2);
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 1);
        model.setData(model.index(1, 0), QStringLiteral("needle changed"));
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 2);
        model.clear();
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 0);
    }

    void modelDestructionAndHundredThousandNodeObjectBudget()
    {
        ZzFluentUI::ZzExplorerPane pane;
        auto *temporary = new QStandardItemModel;
        temporary->appendRow(new QStandardItem(QStringLiteral("needle")));
        pane.setModel(temporary);
        delete temporary;
        QCOMPARE(pane.model(), nullptr);
        QCOMPARE(pane.treeView()->model()->rowCount(), 0);

        QStandardItemModel model;
        for (int row = 0; row < 100000; ++row) model.appendRow(new QStandardItem(QStringLiteral("node-%1").arg(row)));
        pane.setModel(&model);
        pane.setSearchDelay(0);
        const qsizetype widgetsBefore = QApplication::allWidgets().size();
        const qsizetype timersBefore = pane.findChildren<QTimer *>().size();
        pane.setSearchText(QStringLiteral("node-99999"));
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 1);
        pane.setSearchText(QStringLiteral("absent"));
        QTRY_COMPARE(pane.treeView()->model()->rowCount(), 0);
        QCOMPARE(QApplication::allWidgets().size(), widgetsBefore);
        QCOMPARE(pane.findChildren<QTimer *>().size(), timersBefore);
    }
};

QTEST_MAIN(ZzExplorerPaneTest)
#include "ZzExplorerPaneTest.moc"
