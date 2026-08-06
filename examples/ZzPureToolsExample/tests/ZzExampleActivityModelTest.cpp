#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "../ZzExampleActivityModel.h"

/** @brief 验证示例共享活动模型的有界存储与标准模型语义。 */
class ZzExampleActivityModelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /** @brief 验证非法容量收敛、空白忽略和角色数据。 */
    void normalizesCapacityAndText();

    /** @brief 验证超过容量时按顺序移除最旧记录。 */
    void evictsOldestRecordAtCapacity();

    /** @brief 验证 clear 发出一次 reset 且空模型重复清理无信号。 */
    void clearsModelIdempotently();
};

void ZzExampleActivityModelTest::normalizesCapacityAndText()
{
    ZzExample::ZzExampleActivityModel model(0);
    QSignalSpy inserted(
        &model, &QAbstractItemModel::rowsInserted);

    model.append(QStringLiteral("   "));
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(inserted.count(), 0);

    model.append(QStringLiteral("  build    ready  "));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(inserted.count(), 1);
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(
        model.data(index, Qt::DisplayRole).toString(),
        QStringLiteral("build ready"));
    QCOMPARE(
        model.data(index, Qt::AccessibleTextRole),
        model.data(index, Qt::DisplayRole));
    QCOMPARE(
        model.data(index, Qt::ToolTipRole),
        model.data(index, Qt::DisplayRole));
    QVERIFY(!model.data(index, Qt::DecorationRole).isValid());
    QCOMPARE(model.rowCount(index), 0);

    model.append(QStringLiteral("replacement"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(
        model.data(model.index(0, 0)).toString(),
        QStringLiteral("replacement"));
}

void ZzExampleActivityModelTest::evictsOldestRecordAtCapacity()
{
    ZzExample::ZzExampleActivityModel model(2);
    QSignalSpy inserted(
        &model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(
        &model, &QAbstractItemModel::rowsRemoved);

    model.append(QStringLiteral("first"));
    model.append(QStringLiteral("second"));
    model.append(QStringLiteral("third"));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(inserted.count(), 3);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.at(0).at(1).toInt(), 0);
    QCOMPARE(removed.at(0).at(2).toInt(), 0);
    QCOMPARE(
        model.data(model.index(0, 0)).toString(),
        QStringLiteral("second"));
    QCOMPARE(
        model.data(model.index(1, 0)).toString(),
        QStringLiteral("third"));
}

void ZzExampleActivityModelTest::clearsModelIdempotently()
{
    ZzExample::ZzExampleActivityModel model(3);
    model.append(QStringLiteral("one"));
    model.append(QStringLiteral("two"));
    QSignalSpy resets(
        &model, &QAbstractItemModel::modelReset);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(resets.count(), 1);

    model.clear();
    QCOMPARE(resets.count(), 1);
}

QTEST_APPLESS_MAIN(ZzExampleActivityModelTest)

#include "ZzExampleActivityModelTest.moc"
