#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzThemeController themeController;
    ZzFluentUI::ZzPushButton pushButton;
    ZzFluentUI::ZzIconButton iconButton;
    ZzFluentUI::ZzToggleSwitch toggleSwitch;
    ZzFluentUI::ZzMessageBar messageBar;
    ZzFluentUI::ZzNavigationView navigationView;
    ZzFluentUI::ZzBreadcrumbBar breadcrumbBar;
    ZzFluentUI::ZzFluentItemDelegate itemDelegate;
    ZzFluentUI::ZzFluentTitleBar titleBar;

    (void)application;
    (void)themeController;
    (void)pushButton;
    (void)iconButton;
    (void)toggleSwitch;
    (void)messageBar;
    (void)navigationView;
    (void)breadcrumbBar;
    (void)itemDelegate;
    (void)titleBar;
    return 0;
}
