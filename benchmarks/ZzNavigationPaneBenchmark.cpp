#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractListModel>
#include <QtCore/QAbstractProxyModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QList>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzNavigationDisplayMode.h>
#include <ZzFluentUI/ZzNavigationItemRole.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzLargeRowCount = 100000;
constexpr int zzSmallRowCount = 40;
constexpr int zzPaneCount = 40;
constexpr int zzPaneWidth = 240;
constexpr int zzPaneHeight = 320;
constexpr int zzFooterRows = 6;
constexpr int zzWarmupFrames = 10;
constexpr int zzMeasuredFrames = 120;
constexpr int zzMappingOperations = 1000;
constexpr int zzPaintSamples = 100;
constexpr int zzResetWarmups = 2;
constexpr int zzResetSamples = 20;
constexpr int zzStabilityOperations = 1000;
constexpr double zzMaximumPaintComplexityRatio = 1.5;

/** @brief 即时生成有界分区、徽标和固定页脚的平面导航数据。 */
class ZzNavigationBenchmarkModel final : public QAbstractListModel
{
public:
    /**
     * @brief 创建不为每行分配值对象的固定规模模型。
     * @param rows 至少包含六个页脚项的总行数。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzNavigationBenchmarkModel(
        int rows,
        QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , rows_(std::max(rows, zzFooterRows))
    {
    }

    /** @brief 返回当前固定规模的根列表行数。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : rows_;
    }

    /** @brief 按需生成导航展示角色，不持有逐行对象。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.parent().isValid()
            || index.column() != 0 || index.row() < 0
            || index.row() >= rows_) {
            return {};
        }
        const int row = index.row();
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            return QStringLiteral("Route %1").arg(row);
        }
        if (role == static_cast<int>(
                        ZzFluentUI::ZzNavigationItemRole::Section)
            && row < rows_ - zzFooterRows && (row % 5000) == 0) {
            return QStringLiteral("Section %1").arg(row / 5000);
        }
        if (role == static_cast<int>(
                ZzFluentUI::ZzNavigationItemRole::Placement)) {
            return QVariant::fromValue(
                row >= rows_ - zzFooterRows
                    ? ZzFluentUI::ZzNavigationPlacement::Footer
                    : ZzFluentUI::ZzNavigationPlacement::Primary);
        }
        if (role == static_cast<int>(
                ZzFluentUI::ZzNavigationItemRole::Badge)
            && row == badgeRow_) {
            return badgeText_;
        }
        return {};
    }

    /** @brief 每 997 行保留一个禁用目标以覆盖状态绘制。 */
    [[nodiscard]] Qt::ItemFlags flags(
        const QModelIndex &index) const override
    {
        const Qt::ItemFlags base = QAbstractListModel::flags(index);
        return index.isValid() && (index.row() % 997) == 0
            ? base & ~Qt::ItemIsEnabled : base;
    }

    /** @brief 局部更新一个短徽标并只通知展示相关角色。 */
    void setBadge(QString badgeText)
    {
        if (badgeText_ == badgeText) {
            return;
        }
        badgeText_ = std::move(badgeText);
        const QModelIndex changed = index(badgeRow_, 0);
        Q_EMIT dataChanged(
            changed,
            changed,
            {static_cast<int>(ZzFluentUI::ZzNavigationItemRole::Badge),
             Qt::ToolTipRole,
             Qt::AccessibleDescriptionRole});
    }

    /** @brief 发出结构 reset，供投影冷路径基准使用。 */
    void resetProjectionData()
    {
        beginResetModel();
        endResetModel();
    }

private:
    int rows_;
    int badgeRow_ = 1;
    QString badgeText_ = QStringLiteral("1");
};

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出导航面板基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 渲染一个固定尺寸 pane 并返回纳秒耗时。 */
qint64 zzRenderPane(
    ZzFluentUI::ZzNavigationPane *pane,
    QImage *image)
{
    image->fill(Qt::transparent);
    QElapsedTimer timer;
    timer.start();
    pane->render(image);
    return timer.nsecsElapsed();
}

/** @brief 返回有序纳秒样本的 nearest-rank 中位数。 */
qint64 zzMedian(QList<qint64> values)
{
    std::ranges::sort(values);
    return values.at((values.size() - 1) / 2);
}

