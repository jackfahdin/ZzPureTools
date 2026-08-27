#include <QtCore/QHash>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>
#include <QtTest/QTest>

#include <ZzPureTools/ZzWorkspaceActivityId.h>

/** @brief 验证固定 Activity 标识的规范化、比较和 Qt 值类型合同。 */
class ZzWorkspaceActivityIdTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void trimsStableValue()
    {
        const ZzPureTools::ZzWorkspaceActivityId id(
            QStringLiteral("  settings  "));

        QVERIFY(id.isValid());
        QCOMPARE(id.value(), QStringLiteral("settings"));
    }

    void rejectsBlankValue()
    {
        const ZzPureTools::ZzWorkspaceActivityId defaultId;
        const ZzPureTools::ZzWorkspaceActivityId blankId(
            QStringLiteral(" \t\n "));

        QVERIFY(!defaultId.isValid());
        QVERIFY(!blankId.isValid());
        QVERIFY(defaultId.value().isEmpty());
        QVERIFY(blankId.value().isEmpty());
    }

    void comparesAndHashesNormalizedValues()
    {
        const ZzPureTools::ZzWorkspaceActivityId padded(
            QStringLiteral(" settings "));
        const ZzPureTools::ZzWorkspaceActivityId normalized(
            QStringLiteral("settings"));
        const ZzPureTools::ZzWorkspaceActivityId other(
            QStringLiteral("about"));
        QHash<ZzPureTools::ZzWorkspaceActivityId, int> values;
        values.insert(padded, 17);

        QCOMPARE(padded, normalized);
        QVERIFY(padded != other);
        QCOMPARE(qHash(padded), qHash(normalized));
        QCOMPARE(values.value(normalized), 17);
    }

    void roundTripsThroughQVariant()
    {
        const ZzPureTools::ZzWorkspaceActivityId expected(
            QStringLiteral("settings"));
        const QVariant stored = QVariant::fromValue(expected);

        QCOMPARE(
            stored.metaType(),
            QMetaType::fromType<ZzPureTools::ZzWorkspaceActivityId>());
        QCOMPARE(
            stored.value<ZzPureTools::ZzWorkspaceActivityId>(), expected);
    }
};

QTEST_MAIN(ZzWorkspaceActivityIdTest)

#include "ZzWorkspaceActivityIdTest.moc"
