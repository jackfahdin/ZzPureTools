#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QAbstractTableModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QJsonObject>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtGui/QAction>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QRegion>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzAnnotatedScrollBar.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzCommandBar.h>
#include <ZzFluentUI/ZzCommandItemRole.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzExplorerPane.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneMode.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTitleBarMenuDisplayMode.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
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
constexpr int zzSidePanelCount = 32;
constexpr int zzBottomPanelCount = 3;
constexpr int zzCommandBarActionCount = 40;
constexpr int zzSmallGroupCount = 4;
constexpr int zzLargeGroupCount = 32;
constexpr int zzSmallMarkerCount = 20;
constexpr int zzLargeMarkerCount = 100000;
constexpr int zzPaintViewportWidth = 1200;
constexpr int zzPaintViewportHeight = 800;
constexpr double zzRenderP95BudgetMs = 12.0;
constexpr double zzStructureP95BudgetMs = 16.7;
constexpr double zzPaintRatioBudget = 2.0;
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

/** @brief 提供不分配每行 QObject 的固定标记模型。 */
class ZzBenchmarkMarkerModel final : public QAbstractTableModel
{
public:
    explicit ZzBenchmarkMarkerModel(
        int rows,
        int positionCount,
        QObject *parent = nullptr)
        : QAbstractTableModel(parent)
        , rows_(rows)
        , positionCount_(positionCount)
    {
    }

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : rows_;
    }

    [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : 1;
    }

    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.column() != 0) {
            return {};
        }
        if (role == static_cast<int>(
                        ZzFluentUI::ZzScrollMarkerRole::Position)) {
            return static_cast<qreal>(index.row() % positionCount_)
                / qMax(1, positionCount_ - 1);
        }
        if (role == static_cast<int>(
                        ZzFluentUI::ZzScrollMarkerRole::Kind)) {
            return static_cast<int>(ZzFluentUI::ZzScrollMarkerKind::Information);
        }
        if (role == static_cast<int>(
                        ZzFluentUI::ZzScrollMarkerRole::Priority)) {
            return index.row() % 5;
        }
        return {};
    }

private:
    int rows_ = 0;
    int positionCount_ = 1;
};

/** @brief 只使用公开 API 把分屏工作区扩展到固定组数。 */
bool zzCreateTabGroups(
    ZzFluentUI::ZzSplitWorkspace *workspace,
    int groupCount)
{
    if (workspace == nullptr || groupCount < 1) {
        return false;
    }
    const ZzFluentUI::ZzTabGroupId root =
        workspace->groupIds().constFirst();
    while (workspace->groupIds().size() < groupCount) {
        if (!workspace->splitGroup(
                root,
                Qt::Horizontal,
                ZzFluentUI::ZzSplitPlacement::After).has_value()) {
            return false;
        }
    }
    for (const ZzFluentUI::ZzTabGroupId &id : workspace->groupIds()) {
        workspace->tabWidget(id)->addTab(
            new QWidget,
            QStringLiteral("Group %1").arg(id.value()));
    }
    return workspace->groupIds().size() == groupCount;
}

/** @brief 返回隐藏页面是否仍持有运行中的 timer 或 animation。 */
bool zzHasBackgroundWakeup(const QWidget &page)
{
    for (const QTimer *timer : page.findChildren<QTimer *>()) {
        if (timer->isActive()) {
            return true;
        }
    }
    for (const QAbstractAnimation *animation
         : page.findChildren<QAbstractAnimation *>()) {
        if (animation->state() == QAbstractAnimation::Running) {
            return true;
        }
    }
    return false;
}

