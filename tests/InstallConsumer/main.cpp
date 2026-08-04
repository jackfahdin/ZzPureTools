#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

int main()
{
    if (ZzCore::ZzCoreVersion::toString() != QStringLiteral("0.1.0")) {
        return 1;
    }
    if (ZzWindowKit::ZzWindowKitVersion::toString() != QStringLiteral("0.1.0")) {
        return 2;
    }
    if (ZzFluentUI::ZzFluentVersion::toString() != QStringLiteral("0.1.0")) {
        return 3;
    }
    if (ZzFluentUI::ZzFluentWidgetVersion::toString() != QStringLiteral("0.1.0")) {
        return 4;
    }
    if (ZzPureTools::ZzAppCoreVersion::toString() != QStringLiteral("0.1.0")) {
        return 5;
    }
    if (ZzPureTools::ZzPureToolsVersion::toString() != QStringLiteral("0.1.0")) {
        return 6;
    }
    const ZzWindowKit::ZzWindowAgent windowAgent;
    if (windowAgent.state() != ZzWindowKit::ZzWindowAgentState::Detached) {
        return 7;
    }
    return 0;
}
