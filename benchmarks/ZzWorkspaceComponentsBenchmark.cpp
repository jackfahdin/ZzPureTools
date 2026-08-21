#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtGui/QStandardItemModel>
#include <QtGui/QImage>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzCommandItemRole.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzExplorerPane.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTitleBarMenuDisplayMode.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 80;
constexpr int zzStateToggleIterations = 1000;
constexpr int zzExplorerNodeCount = 100000;
constexpr int zzCommandCount = 10000;
constexpr int zzTabCount = 200;
constexpr int zzSidePanelCount = 64;
constexpr int zzDockPanelCount = 32;
// QListView 的 viewport 与滚动条需要固定少量 QWidget；每个结果一行的实现
// 会在 10,000 条命令下远超此预算。
constexpr qsizetype zzMaximumResultViewWidgets = 8;

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出工作区基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 将纳秒计时转换为性能报告使用的毫秒。 */
double zzMilliseconds(qint64 nanoseconds)
{
    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    return static_cast<double>(nanoseconds) / nanosecondsPerMillisecond;
}

/** @brief 添加固定、无业务依赖的命令模型行。 */
void zzPopulateCommands(QStandardItemModel *model)
{
    for (int index = 0; index < zzCommandCount; ++index) {
        auto *item = new QStandardItem(
            QStringLiteral("Workspace command %1").arg(index));
        item->setData(
            QStringList{QStringLiteral("workspace"),
                        QStringLiteral("action-%1").arg(index)},
            static_cast<int>(ZzFluentUI::ZzCommandItemRole::Keywords));
        item->setData(
            index % 11,
            static_cast<int>(ZzFluentUI::ZzCommandItemRole::Priority));
        model->appendRow(item);
    }
}

/** @brief 添加固定、无业务依赖的资源树模型行。 */
void zzPopulateExplorer(QStandardItemModel *model)
{
    for (int index = 0; index < zzExplorerNodeCount; ++index) {
        auto *folder = new QStandardItem(
            QStringLiteral("Workspace node %1").arg(index));
        folder->appendRow(new QStandardItem(
            QStringLiteral("detail-%1.txt").arg(index)));
        model->appendRow(folder);
    }
}

/** @brief 记录当前工作区对象、timer 与 animation 的固定预算。 */
struct ZzWorkspaceObjectBudget final
{
    qsizetype objects = 0;
    qsizetype timers = 0;
    qsizetype animations = 0;
    qsizetype resultViewWidgets = 0;
};

/** @brief 从真实工作区对象树采集结构预算。 */
ZzWorkspaceObjectBudget zzObjectBudget(
    const QMainWindow &host,
    const ZzFluentUI::ZzCommandPalette &palette)
{
    return {
        host.findChildren<QObject *>().size(),
        host.findChildren<QTimer *>().size(),
        host.findChildren<QAbstractAnimation *>().size(),
        palette.resultView()->findChildren<QWidget *>().size()};
}

/** @brief 断言重复操作没有改变固定对象树预算。 */
bool zzHasStableObjectBudget(
    const ZzWorkspaceObjectBudget &expected,
    const ZzWorkspaceObjectBudget &actual)
{
    return expected.objects == actual.objects
        && expected.timers == actual.timers
        && expected.animations == actual.animations
        && expected.resultViewWidgets == actual.resultViewWidgets;
}

/** @brief 断言虚拟化命令列表没有为每条结果创建 QWidget。 */
bool zzHasBoundedResultViewWidgets(const ZzWorkspaceObjectBudget &budget)
{
    return budget.resultViewWidgets <= zzMaximumResultViewWidgets;
}

