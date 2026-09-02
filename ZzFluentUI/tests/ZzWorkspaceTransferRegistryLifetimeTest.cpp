#include <QtCore/QPointer>
#include <QtWidgets/QApplication>

#include "../widgets/src/private/ZzWorkspaceTransferRegistryPrivate.h"
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

using namespace ZzFluentUI;

int main(int argc, char **argv)
{
    QPointer<QObject> registryGuard;
    QPointer<QWidget> pageGuard;
    QByteArray token;
    {
        QApplication app(argc, argv);
        auto *workspace = new ZzSplitWorkspace;
        auto *tabs = workspace->tabWidget(workspace->groupIds().constFirst());
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("lifetime"));

        auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
        if (registry == nullptr || registry->parent() != &app) {
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
            return 2;
        }
        token = published.value();
        registryGuard = registry;
        pageGuard = page;
        delete workspace;
        if (!pageGuard.isNull() || registry->inspect(token)) {
            return 3;
        }
    }

    return registryGuard.isNull() && pageGuard.isNull() ? 0 : 4;
}
