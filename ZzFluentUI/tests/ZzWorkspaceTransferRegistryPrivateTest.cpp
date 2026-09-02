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

    void expiryAndForgery()
    {
        using Clock = ZzWorkspaceTransferRegistryPrivate::Clock;
        auto now = std::chrono::steady_clock::time_point{};
        ZzWorkspaceTransferRegistryPrivate::setClockForTesting([&] { return now; });
        ZzSplitWorkspace workspace;
        auto *tabs = workspace.tabWidget(workspace.groupIds().constFirst());
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("p"));
        auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
        const auto token = registry->publish(&workspace, tabs, workspace.groupIds().constFirst(), 0, workspace.pageId(page), page);
        QVERIFY(token);
        now += std::chrono::seconds(5);
        QVERIFY(!registry->inspect(token.value()));
        QVERIFY(!registry->inspect(QByteArray(18, '\\x01')));
        QCOMPARE(registry->size(), qsizetype(0));
        ZzWorkspaceTransferRegistryPrivate::resetClockForTesting();
    }
};

QTEST_MAIN(ZzWorkspaceTransferRegistryPrivateTest)
#include "ZzWorkspaceTransferRegistryPrivateTest.moc"
