#include <QtTest/QtTest>
#include <QtWidgets/QApplication>

#include "../widgets/src/private/ZzWorkspaceTransferRegistryPrivate.h"
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

using namespace ZzFluentUI;

class ZzWorkspaceTransferRegistryPrivateTest final : public QObject
{
    Q_OBJECT
private slots:
    void publishInspectConsumeOnce()
    {
        ZzSplitWorkspace workspace;
        auto *tabs = workspace.tabWidget(workspace.groupIds().constFirst());
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("p"));
        auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
        QVERIFY(registry != nullptr);
        const auto token = registry->publish(
            &workspace, tabs, workspace.groupIds().constFirst(), 0,
            workspace.pageId(page), page);
        QVERIFY(token);
        const auto inspected = registry->inspect(token.value());
        QVERIFY(inspected);
        QVERIFY(registry->consume(token.value()));
        QVERIFY(!registry->consume(token.value()));
    }
};

QTEST_MAIN(ZzWorkspaceTransferRegistryPrivateTest)
#include "ZzWorkspaceTransferRegistryPrivateTest.moc"
