#include <QtTest/QtTest>
#include <QtWidgets/QApplication>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <thread>

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
        QVERIFY(!registry->inspect(QByteArray(18, char(1))));
        QCOMPARE(registry->size(), qsizetype(0));
        ZzWorkspaceTransferRegistryPrivate::resetClockForTesting();
    }

    void invalidSourceAndPayloads()
    {
        ZzSplitWorkspace workspace;
        auto *tabs = workspace.tabWidget(workspace.groupIds().constFirst());
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("p"));
        auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
        const auto token = registry->publish(&workspace, tabs, workspace.groupIds().constFirst(), 0, workspace.pageId(page), page);
        QVERIFY(token);
        tabs->removeTab(0);
        QCOMPARE(registry->size(), qsizetype(0));
        QVERIFY(!registry->inspect(token.value()));

        const auto forged = QByteArray(4096, char(1));
        QVERIFY(!registry->inspect(forged));
        QVERIFY(!registry->consume(QByteArray(17, char(2))));
    }

    void repeatedPublishInvalidateDoesNotCreateObjects()
    {
        auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
        const auto beforeObjects = qApp->findChildren<QObject *>().size();
        const auto beforeTimers = qApp->findChildren<QTimer *>().size();
        ZzSplitWorkspace workspace;
        auto *tabs = workspace.tabWidget(workspace.groupIds().constFirst());
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("p"));
        for (int i = 0; i < 1000; ++i) {
            const auto token = registry->publish(&workspace, tabs, workspace.groupIds().constFirst(), 0, workspace.pageId(page), page);
            QVERIFY(token);
            registry->invalidate(token.value());
        }
        QCOMPARE(registry->size(), qsizetype(0));
        QCOMPARE(qApp->findChildren<QTimer *>().size(), beforeTimers);
        QVERIFY(qApp->findChildren<QObject *>().size() <= beforeObjects + 3);
    }

    void crossThreadRejected()
    {
        auto *registry = ZzWorkspaceTransferRegistryPrivate::instance();
        ZzSplitWorkspace workspace;
        auto *tabs = workspace.tabWidget(workspace.groupIds().constFirst());
        auto *page = new QWidget;
        tabs->addTab(page, QStringLiteral("p"));
        const auto token = registry->publish(&workspace, tabs, workspace.groupIds().constFirst(), 0, workspace.pageId(page), page);
        QVERIFY(token);
        ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate> result =
            ZzCore::ZzResult<ZzWorkspaceTransferRecordPrivate>::failure(
                ZzCore::ZzError(ZzCore::ZzErrorCode::InvalidState, QStringLiteral("未执行")));
        std::thread worker([&] {
            result = registry->inspect(token.value(), &workspace);
        });
        worker.join();
        QVERIFY(!result);
    }
};

QTEST_MAIN(ZzWorkspaceTransferRegistryPrivateTest)
#include "ZzWorkspaceTransferRegistryPrivateTest.moc"