/** @brief 从 reporter schema 中读取指定 metric 的 P95。 */
std::optional<double> zzMetricP95(
    const QJsonObject &report,
    const QString &metric)
{
    const QJsonValue value = report.value(QStringLiteral("metrics"))
        .toObject().value(metric).toObject().value(QStringLiteral("p95"));
    return value.isDouble()
        ? std::optional<double>(value.toDouble()) : std::nullopt;
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

[[nodiscard]] std::optional<quint64> zzResidentBytes()
{
#if defined(Q_OS_LINUX)
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (!statm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    const QList<QByteArray> fields = statm.readAll().split(' ');
    if (fields.size() < 2) {
        return std::nullopt;
    }
    bool valid = false;
    const quint64 pages = fields.at(1).trimmed().toULongLong(&valid);
    if (!valid) {
        return std::nullopt;
    }
    const long nativePageBytes = ::sysconf(_SC_PAGESIZE);
    if (nativePageBytes <= 0) {
        return std::nullopt;
    }
    return pages * static_cast<quint64>(nativePageBytes);
#else
    return std::nullopt;
#endif
}

/** @brief 在 GUI 队列中完成一次同步状态消费。 */
void zzProcessGuiEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
    ZzFluentUI::ZzThemeController themeController;
    QStyle *const fusion = QStyleFactory::create(QStringLiteral("Fusion"));
    if (fusion == nullptr) {
        return zzFail(QStringLiteral("Fusion style is unavailable"));
    }
    auto *fluentStyle = new ZzFluentUI::ZzFluentStyle(&themeController, fusion);
    QApplication::setStyle(fluentStyle);
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
    QVector<QWidget *> sidePanelContents;
    sidePanelContents.reserve(zzSidePanelCount);
    const ZzFluentUI::ZzIconDescriptor fontActivityIcon =
        ZzFluentUI::ZzIconDescriptor::fromFontIcon(
            ZzFluentUI::ZzFontIcon::Server,
            false,
            ZzFluentUI::ZzIconColorMode::Custom,
            QColor(QStringLiteral("#f35325")));
    const ZzFluentUI::ZzIconDescriptor svgActivityIcon =
        ZzFluentUI::ZzIconDescriptor::fromBundledSvg(
            ZzFluentUI::ZzBundledSvgIcon::PinFill,
            false,
            ZzFluentUI::ZzIconColorMode::Custom,
            QColor(QStringLiteral("#3478f6")));
    const int cacheBytesBeforeActivityIcons = fluentStyle->iconCacheBytes();
    int cacheBytesAfterFontIcon = cacheBytesBeforeActivityIcons;
    for (int index = 0; index < zzSidePanelCount; ++index) {
        const ZzPureTools::ZzWorkspacePanelId id(
            QStringLiteral("benchmark-side-%1").arg(index));
        QWidget *content = index == 0
            ? static_cast<QWidget *>(explorer) : new QWidget;
        const auto registration = shell->registerSidePanel(
            id,
            QStringLiteral("Side panel %1").arg(index),
            (index % 2) == 0 ? fontActivityIcon : svgActivityIcon,
            (index % 2) == 0
                ? ZzFluentUI::ZzActivityArea::LeftPrimary
                : ZzFluentUI::ZzActivityArea::LeftSecondary,
            content);
        if (!registration) {
            delete content;
            return zzFail(registration.error().technicalMessage());
        }
        sidePanelIds.append(id);
        sidePanelContents.append(content);
        if (index <= 1) {
            zzProcessGuiEvents();
            auto *activityBar = shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left);
            QImage activityImage(
                activityBar->size(), QImage::Format_ARGB32_Premultiplied);
            activityImage.fill(Qt::transparent);
            activityBar->render(&activityImage);
            if (!zzContainsOpaquePixel(activityImage)) {
                return zzFail(QStringLiteral(
                    "activity bar descriptor render was fully transparent"));
            }
            const int currentCacheBytes = fluentStyle->iconCacheBytes();
            if (index == 0) {
                if (currentCacheBytes <= cacheBytesBeforeActivityIcons) {
                    return zzFail(QStringLiteral(
                        "font activity icon did not populate the style cache"));
                }
                cacheBytesAfterFontIcon = currentCacheBytes;
            } else if (currentCacheBytes <= cacheBytesAfterFontIcon) {
                return zzFail(QStringLiteral(
                    "SVG activity icon did not populate the style cache"));
            }
        }
    }

    ZzFluentUI::ZzSidePane *const leftSidePane = shell->sidePane(
        ZzFluentUI::ZzSidePaneEdge::Left);
    leftSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
    for (QWidget *content : sidePanelContents) {
        if (!leftSidePane->setWidgetVisible(content, true)) {
            return zzFail(QStringLiteral(
                "failed to expose all 32 side panels"));
        }
    }
    if (leftSidePane->visibleWidgets().size() != zzSidePanelCount) {
        return zzFail(QStringLiteral(
            "side pane did not retain 32 visible panels"));
    }

    QVector<ZzPureTools::ZzWorkspacePanelId> bottomPanelIds;
    bottomPanelIds.reserve(zzBottomPanelCount);
    for (int index = 0; index < zzBottomPanelCount; ++index) {
        auto *content = new QWidget;
        const ZzPureTools::ZzWorkspacePanelId id(
            QStringLiteral("benchmark-bottom-%1").arg(index));
        const auto registration = shell->registerBottomPanel(
            id,
            QStringLiteral("Bottom tool %1").arg(index),
            {},
            content);
        if (!registration) {
            delete content;
            return zzFail(registration.error().technicalMessage());
        }
        bottomPanelIds.append(id);
    }
    if (shell->bottomPane()->widgetCount() != zzBottomPanelCount) {
        return zzFail(QStringLiteral("bottom pane did not retain three tools"));
    }
    const auto showBottom = shell->showPanel(bottomPanelIds.constFirst());
    if (!showBottom) {
        return zzFail(showBottom.error().technicalMessage());
    }

    auto *commandBar = new ZzFluentUI::ZzCommandBar(&host);
    commandBar->resize(920, 40);
    for (int index = 0; index < zzCommandBarActionCount; ++index) {
        QAction *action = nullptr;
        if ((index % 4) == 3) {
            action = commandBar->addSecondaryAction(
                {}, QStringLiteral("Secondary %1").arg(index));
        } else {
            action = commandBar->addPrimaryAction(
                {}, QStringLiteral("Primary %1").arg(index));
        }
        if (action == nullptr) {
            return zzFail(QStringLiteral("failed to create command bar action"));
        }
    }
    if (commandBar->primaryActions().size()
            + commandBar->secondaryActions().size()
        != zzCommandBarActionCount) {
        return zzFail(QStringLiteral("command bar did not retain 40 actions"));
    }

    auto *fourGroupWorkspace = new ZzFluentUI::ZzSplitWorkspace(&host);
    auto *thirtyTwoGroupWorkspace = new ZzFluentUI::ZzSplitWorkspace(&host);
    auto *structureWorkspace = new ZzFluentUI::ZzSplitWorkspace(&host);
    fourGroupWorkspace->resize(zzPaintViewportWidth, zzPaintViewportHeight);
    thirtyTwoGroupWorkspace->resize(
        zzPaintViewportWidth * zzLargeGroupCount / zzSmallGroupCount,
        zzPaintViewportHeight);
    structureWorkspace->resize(zzPaintViewportWidth, zzPaintViewportHeight);
    for (ZzFluentUI::ZzSplitWorkspace *workspace
         : {fourGroupWorkspace, thirtyTwoGroupWorkspace, structureWorkspace}) {
        workspace->hide();
    }
    if (!zzCreateTabGroups(fourGroupWorkspace, zzSmallGroupCount)
        || !zzCreateTabGroups(thirtyTwoGroupWorkspace, zzLargeGroupCount)) {
        return zzFail(QStringLiteral("failed to create fixed tab group scales"));
    }

    ZzBenchmarkMarkerModel smallMarkerModel(
        zzSmallMarkerCount, zzSmallMarkerCount);
    ZzBenchmarkMarkerModel largeMarkerModel(
        zzLargeMarkerCount, zzLargeMarkerCount);
    auto *smallMarkerBar = new ZzFluentUI::ZzAnnotatedScrollBar(&host);
    auto *largeMarkerBar = new ZzFluentUI::ZzAnnotatedScrollBar(&host);
    for (ZzFluentUI::ZzAnnotatedScrollBar *bar
         : {smallMarkerBar, largeMarkerBar}) {
        bar->resize(16, 600);
        bar->setRange(0, 100000);
        bar->hide();
    }
    smallMarkerBar->setMarkerModel(&smallMarkerModel);
    largeMarkerBar->setMarkerModel(&largeMarkerModel);
    zzProcessGuiEvents();

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
    themeController.setMode(ZzFluentUI::ZzThemeMode::Dark);
    themeController.setMode(ZzFluentUI::ZzThemeMode::Light);
    zzProcessGuiEvents();

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
    const QSize paintViewportSize(
        zzPaintViewportWidth, zzPaintViewportHeight);
    const QRegion paintViewportRegion(QRect(QPoint{}, paintViewportSize));
    QImage fourGroupImage(
        paintViewportSize, QImage::Format_ARGB32_Premultiplied);
    QImage thirtyTwoGroupImage(
        paintViewportSize, QImage::Format_ARGB32_Premultiplied);
    QImage smallMarkerImage(
        smallMarkerBar->size(), QImage::Format_ARGB32_Premultiplied);
    QImage largeMarkerImage(
        largeMarkerBar->size(), QImage::Format_ARGB32_Premultiplied);

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
        QWidget *const toggledPanel = sidePanelContents.at(
            iteration % zzSidePanelCount);
        if (!leftSidePane->setWidgetVisible(toggledPanel, false)
            || !leftSidePane->setWidgetVisible(toggledPanel, true)) {
            return zzFail(QStringLiteral("side panel visibility toggle failed"));
        }
        zzProcessGuiEvents();
        if (leftSidePane->visibleWidgets().size() != zzSidePanelCount) {
            return zzFail(QStringLiteral(
                "side panel visibility did not return to 32"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("panel-toggle-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }
        timer.restart();
        const ZzFluentUI::ZzTabGroupId structureRoot =
            structureWorkspace->groupIds().constFirst();
        const auto temporaryGroup = structureWorkspace->splitGroup(
            structureRoot,
            Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        if (!temporaryGroup.has_value()
            || !structureWorkspace->removeEmptyGroup(*temporaryGroup)) {
            return zzFail(QStringLiteral(
                "split/merge structure operation failed"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("group-structure-time"),
                                QStringLiteral("ms"),
                                zzMilliseconds(timer.nsecsElapsed())});
        }

        fourGroupImage.fill(Qt::transparent);
        timer.restart();
        fourGroupWorkspace->render(
            &fourGroupImage,
            QPoint{},
            paintViewportRegion,
            QWidget::DrawWindowBackground | QWidget::DrawChildren);
        const double fourGroupElapsed = zzMilliseconds(timer.nsecsElapsed());
        if (!zzContainsOpaquePixel(fourGroupImage)) {
            return zzFail(QStringLiteral(
                "four group workspace paint was fully transparent"));
        }
        thirtyTwoGroupImage.fill(Qt::transparent);
        timer.restart();
        thirtyTwoGroupWorkspace->render(
            &thirtyTwoGroupImage,
            QPoint{},
            paintViewportRegion,
            QWidget::DrawWindowBackground | QWidget::DrawChildren);
        const double thirtyTwoGroupElapsed =
            zzMilliseconds(timer.nsecsElapsed());
        if (!zzContainsOpaquePixel(thirtyTwoGroupImage)) {
            return zzFail(QStringLiteral(
                "32 group workspace paint was fully transparent"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral(
                                    "workspace-paint-4-groups-time"),
                                QStringLiteral("ms"), fourGroupElapsed});
            reporter.addSample({QStringLiteral(
                                    "workspace-paint-32-groups-time"),
                                QStringLiteral("ms"), thirtyTwoGroupElapsed});
        }

        smallMarkerImage.fill(Qt::transparent);
        timer.restart();
        smallMarkerBar->render(&smallMarkerImage);
        const double smallMarkerElapsed = zzMilliseconds(timer.nsecsElapsed());
        largeMarkerImage.fill(Qt::transparent);
        timer.restart();
        largeMarkerBar->render(&largeMarkerImage);
        const double largeMarkerElapsed = zzMilliseconds(timer.nsecsElapsed());
        if (!zzContainsOpaquePixel(smallMarkerImage)
            || !zzContainsOpaquePixel(largeMarkerImage)) {
            return zzFail(QStringLiteral("marker paint was fully transparent"));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("marker-paint-20-time"),
                                QStringLiteral("ms"), smallMarkerElapsed});
            reporter.addSample({QStringLiteral("marker-paint-100000-time"),
                                QStringLiteral("ms"), largeMarkerElapsed});
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
        zzProcessGuiEvents();

        const ZzWorkspaceObjectBudget currentBudget = zzObjectBudget(
            host, *palette);
        if (!zzHasBoundedResultViewWidgets(currentBudget)) {
            return zzFail(QStringLiteral(
                "command palette result view exceeded widget virtualization budget"));
        }
        if (!measured) {
            stableBudget = currentBudget;
        } else if (!zzHasStableObjectBudget(stableBudget, currentBudget)) {
            return zzFail(QStringLiteral(
                "repeated workspace operations changed object budget: "
                "expected %1/%2/%3/%4, actual %5/%6/%7/%8")
                .arg(stableBudget.objects)
                .arg(stableBudget.timers)
                .arg(stableBudget.animations)
                .arg(stableBudget.resultViewWidgets)
                .arg(currentBudget.objects)
                .arg(currentBudget.timers)
                .arg(currentBudget.animations)
                .arg(currentBudget.resultViewWidgets));
        }
        if (measured) {
            reporter.addSample({QStringLiteral("object-count"),
                                QStringLiteral("count"),
                                static_cast<double>(currentBudget.objects)});
            reporter.addSample({QStringLiteral("timer-count"),
                                QStringLiteral("count"),
                                static_cast<double>(currentBudget.timers)});
            reporter.addSample({QStringLiteral("animation-count"),
                                QStringLiteral("count"),
                                static_cast<double>(currentBudget.animations)});
            reporter.addSample({QStringLiteral("result-view-widget-count"),
                                QStringLiteral("count"),
                                static_cast<double>(currentBudget.resultViewWidgets)});
            reporter.addSample({QStringLiteral("style-cache-bytes"),
                                QStringLiteral("bytes"),
                                static_cast<double>(fluentStyle->iconCacheBytes())});
            if (const auto resident = zzResidentBytes()) {
                reporter.addSample({QStringLiteral("rss-bytes"),
                                    QStringLiteral("bytes"),
                                    static_cast<double>(*resident)});
            }
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
    const ZzWorkspaceObjectBudget stateObjectBudget = zzObjectBudget(
        host, *palette);
    for (int iteration = 0; iteration < zzStateToggleIterations; ++iteration) {
        QWidget *const statePanel = sidePanelContents.at(
            iteration % zzSidePanelCount);
        if (!leftSidePane->setWidgetVisible(statePanel, false)
            || !leftSidePane->setWidgetVisible(statePanel, true)) {
            return zzFail(QStringLiteral(
                "1000 state changes failed to restore side visibility"));
        }
        const ZzFluentUI::ZzTabGroupId structureRoot =
            structureWorkspace->groupIds().constFirst();
        const auto temporaryGroup = structureWorkspace->splitGroup(
            structureRoot,
            (iteration % 2) == 0 ? Qt::Horizontal : Qt::Vertical,
            ZzFluentUI::ZzSplitPlacement::After);
        if (!temporaryGroup.has_value()
            || !structureWorkspace->removeEmptyGroup(*temporaryGroup)) {
            return zzFail(QStringLiteral(
                "1000 state changes failed to restore group structure"));
        }
        shell->bottomPane()->setCollapsed((iteration % 2) == 0);
        shell->bottomPane()->setCollapsed(false);
        themeController.setMode((iteration % 2) == 0
            ? ZzFluentUI::ZzThemeMode::Dark
            : ZzFluentUI::ZzThemeMode::Light);
        shell->setTitleMode(static_cast<ZzPureTools::ZzWorkspaceTitleMode>(
            iteration % 4));
        shell->setApplicationTitle(QStringLiteral("Workspace %1").arg(iteration));
        const auto badge = shell->setPanelBadge(
            sidePanelIds.at(iteration % zzSidePanelCount), iteration % 100);
        if (!badge) {
            return zzFail(badge.error().technicalMessage());
        }
        if ((iteration % 50) == 49) {
            zzProcessGuiEvents();
        }
    }
    themeController.setMode(ZzFluentUI::ZzThemeMode::Light);
    zzProcessGuiEvents();
    if (host.findChildren<QTimer *>().size() != stateTimerBudget
        || host.findChildren<QAbstractAnimation *>().size()
            != stateAnimationBudget
        || !zzHasStableObjectBudget(
            stateObjectBudget, zzObjectBudget(host, *palette))) {
        return zzFail(QStringLiteral("1000 state changes changed timer, animation, or object budget"));
    }

    QWidget *const hiddenPanel = sidePanelContents.constFirst();
    if (!leftSidePane->setWidgetVisible(hiddenPanel, false)) {
        return zzFail(QStringLiteral("failed to hide background wakeup probe"));
    }
    zzProcessGuiEvents();
    if (zzHasBackgroundWakeup(*hiddenPanel)) {
        return zzFail(QStringLiteral(
            "hidden workspace page retained an active timer or animation"));
    }
    if (!leftSidePane->setWidgetVisible(hiddenPanel, true)) {
        return zzFail(QStringLiteral("failed to restore background wakeup probe"));
    }
    zzProcessGuiEvents();
    if (!zzHasStableObjectBudget(
            stateObjectBudget, zzObjectBudget(host, *palette))) {
        return zzFail(QStringLiteral(
            "hidden page wakeup probe changed the object budget"));
    }

    const auto reportResult = reporter.report();
    if (!reportResult) {
        return zzFail(reportResult.error().technicalMessage());
    }
    const QJsonObject report = reportResult.value();
    const auto renderP95 = zzMetricP95(
        report, QStringLiteral("workspace-render-time"));
    const auto panelP95 = zzMetricP95(
        report, QStringLiteral("panel-toggle-time"));
    const auto structureP95 = zzMetricP95(
        report, QStringLiteral("group-structure-time"));
    const auto fourGroupP95 = zzMetricP95(
        report, QStringLiteral("workspace-paint-4-groups-time"));
    const auto thirtyTwoGroupP95 = zzMetricP95(
        report, QStringLiteral("workspace-paint-32-groups-time"));
    const auto smallMarkerP95 = zzMetricP95(
        report, QStringLiteral("marker-paint-20-time"));
    const auto largeMarkerP95 = zzMetricP95(
        report, QStringLiteral("marker-paint-100000-time"));
    if (!renderP95 || !panelP95 || !structureP95
        || !fourGroupP95 || !thirtyTwoGroupP95
        || !smallMarkerP95 || !largeMarkerP95) {
        return zzFail(QStringLiteral("workspace performance report is incomplete"));
    }
    if (*renderP95 > zzRenderP95BudgetMs) {
        return zzFail(QStringLiteral(
            "workspace render P95 %1 ms exceeds 12 ms")
                          .arg(*renderP95));
    }
    if (*panelP95 > zzStructureP95BudgetMs
        || *structureP95 > zzStructureP95BudgetMs) {
        return zzFail(QStringLiteral(
            "workspace structure P95 exceeds 16.7 ms"));
    }
    if (*fourGroupP95 <= 0.0
        || *thirtyTwoGroupP95 / *fourGroupP95 > zzPaintRatioBudget) {
        return zzFail(QStringLiteral(
            "32/4 group paint P95 ratio %1/%2 exceeds 2.0")
            .arg(*thirtyTwoGroupP95)
            .arg(*fourGroupP95));
    }
    if (*smallMarkerP95 <= 0.0
        || *largeMarkerP95 / *smallMarkerP95 > zzPaintRatioBudget) {
        return zzFail(QStringLiteral(
            "100000/20 marker paint P95 ratio %1/%2 exceeds 2.0")
            .arg(*largeMarkerP95)
            .arg(*smallMarkerP95));
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
