#include <type_traits>

#include <QtCore/QHash>
#include <QtTest/QTest>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzRouteId.h>

/** @brief 验证模块与路由标识的拥有型值语义及模块接口边界。 */
class ZzModuleGraphTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void idsAreOwningAndStronglyTyped()
    {
        QString source = QStringLiteral("  settings  ");
        const ZzPureTools::ZzModuleId module(source);
        const ZzPureTools::ZzRouteId route(source);
        source.clear();

        QCOMPARE(module.value(), QStringLiteral("settings"));
        QCOMPARE(route.value(), QStringLiteral("settings"));
        QVERIFY(module.isValid());
        QVERIFY(route.isValid());
        static_assert(!std::is_same_v<
            ZzPureTools::ZzModuleId,
            ZzPureTools::ZzRouteId>);
        static_assert(!std::is_convertible_v<
            ZzPureTools::ZzModuleId,
            ZzPureTools::ZzRouteId>);
        static_assert(std::is_abstract_v<
            ZzPureTools::ZzApplicationModule>);

        QHash<ZzPureTools::ZzModuleId, int> modules;
        modules.insert(module, 1);
        QCOMPARE(
            modules.value(ZzPureTools::ZzModuleId(
                QStringLiteral("settings"))),
            1);
    }

    void emptyIdsAreInvalid()
    {
        QVERIFY(!ZzPureTools::ZzModuleId().isValid());
        QVERIFY(!ZzPureTools::ZzModuleId(QStringLiteral(" \t ")).isValid());
        QVERIFY(!ZzPureTools::ZzRouteId(QStringLiteral("   ")).isValid());
    }
};

QTEST_GUILESS_MAIN(ZzModuleGraphTest)

#include "ZzModuleGraphTest.moc"
