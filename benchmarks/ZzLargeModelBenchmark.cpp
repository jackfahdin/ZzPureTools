#include <cstdio>
#include <cstdlib>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QtGui/QImage>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QScrollBar>

#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzItemDensity.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzModelRows = 100000;
constexpr int zzVisibleRows = 40;
constexpr int zzItemHeight = 32;
constexpr int zzMaximumDataCalls = 120;
constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 100;

/** @brief 即时生成 10 万行数据并记录单帧批量访问范围。 */
class ZzBenchmarkListModel final : public QAbstractListModel
{
public:
    /** @brief 创建只保存固定行数和统计计数的模型。 */
    explicit ZzBenchmarkListModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回根索引的固定十万行，不为行分配容器。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : zzModelRows;
    }

    /** @brief 为单角色回退路径即时生成当前行数据。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= zzModelRows) {
            return {};
        }
        requestedRows_.insert(index.row());
        if (role == Qt::DisplayRole) {
            return QStringLiteral("Row %1").arg(index.row());
        }
        if (role == Qt::TextAlignmentRole) {
            return QVariant::fromValue(
                Qt::Alignment(Qt::AlignLeading | Qt::AlignVCenter));
        }
        return {};
    }

    /** @brief 一次填充当前索引请求的全部 role 并统计调用。 */
    void multiData(
        const QModelIndex &index,
        QModelRoleDataSpan roleDataSpan) const override
    {
        ++multiDataCalls_;
        requestedRows_.insert(index.row());
        for (QModelRoleData &roleData : roleDataSpan) {
            roleData.setData(data(index, roleData.role()));
        }
    }

    /** @brief 清空上一帧统计而不改变模型内容。 */
    void resetStatistics() const
    {
        requestedRows_.clear();
        multiDataCalls_ = 0;
    }

    /** @brief 返回当前帧 multiData 调用数。 */
    [[nodiscard]] int multiDataCalls() const noexcept
    {
        return multiDataCalls_;
    }

    /** @brief 返回当前帧请求过的去重行数。 */
    [[nodiscard]] qsizetype requestedRowCount() const noexcept
    {
        return requestedRows_.size();
    }

private:
    mutable QSet<int> requestedRows_;
    mutable int multiDataCalls_ = 0;
};

/** @brief 统计 QListView viewport 在单帧中的 Paint 事件。 */
class ZzViewportPaintCounter final : public QObject
{
public:
    /** @brief 清空上一帧 Paint 次数。 */
    void reset() noexcept
    {
        paintCount_ = 0;
    }

    /** @brief 返回当前帧 Paint 次数。 */
    [[nodiscard]] int paintCount() const noexcept
    {
        return paintCount_;
    }

protected:
    /** @brief 只观察 Paint，不改变 viewport 的事件处理。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (event != nullptr && event->type() == QEvent::Paint) {
            ++paintCount_;
        }
        return false;
    }

private:
    int paintCount_ = 0;
};

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出大模型基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    ZzBenchmarkListModel model;
    QListView view;
    view.setObjectName(QStringLiteral("ZzLargeModelBenchmarkView"));
    view.setModel(&model);
    auto *delegate = new ZzFluentUI::ZzFluentItemDelegate(&view);
    delegate->setDensity(ZzFluentUI::ZzItemDensity::Compact);
    view.setItemDelegate(delegate);
    view.setUniformItemSizes(true);
    view.setLayoutMode(QListView::Batched);
    view.setBatchSize(64);
    view.setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    view.setFixedSize(
        480,
        (zzVisibleRows * zzItemHeight) + (2 * view.frameWidth()));

    ZzViewportPaintCounter paintCounter;
    view.viewport()->installEventFilter(&paintCounter);
    view.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    view.doItemsLayout();
    QElapsedTimer layoutTimeout;
    layoutTimeout.start();
    while ((view.verticalScrollBar()->pageStep() <= 0
            || !view.indexAt(QPoint(1, 1)).isValid())
           && layoutTimeout.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(1);
    }
    if (view.verticalScrollBar()->pageStep() <= 0
        || !view.indexAt(QPoint(1, 1)).isValid()) {
        return zzFail(QStringLiteral(
            "large-model view did not establish a scrollable viewport"));
    }

    QImage image(
        view.viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    view.viewport()->render(&image);

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("large-model"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, view.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        const QModelIndex firstBefore = view.indexAt(QPoint(1, 1));
        const QModelIndex lastBefore = view.indexAt(
            QPoint(1, view.viewport()->height() - 2));
        const int oldValue = view.verticalScrollBar()->value();
        const int nextValue = oldValue
            + view.verticalScrollBar()->pageStep();

        model.resetStatistics();
        paintCounter.reset();
        QElapsedTimer timer;
        timer.start();
        view.verticalScrollBar()->setValue(nextValue);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        view.viewport()->render(&image);
        const qint64 elapsedNanoseconds = timer.nsecsElapsed();

        const QModelIndex firstAfter = view.indexAt(QPoint(1, 1));
        const QModelIndex lastAfter = view.indexAt(
            QPoint(1, view.viewport()->height() - 2));
        if (view.verticalScrollBar()->value() == oldValue
            || !firstBefore.isValid() || !lastBefore.isValid()
            || !firstAfter.isValid() || !lastAfter.isValid()
            || firstBefore.row() == firstAfter.row()
            || lastBefore.row() == lastAfter.row()) {
            return zzFail(QStringLiteral(
                "large-model viewport did not advance by a page"));
        }
        if (model.multiDataCalls() <= 0
            || model.multiDataCalls() > zzMaximumDataCalls
            || model.requestedRowCount() <= 0
            || model.requestedRowCount() > zzMaximumDataCalls
            || paintCounter.paintCount() <= 0) {
            return zzFail(QStringLiteral(
                "large-model frame exceeded locality budget: calls=%1 rows=%2 paints=%3")
                              .arg(model.multiDataCalls())
                              .arg(model.requestedRowCount())
                              .arg(paintCounter.paintCount()));
        }

        if (iteration >= zzWarmupIterations) {
            reporter.addSample({
                QStringLiteral("frame-time"),
                QStringLiteral("ms"),
                static_cast<double>(elapsedNanoseconds)
                    / nanosecondsPerMillisecond});
            reporter.addSample({
                QStringLiteral("multi-data-calls"),
                QStringLiteral("count"),
                static_cast<double>(model.multiDataCalls())});
            reporter.addSample({
                QStringLiteral("requested-rows"),
                QStringLiteral("count"),
                static_cast<double>(model.requestedRowCount())});
            reporter.addSample({
                QStringLiteral("viewport-paints"),
                QStringLiteral("count"),
                static_cast<double>(paintCounter.paintCount())});
        }
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
