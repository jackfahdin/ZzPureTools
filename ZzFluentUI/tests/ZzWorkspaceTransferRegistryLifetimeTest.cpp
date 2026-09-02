#include <QtCore/QPointer>
#include <QtWidgets/QApplication>

#include "../widgets/src/private/ZzWorkspaceTransferRegistryPrivate.h"
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

using namespace ZzFluentUI;

int main(int argc, char **argv)
{
    QPointer<QObject> registryGuard;
    QPointer<ZzSplitWorkspace> workspaceGuard;
    QPointer<QWidget> pageGuard;
    QByteArray token;
    auto *app = new QApplication(argc, argv);
    auto *workspace = new ZzSplitWorkspace;
    auto *tabs = workspace->tabWidget(workspace->groupIds().constFirst());
    auto *page = new QWidget;
    tabs->addTab(page, QStringLiteral("lifetime"));

    auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
    if (registry == nullptr || registry->parent() != app) {
        delete workspace;
        delete app;
        return 1;
    }
    const auto published = registry->publish(
        workspace,
        tabs,
        workspace->groupIds().constFirst(),
        0,
        workspace->pageId(page),
        page);
    if (!published || !registry->inspect(published.value())) {
        delete workspace;
        delete app;
        return 2;
    }
    token = published.value();
    registryGuard = registry;
    workspaceGuard = workspace;
    pageGuard = page;

    delete app;
    if (!registryGuard.isNull() || workspaceGuard.isNull()
        || pageGuard.isNull()) {
        delete workspace;
        return 3;
    }

    // 不在 QApplication 销毁后调用 registry API；仅验证对象仍可安全析构。
    delete workspace;

    return registryGuard.isNull() && workspaceGuard.isNull()
            && pageGuard.isNull()
        ? 0
        : 4;
}
