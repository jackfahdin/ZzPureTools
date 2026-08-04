#include "ZzVersionApiTest.h"

#include <QtTest/QTest>

#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

void ZzVersionApiTest::reportsProjectVersion()
{
    QCOMPARE(ZzCore::ZzCoreVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzWindowKit::ZzWindowKitVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzFluentUI::ZzFluentVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzFluentUI::ZzFluentWidgetVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzPureTools::ZzAppCoreVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzPureTools::ZzPureToolsVersion::toString(), QStringLiteral("0.1.0"));
}