/** @brief 返回渲染图像是否至少包含一个非透明像素。 */
bool zzContainsOpaquePixel(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) != 0) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 在 GUI 队列中完成一次同步状态消费。 */
void zzProcessGuiEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    QMainWindow host;
    host.setObjectName(QStringLiteral("ZzWorkspaceComponentsBenchmarkHost"));
    host.resize(1200, 800);
    ZzFluentUI::ZzFluentTitleBar titleBar(&host);
    titleBar.setGeometry(0, 0, host.width(), 40);
    auto *fileMenu = titleBar.menuBar()->addMenu(QStringLiteral("File"));
    fileMenu->addAction(QStringLiteral("Open workspace"));
    auto *workspaceMenu = titleBar.menuBar()->addMenu(
        QStringLiteral("Workspace"));
    workspaceMenu->addAction(QStringLiteral("Toggle panel"));
    auto shellResult = ZzPureTools::ZzWorkspaceShell::create(
        &host, &titleBar);
    if (!shellResult) {
        return zzFail(shellResult.error().technicalMessage());
    }
    std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell =
        std::move(shellResult).value();
    host.setCentralWidget(shell->workspaceWidget());
    host.show();
    titleBar.show();
    titleBar.raise();
    zzProcessGuiEvents();

    auto *explorer = new ZzFluentUI::ZzExplorerPane;
    QStandardItemModel explorerModel;
    zzPopulateExplorer(&explorerModel);
    explorer->setModel(&explorerModel);
    explorer->setSearchDelay(0);
    QVector<ZzPureTools::ZzWorkspacePanelId> sidePanelIds;
    sidePanelIds.reserve(zzSidePanelCount);
    for (int index = 0; index < zzSidePanelCount; ++index) {
        const ZzPureTools::ZzWorkspacePanelId id(
            QStringLiteral("benchmark-side-%1").arg(index));
        QWidget *content = index == 0
            ? static_cast<QWidget *>(explorer) : new QWidget;
        const auto registration = shell->registerSidePanel(
            id,
            QStringLiteral("Side panel %1").arg(index),
            {},
            (index % 2) == 0
                ? ZzFluentUI::ZzActivityArea::LeftPrimary
                : ZzFluentUI::ZzActivityArea::LeftSecondary,
            content);
        if (!registration) {
            delete content;
            return zzFail(registration.error().technicalMessage());
        }
        sidePanelIds.append(id);
    }
    for (int index = 0; index < zzDockPanelCount; ++index) {
        auto *content = new QWidget;
        const auto registration = shell->registerDockPanel(
            ZzPureTools::ZzWorkspacePanelId(
                QStringLiteral("benchmark-dock-%1").arg(index)),
            QStringLiteral("Dock panel %1").arg(index),
            {},
            Qt::BottomDockWidgetArea,
            content);
        if (!registration) {
            delete content;
            return zzFail(registration.error().technicalMessage());
        }
    }

    auto *tabs = shell->tabWidget();
    for (int index = 0; index < zzTabCount; ++index) {
        tabs->addTab(
            new QWidget,
            QStringLiteral("Workspace tab %1").arg(index));
    }

    auto *palette = shell->commandPalette();
    QStandardItemModel commandModel;
    zzPopulateCommands(&commandModel);
    palette->setModel(&commandModel);
    palette->open();
    zzProcessGuiEvents();
    if (palette->resultCount() != zzCommandCount) {
        return zzFail(QStringLiteral("command palette did not expose all commands"));
    }

    const auto initialLayout = shell->saveLayout();
    if (!initialLayout) {
        return zzFail(initialLayout.error().technicalMessage());
    }
    ZzWorkspaceObjectBudget stableBudget = zzObjectBudget(
        host, *palette);
    if (!zzHasBoundedResultViewWidgets(stableBudget)) {
        return zzFail(QStringLiteral(
            "command palette result view exceeded widget virtualization budget"));
    }
    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("workspace-components"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, host.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    for (int iteration = 0;
         iteration < zzWarmupIterations + zzMeasuredIterations;
         ++iteration) {
        const bool measured = iteration >= zzWarmupIterations;

        QElapsedTimer timer;
        timer.start();
        explorer->setSearchText(
            QStringLiteral("detail-%1.txt").arg(
                iteration % zzExplorerNodeCount));
        zzProcessGuiEvents();
        if (explorer->treeView()->model()->rowCount() != 1) {
            return zzFail(QStringLiteral("explorer recursive filter lost its source row"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("explorer-filter-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        timer.restart();
        palette->setQuery(QStringLiteral("command %1").arg(
            iteration % zzCommandCount));
        zzProcessGuiEvents();
        if (palette->resultCount() == 0) {
            return zzFail(QStringLiteral("command palette filter returned no result"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("command-filter-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        timer.restart();
        const int tabIndex = iteration % zzTabCount;
        tabs->setTabPinned(tabIndex, (iteration % 2) == 0);
        tabs->setTabModified(tabIndex, (iteration % 3) == 0);
        tabs->setTabAttention(tabIndex, (iteration % 5) == 0);
        tabs->setTabCloseEnabled(tabIndex, (iteration % 7) != 0);
        if (measured) {
            reporter.addSample({QStringLiteral("tab-state-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        timer.restart();
        constexpr std::array titleMenuModes{
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Expanded,
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact,
            ZzFluentUI::ZzTitleBarMenuDisplayMode::Adaptive};
        const ZzFluentUI::ZzTitleBarMenuDisplayMode titleMenuMode =
            titleMenuModes.at(static_cast<qsizetype>(
                iteration % static_cast<int>(titleMenuModes.size())));
        host.resize(titleMenuMode
                == ZzFluentUI::ZzTitleBarMenuDisplayMode::Adaptive
            ? 1200 : 720,
            800);
        titleBar.setGeometry(0, 0, host.width(), 40);
        titleBar.setMenuDisplayMode(titleMenuMode);
        zzProcessGuiEvents();
        const bool menuShouldBeVisible = titleMenuMode
            != ZzFluentUI::ZzTitleBarMenuDisplayMode::Compact;
        if (titleBar.menuDisplayMode() != titleMenuMode
            || titleBar.menuBar()->isVisible() != menuShouldBeVisible) {
            return zzFail(QStringLiteral("title menu mode did not change visible form"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("title-menu-switch-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        timer.restart();
        auto *activityBar = shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *activityModel = activityBar->model();
        const QModelIndex activityIndex = activityModel->index(
            iteration % zzSidePanelCount, 0);
        activityBar->activationRequested(activityIndex);
        if (activityBar->currentSourceIndex() != activityIndex) {
            return zzFail(QStringLiteral("activity activation selected the wrong panel"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("activity-activation-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        timer.restart();
        const auto savedLayout = shell->saveLayout();
        if (!savedLayout) {
            return zzFail(savedLayout.error().technicalMessage());
        }
        if (measured) {
            reporter.addSample({QStringLiteral("layout-save-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        timer.restart();
        const auto restoredLayout = shell->restoreLayout(savedLayout.value());
        if (!restoredLayout) {
            return zzFail(restoredLayout.error().technicalMessage());
        }
        if (measured) {
            reporter.addSample({QStringLiteral("layout-restore-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        timer.restart();
        host.render(&image);
        if (!zzContainsOpaquePixel(image)) {
            return zzFail(QStringLiteral("workspace render was fully transparent"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("workspace-render-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        const ZzWorkspaceObjectBudget currentBudget = zzObjectBudget(
            host, *palette);
        if (!zzHasBoundedResultViewWidgets(currentBudget)) {
            return zzFail(QStringLiteral(
                "command palette result view exceeded widget virtualization budget"));
        }
        if (!measured) {
            stableBudget = currentBudget;
        } else if (!zzHasStableObjectBudget(stableBudget, currentBudget)) {
            return zzFail(QStringLiteral("repeated workspace operations changed object budget"));
        }
    }

    const auto layoutBeforeFailedRestoreResult = shell->saveLayout();
    if (!layoutBeforeFailedRestoreResult) {
        return zzFail(layoutBeforeFailedRestoreResult.error().technicalMessage());
    }
    const QByteArray layoutBeforeFailedRestore =
        layoutBeforeFailedRestoreResult.value();
    if (shell->restoreLayout(QByteArrayLiteral("invalid workspace layout"))) {
        return zzFail(QStringLiteral("invalid workspace layout was accepted"));
    }
    const auto layoutAfterFailedRestore = shell->saveLayout();
    if (!layoutAfterFailedRestore
        || layoutAfterFailedRestore.value() != layoutBeforeFailedRestore) {
        return zzFail(QStringLiteral("failed layout restore did not roll back"));
    }

    const qsizetype stateTimerBudget = host.findChildren<QTimer *>().size();
    const qsizetype stateAnimationBudget =
        host.findChildren<QAbstractAnimation *>().size();
    for (int iteration = 0; iteration < zzStateToggleIterations; ++iteration) {
        shell->setTitleMode(static_cast<ZzPureTools::ZzWorkspaceTitleMode>(
            iteration % 4));
        shell->setApplicationTitle(QStringLiteral("Workspace %1").arg(iteration));
        const auto badge = shell->setPanelBadge(
            sidePanelIds.at(iteration % zzSidePanelCount), iteration % 100);
        if (!badge) {
            return zzFail(badge.error().technicalMessage());
        }
    }
    zzProcessGuiEvents();
    if (host.findChildren<QTimer *>().size() != stateTimerBudget
        || host.findChildren<QAbstractAnimation *>().size()
            != stateAnimationBudget
        || !zzHasStableObjectBudget(stableBudget, zzObjectBudget(host, *palette))) {
        return zzFail(QStringLiteral("1000 state changes changed timer, animation, or object budget"));
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
