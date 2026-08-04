#include <memory>

#include <QtWidgets/QApplication>

#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowAgentState.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
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
    ZzFluentUI::ZzThemeController controller;
    controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
    auto style = std::make_unique<ZzFluentUI::ZzFluentStyle>(
        &controller);
    if (style->themeRevision() != controller.snapshot()->revision()) {
        return 8;
    }
    return 0;
}
