#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>

#include <ZzFluentUI/ZzCommandItemRole.h>
#include <ZzFluentUI/ZzCommandPalette.h>

namespace {

class ZzCountingCommandModel final : public QStandardItemModel
{
public:
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        ++dataReads;
        return QStandardItemModel::data(index, role);
    }

    mutable int dataReads = 0;
};

} // namespace

class ZzCommandPaletteTest final : public QObject
{
    Q_OBJECT
private:
    static QStandardItem *command(QString name, QStringList words = {}, int priority = 0)
    {
        auto *item = new QStandardItem(std::move(name));
        item->setData(std::move(words), static_cast<int>(ZzFluentUI::ZzCommandItemRole::Keywords));
        item->setData(priority, static_cast<int>(ZzFluentUI::ZzCommandItemRole::Priority));
        return item;
    }
private slots:
    void destroyingCurrentModelNotifiesThatTheModelWasCleared()
    {
        auto *model = new QStandardItemModel;
        model->appendRow(command(QStringLiteral("current")));
        ZzFluentUI::ZzCommandPalette palette;
        palette.setModel(model);
        QSignalSpy modelChangedSpy(
            &palette, &ZzFluentUI::ZzCommandPalette::modelChanged);

        delete model;

        QCOMPARE(palette.model(), nullptr);
        QCOMPARE(palette.resultCount(), 0);
        QCOMPARE(modelChangedSpy.count(), 1);
        QCOMPARE(
            modelChangedSpy.at(0).at(0).value<QAbstractItemModel *>(),
            nullptr);
    }

    void replacingModelThenDestroyingOldModelKeepsCurrentModel()
    {
        auto *first = new QStandardItemModel;
        first->appendRow(command(QStringLiteral("first")));
        auto *second = new QStandardItemModel;
        second->appendRow(command(QStringLiteral("second")));
        ZzFluentUI::ZzCommandPalette palette;
        palette.setModel(first);
        palette.setModel(second);
        delete first;
        QCOMPARE(palette.model(), second);
        QCOMPARE(palette.resultCount(), 1);
        QCOMPARE(palette.resultView()->model()->index(0, 0).data().toString(), QStringLiteral("second"));
        delete second;
    }

    void ignoresChangesFromAnOldModelThatRemainsAlive()
    {
        QStandardItemModel oldModel;
        oldModel.appendRow(command(QStringLiteral("old command")));
        ZzCountingCommandModel currentModel;
        currentModel.appendRow(command(QStringLiteral("current command")));
        ZzFluentUI::ZzCommandPalette palette;
        palette.setModel(&oldModel);
        palette.setModel(&currentModel);
        palette.setQuery(QStringLiteral("current"));
        QCOMPARE(palette.resultCount(), 1);
        currentModel.dataReads = 0;

        oldModel.item(0)->setText(QStringLiteral("changed old command"));
        QCoreApplication::processEvents();

        QCOMPARE(palette.model(), &currentModel);
        QCOMPARE(palette.resultCount(), 1);
        QCOMPARE(currentModel.dataReads, 0);
    }

    void structuralChangesRefreshCurrentQuery()
    {
        QStandardItemModel model;
        model.appendRow(command(QStringLiteral("open one")));
        ZzFluentUI::ZzCommandPalette palette;
        palette.setModel(&model);
        palette.setQuery(QStringLiteral("open"));
        QCOMPARE(palette.resultCount(), 1);
        model.insertRow(0, command(QStringLiteral("open zero")));
        QTRY_COMPARE(palette.resultCount(), 2);
        model.removeRow(1);
        QTRY_COMPARE(palette.resultCount(), 1);
        QCOMPARE(palette.resultView()->model()->index(0, 0).data().toString(), QStringLiteral("open zero"));
    }

    void ranksFiveMatchLevelsPriorityAndStableSourceRow()
    {
        QStandardItemModel model;
        model.appendRow(command(QStringLiteral("open"), {}, 0));
        model.appendRow(command(QStringLiteral("open file"), {}, 0));
        model.appendRow(command(QStringLiteral("file open recent"), {}, 0));
        model.appendRow(command(QStringLiteral("reopen"), {}, 0));
        model.appendRow(command(QStringLiteral("close"), {QStringLiteral("open-anything")}, 100));
        model.appendRow(command(QStringLiteral("open second"), {}, 10));
        ZzFluentUI::ZzCommandPalette palette;
        palette.setModel(&model);
        palette.setQuery(QStringLiteral("open"));
        QCOMPARE(palette.resultCount(), 6);
        const auto *results = palette.resultView()->model();
        QCOMPARE(results->index(0, 0).data().toString(), QStringLiteral("open"));
        QCOMPARE(results->index(1, 0).data().toString(), QStringLiteral("open second"));
        QCOMPARE(results->index(2, 0).data().toString(), QStringLiteral("open file"));
        QCOMPARE(results->index(3, 0).data().toString(), QStringLiteral("file open recent"));
        QCOMPARE(results->index(4, 0).data().toString(), QStringLiteral("reopen"));
        QCOMPARE(results->index(5, 0).data().toString(), QStringLiteral("close"));
    }

    void tenThousandCommandsLengthLimitDisabledFocusAndObjectBudget()
    {
        QWidget workspace;
        QLineEdit initial(&workspace);
        workspace.show();
        initial.setFocus();
        QStandardItemModel model;
        for (int row = 0; row < 10000; ++row) model.appendRow(command(QStringLiteral("Command %1").arg(row), {QStringLiteral("action-%1").arg(row)}, row % 7));
        model.item(9999)->setEnabled(false);
        ZzFluentUI::ZzCommandPalette palette(&workspace);
        palette.setModel(&model);
        const qsizetype widgetsBefore = QApplication::allWidgets().size();
        const qsizetype objectsBefore = palette.findChildren<QObject *>().size();
        palette.open();
        QVERIFY(palette.isOpen());
        QTRY_COMPARE(QApplication::focusWidget(), palette.searchEdit());
        palette.setQuery(QString(600, u'x'));
        QCOMPARE(palette.query().size(), 512);
        palette.setQuery(QStringLiteral("command 9999"));
        QCOMPARE(palette.resultCount(), 1);
        QVERIFY(!palette.activateCurrent());
        QTest::keyClick(palette.searchEdit(), Qt::Key_Escape);
        QVERIFY(!palette.isOpen());
        QTRY_COMPARE(QApplication::focusWidget(), &initial);
        QSignalSpy activated(&palette, &ZzFluentUI::ZzCommandPalette::commandActivated);
        palette.open();
        palette.setQuery(QStringLiteral("command 1"));
        QVERIFY(palette.activateCurrent());
        QCOMPARE(activated.count(), 1);
        palette.open();
        QTest::mouseClick(&palette, Qt::LeftButton, Qt::NoModifier,
                          QPoint(palette.width() - 1, palette.height() - 1));
        QVERIFY(!palette.isOpen());
        QCOMPARE(QApplication::allWidgets().size(), widgetsBefore);
        QCOMPARE(palette.findChildren<QObject *>().size(), objectsBefore);
    }
};

QTEST_MAIN(ZzCommandPaletteTest)
#include "ZzCommandPaletteTest.moc"