/** @brief 验证 pane 固定拥有两个 view、两个 projection 且无动画或 timer。 */
bool zzHasFixedObjectShape(
    const ZzFluentUI::ZzNavigationPane *pane)
{
    return pane->findChildren<ZzFluentUI::ZzNavigationView *>().size() == 2
        && pane->findChildren<QAbstractProxyModel *>().size() == 2
        && pane->findChildren<QAbstractAnimation *>().isEmpty()
        && pane->findChildren<QTimer *>().isEmpty();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    ZzFluentUI::ZzThemeController controller;
    auto style = std::make_unique<ZzFluentUI::ZzFluentStyle>(&controller);
    QWidget host;
    host.setObjectName(QStringLiteral("ZzNavigationPaneBenchmarkWindow"));
    host.setStyle(style.get());
    host.setFixedSize(zzPaneWidth, zzPaneHeight);

    ZzNavigationBenchmarkModel largeModel(zzLargeRowCount);
    QList<ZzFluentUI::ZzNavigationPane *> panes;
    QList<ZzFluentUI::ZzNavigationView *> primaryViews;
    panes.reserve(zzPaneCount);
    primaryViews.reserve(zzPaneCount);
    int activationCount = 0;
    QModelIndex lastRequestedIndex;
    for (int index = 0; index < zzPaneCount; ++index) {
        auto *pane = new ZzFluentUI::ZzNavigationPane(&host);
        pane->setObjectName(
            QStringLiteral("ZzNavigationBenchmarkPane%1").arg(index));
        pane->setDisplayMode(
            ZzFluentUI::ZzNavigationDisplayMode::Regular);
        pane->setFixedHeight(zzPaneHeight);
        pane->setModel(&largeModel);
        pane->setCurrentSourceIndex(largeModel.index(index + 1, 0));
        QObject::connect(
            pane,
            &ZzFluentUI::ZzNavigationPane::navigationRequested,
            &host,
            [&activationCount, &lastRequestedIndex](
                const QModelIndex &sourceIndex) {
                ++activationCount;
                lastRequestedIndex = sourceIndex;
            });
        pane->show();
        panes.append(pane);
    }
    host.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    for (const auto *pane : panes) {
        if (!zzHasFixedObjectShape(pane)) {
            return zzFail(QStringLiteral(
                "navigation pane violated fixed object-shape budget"));
        }
        ZzFluentUI::ZzNavigationView *selectedView = nullptr;
        const auto views =
            pane->findChildren<ZzFluentUI::ZzNavigationView *>();
        for (auto *view : views) {
            if (view->currentIndex().isValid()) {
                if (selectedView != nullptr) {
                    return zzFail(QStringLiteral(
                        "navigation pane selected more than one view"));
                }
                selectedView = view;
            }
        }
        if (selectedView == nullptr) {
            return zzFail(QStringLiteral(
                "navigation pane did not select its primary view"));
        }
        primaryViews.append(selectedView);
    }

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("navigation-pane"));
    reporter.setWarmupIterations(zzWarmupFrames);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, host.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    constexpr double nanosecondsPerMicrosecond = 1'000.0;
    for (int operation = 0;
         operation < zzMappingOperations; ++operation) {
        const qsizetype paneIndex = operation % panes.size();
        auto *pane = panes.at(paneIndex);
        auto *primaryView = primaryViews.at(paneIndex);
        int sourceRow = 1 + ((operation * 97)
            % (zzLargeRowCount - zzFooterRows - 1));
        if ((sourceRow % 997) == 0) {
            --sourceRow;
        }
        const QModelIndex sourceIndex = largeModel.index(sourceRow, 0);
        QElapsedTimer timer;
        timer.start();
        pane->setCurrentSourceIndex(sourceIndex);
        const QModelIndex projectedIndex = primaryView->currentIndex();
        if (!projectedIndex.isValid()) {
            return zzFail(QStringLiteral(
                "source selection did not map to the primary view"));
        }
        Q_EMIT primaryView->activated(projectedIndex);
        const qint64 elapsed = timer.nsecsElapsed();
        if (pane->currentSourceIndex() != sourceIndex
            || lastRequestedIndex != sourceIndex
            || activationCount != operation + 1) {
            return zzFail(QStringLiteral(
                "constant-time selection or activation mapped the wrong row"));
        }
        reporter.addSample({
            QStringLiteral("mapping-time"),
            QStringLiteral("us"),
            static_cast<double>(elapsed) / nanosecondsPerMicrosecond});
    }

    QImage image(
        QSize(zzPaneWidth, zzPaneHeight),
        QImage::Format_ARGB32_Premultiplied);
    constexpr int totalFrames = zzWarmupFrames + zzMeasuredFrames;
    for (int frame = 0; frame < totalFrames; ++frame) {
        QElapsedTimer timer;
        timer.start();
        controller.setMode((frame % 2) == 0
                ? ZzFluentUI::ZzThemeMode::Dark
                : ZzFluentUI::ZzThemeMode::Light);
        largeModel.setBadge(QString::number(frame % 100));
        for (int paneIndex = 0; paneIndex < panes.size(); ++paneIndex) {
            const int row = 1 + ((frame * 31 + paneIndex * 17)
                % (zzLargeRowCount - zzFooterRows - 1));
            panes.at(paneIndex)->setCurrentSourceIndex(
                largeModel.index(row, 0));
        }
        const qint64 updateElapsed = timer.nsecsElapsed();
        for (auto *pane : panes) {
            static_cast<void>(zzRenderPane(pane, &image));
        }
        const qint64 elapsed = timer.nsecsElapsed();
        if (frame >= zzWarmupFrames) {
            reporter.addSample({
                QStringLiteral("state-update-time"),
                QStringLiteral("ms"),
                static_cast<double>(updateElapsed)
                    / nanosecondsPerMillisecond});
            reporter.addSample({
                QStringLiteral("render-time"),
                QStringLiteral("ms"),
                static_cast<double>(elapsed - updateElapsed)
                    / nanosecondsPerMillisecond});
            reporter.addSample({
                QStringLiteral("frame-time"),
                QStringLiteral("ms"),
                static_cast<double>(elapsed)
                    / nanosecondsPerMillisecond});
        }
    }

    ZzNavigationBenchmarkModel smallModel(zzSmallRowCount);
    ZzFluentUI::ZzNavigationPane smallPane;
    smallPane.setStyle(style.get());
    smallPane.setDisplayMode(ZzFluentUI::ZzNavigationDisplayMode::Regular);
    smallPane.setFixedHeight(zzPaneHeight);
    smallPane.setModel(&smallModel);
    smallPane.show();
    ZzFluentUI::ZzNavigationPane largePaintPane;
    largePaintPane.setStyle(style.get());
    largePaintPane.setDisplayMode(
        ZzFluentUI::ZzNavigationDisplayMode::Regular);
    largePaintPane.setFixedHeight(zzPaneHeight);
    largePaintPane.setModel(&largeModel);
    largePaintPane.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    for (int warmup = 0; warmup < zzWarmupFrames; ++warmup) {
        static_cast<void>(zzRenderPane(&smallPane, &image));
        static_cast<void>(zzRenderPane(&largePaintPane, &image));
    }
    QList<qint64> smallPaintSamples;
    QList<qint64> largePaintSamples;
    smallPaintSamples.reserve(zzPaintSamples);
    largePaintSamples.reserve(zzPaintSamples);
    for (int sample = 0; sample < zzPaintSamples; ++sample) {
        smallPaintSamples.append(zzRenderPane(&smallPane, &image));
        largePaintSamples.append(zzRenderPane(&largePaintPane, &image));
    }
    const qint64 smallMedian = zzMedian(std::move(smallPaintSamples));
    const qint64 largeMedian = zzMedian(std::move(largePaintSamples));
    if (smallMedian <= 0) {
        return zzFail(QStringLiteral("small-model paint median is invalid"));
    }
    const double paintComplexityRatio =
        static_cast<double>(largeMedian) / static_cast<double>(smallMedian);
    if (paintComplexityRatio > zzMaximumPaintComplexityRatio) {
        return zzFail(QStringLiteral(
            "navigation paint complexity ratio %1 exceeds %2")
                          .arg(paintComplexityRatio)
                          .arg(zzMaximumPaintComplexityRatio));
    }
    reporter.addSample({
        QStringLiteral("paint-complexity-ratio"),
        QStringLiteral("ratio"),
        paintComplexityRatio});

    ZzNavigationBenchmarkModel resetModel(zzLargeRowCount);
    ZzFluentUI::ZzNavigationPane resetPane;
    resetPane.setModel(&resetModel);
    for (int warmup = 0; warmup < zzResetWarmups; ++warmup) {
        resetModel.resetProjectionData();
    }
    for (int sample = 0; sample < zzResetSamples; ++sample) {
        QElapsedTimer timer;
        timer.start();
        resetModel.resetProjectionData();
        reporter.addSample({
            QStringLiteral("reset-time"),
            QStringLiteral("ms"),
            static_cast<double>(timer.nsecsElapsed())
                / nanosecondsPerMillisecond});
    }

    ZzNavigationBenchmarkModel stabilityModel(100);
    ZzFluentUI::ZzNavigationPane stabilityPane;
    stabilityPane.setModel(&stabilityModel);
    stabilityPane.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    const qsizetype initialDescendants =
        stabilityPane.findChildren<QObject *>().size();
    for (int operation = 0;
         operation < zzStabilityOperations; ++operation) {
        stabilityPane.setDisplayMode(
            (operation % 2) == 0
                ? ZzFluentUI::ZzNavigationDisplayMode::Regular
                : ZzFluentUI::ZzNavigationDisplayMode::Compact);
        stabilityPane.setLayoutDirection(
            (operation % 2) == 0
                ? Qt::LeftToRight : Qt::RightToLeft);
        stabilityModel.setBadge(QString::number(operation % 100));
        stabilityPane.setCurrentSourceIndex(
            stabilityModel.index(1 + (operation % 90), 0));
        if ((operation % 50) == 0) {
            stabilityModel.resetProjectionData();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    const qsizetype finalDescendants =
        stabilityPane.findChildren<QObject *>().size();
    if (finalDescendants != initialDescendants
        || !zzHasFixedObjectShape(&stabilityPane)) {
        return zzFail(QStringLiteral(
            "navigation pane object shape changed from %1 to %2")
                          .arg(initialDescendants)
                          .arg(finalDescendants));
    }
    reporter.addSample({
        QStringLiteral("descendants"),
        QStringLiteral("count"),
        static_cast<double>(finalDescendants)});
    reporter.addSample({
        QStringLiteral("views-per-pane"),
        QStringLiteral("count"),
        2.0});
    reporter.addSample({
        QStringLiteral("projections-per-pane"),
        QStringLiteral("count"),
        2.0});

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
