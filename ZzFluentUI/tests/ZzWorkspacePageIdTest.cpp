#include <QtTest/QtTest>

#include <QtCore/QObject>
#include <QtTest/QSignalSpy>

#include <ZzFluentUI/ZzWorkspacePageId.h>

namespace {

class TestEmitter final : public QObject
{
    Q_OBJECT

signals:
    void pageChanged(ZzFluentUI::ZzWorkspacePageId id);
};

} // namespace

class ZzWorkspacePageIdTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsStableRoundTrippableIds()
    {
        const auto first = ZzFluentUI::ZzWorkspacePageId::create();
        const auto second = ZzFluentUI::ZzWorkspacePageId::create();

        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        QVERIFY(first != second);
        QCOMPARE(
            ZzFluentUI::ZzWorkspacePageId::fromString(first.toString()),
            first);
        QVERIFY(!ZzFluentUI::ZzWorkspacePageId::fromString({}).isValid());
    }

    void rejectsNonCanonicalStrings()
    {
        const auto id = ZzFluentUI::ZzWorkspacePageId::create();
        const auto canonical = id.toString();

        QVERIFY(
            ZzFluentUI::ZzWorkspacePageId::fromString(
                QStringLiteral("{") + canonical + QStringLiteral("}"))
                .isValid());
        QVERIFY(
            ZzFluentUI::ZzWorkspacePageId::fromString(canonical.toUpper())
                .isValid());
        QVERIFY(!ZzFluentUI::ZzWorkspacePageId::fromString(
                       QStringLiteral("not-a-uuid"))
                     .isValid());
        QVERIFY(!ZzFluentUI::ZzWorkspacePageId::fromString(
                       QStringLiteral("{") + canonical)
                     .isValid());
        QVERIFY(!ZzFluentUI::ZzWorkspacePageId::fromString(
                       canonical + QStringLiteral("x"))
                     .isValid());
    }

    void hashesAndEqualityAgree()
    {
        const auto id = ZzFluentUI::ZzWorkspacePageId::create();
        const auto same = ZzFluentUI::ZzWorkspacePageId::fromString(
            id.toString());

        QCOMPARE(id, same);
        QCOMPARE(qHash(id), qHash(same));
        QVERIFY(qHash(id, 42) == qHash(same, 42));
    }

    void canBeCapturedBySignalSpy()
    {
        TestEmitter emitter;
        QSignalSpy spy(&emitter, &TestEmitter::pageChanged);
        QVERIFY(spy.isValid());

        const auto id = ZzFluentUI::ZzWorkspacePageId::create();
        emit emitter.pageChanged(id);

        QCOMPARE(spy.size(), 1);
        QCOMPARE(
            spy.constFirst().constFirst()
                .value<ZzFluentUI::ZzWorkspacePageId>(),
            id);
    }
};

QTEST_MAIN(ZzWorkspacePageIdTest)
#include "ZzWorkspacePageIdTest.moc"
