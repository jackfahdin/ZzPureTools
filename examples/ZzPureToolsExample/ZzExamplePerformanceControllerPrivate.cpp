#include "ZzExamplePerformanceControllerPrivate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QScrollBar>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzResult.h>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace ZzExample {

namespace {

constexpr int zzExposeTimeoutMilliseconds = 2000;
constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 100;
constexpr int zzModelRows = 100000;
constexpr int zzMaximumDataCalls = 160;
constexpr int zzIdleWarmupMilliseconds = 5000;
constexpr int zzIdleMeasurementMilliseconds = 30000;
constexpr double zzNanosecondsPerMillisecond = 1'000'000.0;

/** @brief 保存 Linux 进程 user 与 system CPU tick。 */
struct ZzExampleProcessCpuTimes final
{
    quint64 userTicks = 0;
    quint64 systemTicks = 0;
};

/** @brief 即时生成十万行数据并记录单帧批量访问范围。 */
class ZzExampleLargeListModel final : public QAbstractListModel
{
public:
    /** @brief 创建不为行分配容器的固定大模型。 */
    explicit ZzExampleLargeListModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回根索引的固定十万行。 */
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
            return QStringLiteral("Integrated row %1").arg(index.row());
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

/** @brief 统计真实列表 viewport 在单帧中的 Paint 事件。 */
class ZzExampleViewportPaintCounter final : public QObject
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

/** @brief 在真实综合窗口首次绘制完成后输出严格启动标记。 */
class ZzExampleFirstPaintProbe final : public QObject
{
public:
    /** @brief 保存应用、窗口和进程入口计时器的非拥有观察值。 */
    ZzExampleFirstPaintProbe(
        ZzPureTools::ZzPureApplication *application,
        ZzPureTools::ZzApplicationWindow *window,
        const QElapsedTimer *processTimer)
        : QObject(window)
        , application_(application)
        , window_(window)
        , processTimer_(processTimer)
    {
    }

protected:
    /** @brief 观察唯一一次顶层 Paint 并在下一事件轮次验证窗口可用。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (handled_ || watched != window_.data() || event == nullptr
            || event->type() != QEvent::Paint) {
            return QObject::eventFilter(watched, event);
        }
        handled_ = true;
        QTimer::singleShot(0, application_, [this] {
            const bool usable = application_ != nullptr
                && !window_.isNull()
                && window_->isVisible()
                && window_->isEnabled()
                && window_->centralWidget() != nullptr
                && window_->navigationController() != nullptr
                && window_->windowHandle() != nullptr
                && window_->windowHandle()->isExposed()
                && processTimer_ != nullptr
                && processTimer_->isValid();
            const qint64 elapsed = usable
                ? processTimer_->nsecsElapsed() : 0;
            if (!usable || elapsed <= 0) {
                QTextStream error(stderr, QIODevice::WriteOnly);
                error << "integrated startup window was not usable after first paint\n";
                error.flush();
                QCoreApplication::exit(EXIT_FAILURE);
                return;
            }

            QJsonObject markers;
            markers.insert(QStringLiteral("process-entry"), 0);
            markers.insert(QStringLiteral("first-paint"), elapsed);
            QTextStream output(stdout, QIODevice::WriteOnly);
            output << QJsonDocument(markers).toJson(QJsonDocument::Compact)
                   << '\n';
            output.flush();
            QCoreApplication::exit(EXIT_SUCCESS);
        });
        return QObject::eventFilter(watched, event);
    }

private:
    ZzPureTools::ZzPureApplication *application_ = nullptr;
    QPointer<ZzPureTools::ZzApplicationWindow> window_;
    const QElapsedTimer *processTimer_ = nullptr;
    bool handled_ = false;
};

/** @brief 在有界时间内等待综合窗口 native handle exposed。 */
[[nodiscard]] bool zzWaitForExposed(
    ZzPureTools::ZzApplicationWindow &window)
{
    QElapsedTimer timeout;
    timeout.start();
    while ((window.windowHandle() == nullptr
            || !window.windowHandle()->isExposed())
           && timeout.elapsed() < zzExposeTimeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return window.windowHandle() != nullptr
        && window.windowHandle()->isExposed();
}

/** @brief 在不轮询的 Qt 事件循环中等待指定毫秒。 */
void zzWaitWithEventLoop(int milliseconds)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timer.start(milliseconds);
    eventLoop.exec();
}

/** @brief 从 /proc/self/stat 严格读取进程 CPU tick。 */
[[nodiscard]] ZzCore::ZzResult<ZzExampleProcessCpuTimes> zzReadCpuTimes()
{
#if !defined(Q_OS_LINUX)
    return ZzCore::ZzResult<ZzExampleProcessCpuTimes>::failure(
        ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unsupported,
            QStringLiteral("integrated idle CPU is supported on Linux only")));
#else
    QFile stat(QStringLiteral("/proc/self/stat"));
    if (!stat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ZzCore::ZzResult<ZzExampleProcessCpuTimes>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Io,
                QStringLiteral("failed to open /proc/self/stat"),
                stat.errorString()));
    }
    const QByteArray content = stat.readAll().trimmed();
    const qsizetype closeParenthesis = content.lastIndexOf(')');
    if (closeParenthesis < 0 || closeParenthesis + 2 >= content.size()
        || content.at(closeParenthesis + 1) != ' ') {
        return ZzCore::ZzResult<ZzExampleProcessCpuTimes>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Io,
                QStringLiteral("invalid /proc/self/stat comm field")));
    }
    const QList<QByteArray> fields = content.sliced(closeParenthesis + 2)
                                         .simplified().split(' ');
    constexpr qsizetype userTimeIndex = 11;
    constexpr qsizetype systemTimeIndex = 12;
    bool userValid = false;
    bool systemValid = false;
    const quint64 userTicks = fields.size() > systemTimeIndex
        ? fields.at(userTimeIndex).toULongLong(&userValid) : 0;
    const quint64 systemTicks = fields.size() > systemTimeIndex
        ? fields.at(systemTimeIndex).toULongLong(&systemValid) : 0;
    if (!userValid || !systemValid) {
        return ZzCore::ZzResult<ZzExampleProcessCpuTimes>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Io,
                QStringLiteral("invalid CPU fields in /proc/self/stat")));
    }
    return ZzCore::ZzResult<ZzExampleProcessCpuTimes>::success(
        {userTicks, systemTicks});
#endif
}

/** @brief 从 /proc/self/status 读取严格正数的 VmRSS 字节数。 */
[[nodiscard]] ZzCore::ZzResult<qint64> zzReadResidentBytes()
{
#if !defined(Q_OS_LINUX)
    return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Unsupported,
        QStringLiteral("integrated idle RSS is supported on Linux only")));
#else
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Io,
            QStringLiteral("failed to open /proc/self/status"),
            status.errorString()));
    }
    while (true) {
        const QByteArray line = status.readLine();
        if (line.isEmpty()) {
            break;
        }
        if (!line.startsWith("VmRSS:")) {
            continue;
        }
        const QList<QByteArray> fields = line.simplified().split(' ');
        bool converted = false;
        const qint64 kibibytes = fields.size() == 3
            && fields.at(0) == QByteArrayLiteral("VmRSS:")
            && fields.at(2) == QByteArrayLiteral("kB")
            ? fields.at(1).toLongLong(&converted) : 0;
        constexpr qint64 bytesPerKibibyte = 1024;
        if (converted && kibibytes > 0
            && kibibytes <= std::numeric_limits<qint64>::max()
                / bytesPerKibibyte) {
            return ZzCore::ZzResult<qint64>::success(
                kibibytes * bytesPerKibibyte);
        }
        break;
    }
    return ZzCore::ZzResult<qint64>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::Io,
        QStringLiteral("missing or invalid VmRSS in /proc/self/status")));
#endif
}

} // namespace

ZzExamplePerformanceControllerPrivate::ZzExamplePerformanceControllerPrivate(
    ZzPureTools::ZzPureApplication *pureApplication,
    const QElapsedTimer *entryTimer)
    : application(pureApplication)
    , processTimer(entryTimer)
    , scenario(readScenario())
    , reportPath(qEnvironmentVariable(
          "ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_REPORT").trimmed())
{
    Q_ASSERT(application != nullptr);
    Q_ASSERT(processTimer != nullptr);
}

bool ZzExamplePerformanceControllerPrivate::isEnabled() const noexcept
{
    return scenario != ZzExamplePerformanceScenario::Disabled;
}

void ZzExamplePerformanceControllerPrivate::windowAttached(
    ZzPureTools::ZzApplicationWindow &window)
{
    if (scheduled || scenario == ZzExamplePerformanceScenario::Disabled) {
        return;
    }
    scheduled = true;
    if (scenario == ZzExamplePerformanceScenario::Invalid) {
        QTimer::singleShot(0, application, [this] {
            fail(QStringLiteral("unsupported integrated performance scenario"));
        });
        return;
    }
    if (scenario == ZzExamplePerformanceScenario::StartupProbe) {
        installStartupProbe(window);
        return;
    }
    if (reportPath.isEmpty()) {
        QTimer::singleShot(0, application, [this] {
            fail(QStringLiteral("missing integrated performance report path"));
        });
        return;
    }

    QTimer::singleShot(0, &window, [this, &window] {
        if (!zzWaitForExposed(window)) {
            fail(QStringLiteral("integrated performance window was not exposed"));
            return;
        }
        application->themeController()->setReducedMotion(true);
        switch (scenario) {
        case ZzExamplePerformanceScenario::Navigation:
            measureNavigation(window);
            break;
        case ZzExamplePerformanceScenario::ThemeSwitch:
            measureThemeSwitch(window);
            break;
        case ZzExamplePerformanceScenario::LargeModel:
            measureLargeModel(window);
            break;
        case ZzExamplePerformanceScenario::Idle:
            measureIdle(window);
            break;
        case ZzExamplePerformanceScenario::Disabled:
        case ZzExamplePerformanceScenario::StartupProbe:
        case ZzExamplePerformanceScenario::Invalid:
            fail(QStringLiteral("invalid integrated performance dispatch"));
            break;
        }
    });
}

ZzExamplePerformanceScenario
ZzExamplePerformanceControllerPrivate::readScenario()
{
    const QString value = qEnvironmentVariable(
        "ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_SCENARIO").trimmed();
    if (value.isEmpty()) {
        return ZzExamplePerformanceScenario::Disabled;
    }
    if (value == QStringLiteral("startup-probe")) {
        return ZzExamplePerformanceScenario::StartupProbe;
    }
    if (value == QStringLiteral("navigation")) {
        return ZzExamplePerformanceScenario::Navigation;
    }
    if (value == QStringLiteral("theme-switch")) {
        return ZzExamplePerformanceScenario::ThemeSwitch;
    }
    if (value == QStringLiteral("large-model")) {
        return ZzExamplePerformanceScenario::LargeModel;
    }
    if (value == QStringLiteral("idle")) {
        return ZzExamplePerformanceScenario::Idle;
    }
    return ZzExamplePerformanceScenario::Invalid;
}

void ZzExamplePerformanceControllerPrivate::installStartupProbe(
    ZzPureTools::ZzApplicationWindow &window)
{
    auto *probe = new ZzExampleFirstPaintProbe(
        application, &window, processTimer);
    window.installEventFilter(probe);
}

void ZzExamplePerformanceControllerPrivate::measureNavigation(
    ZzPureTools::ZzApplicationWindow &window)
{
    auto *navigation = window.navigationController();
    if (navigation == nullptr) {
        fail(QStringLiteral("integrated navigation controller is unavailable"));
        return;
    }
    static constexpr std::array routes = {
        "controls", "cards", "list-view", "table-view",
        "tree-view", "navigation", "feedback", "icons"};
    ZzBenchmarks::ZzPerformanceReporter reporter;
    if (!initializeReporter(
            &reporter,
            QStringLiteral("example-navigation"),
            zzWarmupIterations,
            window)) {
        return;
    }

    window.resize(1280, 800);
    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        const QString route = QString::fromLatin1(
            routes.at(static_cast<std::size_t>(iteration) % routes.size()));
        QElapsedTimer timer;
        timer.start();
        const auto result = navigation->navigate(
            ZzPureTools::ZzRouteId(route));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        image.fill(Qt::transparent);
        window.render(&image);
        const qint64 elapsed = timer.nsecsElapsed();
        if (!result || !navigation->currentRoute().isValid()
            || navigation->currentRoute().value() != route
            || elapsed <= 0) {
            fail(QStringLiteral("integrated navigation measurement failed"));
            return;
        }
        if (iteration >= zzWarmupIterations) {
            reporter.addSample({
                QStringLiteral("latency"),
                QStringLiteral("ms"),
                static_cast<double>(elapsed)
                    / zzNanosecondsPerMillisecond});
        }
    }
    writeReportAndExit(reporter);
}

void ZzExamplePerformanceControllerPrivate::measureThemeSwitch(
    ZzPureTools::ZzApplicationWindow &window)
{
    auto *theme = application->themeController();
    if (theme == nullptr) {
        fail(QStringLiteral("integrated theme controller is unavailable"));
        return;
    }
    ZzBenchmarks::ZzPerformanceReporter reporter;
    if (!initializeReporter(
            &reporter,
            QStringLiteral("example-theme-switch"),
            zzWarmupIterations,
            window)) {
        return;
    }

    window.resize(1280, 800);
    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        const auto mode = (iteration % 2) == 0
            ? ZzFluentUI::ZzThemeMode::Dark
            : ZzFluentUI::ZzThemeMode::Light;
        QElapsedTimer timer;
        timer.start();
        theme->setMode(mode);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        image.fill(Qt::transparent);
        window.render(&image);
        const qint64 elapsed = timer.nsecsElapsed();
        if (theme->mode() != mode || elapsed <= 0) {
            fail(QStringLiteral("integrated theme measurement failed"));
            return;
        }
        if (iteration >= zzWarmupIterations) {
            reporter.addSample({
                QStringLiteral("latency"),
                QStringLiteral("ms"),
                static_cast<double>(elapsed)
                    / zzNanosecondsPerMillisecond});
        }
    }
    writeReportAndExit(reporter);
}

void ZzExamplePerformanceControllerPrivate::measureLargeModel(
    ZzPureTools::ZzApplicationWindow &window)
{
    auto *navigation = window.navigationController();
    const auto navigationResult = navigation != nullptr
        ? navigation->navigate(
              ZzPureTools::ZzRouteId(QStringLiteral("list-view")))
        : ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
              ZzCore::ZzErrorCode::InvalidState,
              QStringLiteral("missing navigation controller")));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    auto *view = window.findChild<QListView *>(
        QStringLiteral("zzExampleListView"));
    if (!navigationResult || view == nullptr) {
        fail(QStringLiteral("integrated large-model page is unavailable"));
        return;
    }

    auto *model = new ZzExampleLargeListModel(view);
    view->setModel(model);
    view->setUniformItemSizes(true);
    view->setLayoutMode(QListView::Batched);
    view->setBatchSize(64);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    view->doItemsLayout();
    QElapsedTimer layoutTimeout;
    layoutTimeout.start();
    while ((view->verticalScrollBar()->pageStep() <= 0
            || !view->indexAt(QPoint(1, 1)).isValid())
           && layoutTimeout.elapsed() < zzExposeTimeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    if (view->verticalScrollBar()->pageStep() <= 0
        || !view->indexAt(QPoint(1, 1)).isValid()) {
        fail(QStringLiteral("integrated large-model viewport is not scrollable"));
        return;
    }

    ZzExampleViewportPaintCounter paintCounter;
    view->viewport()->installEventFilter(&paintCounter);
    QImage image(
        view->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    view->viewport()->render(&image);

    ZzBenchmarks::ZzPerformanceReporter reporter;
    if (!initializeReporter(
            &reporter,
            QStringLiteral("example-large-model"),
            zzWarmupIterations,
            window)) {
        return;
    }
    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        const QModelIndex firstBefore = view->indexAt(QPoint(1, 1));
        const int oldValue = view->verticalScrollBar()->value();
        const int nextValue = oldValue + view->verticalScrollBar()->pageStep();
        model->resetStatistics();
        paintCounter.reset();
        QElapsedTimer timer;
        timer.start();
        view->verticalScrollBar()->setValue(nextValue);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        image.fill(Qt::transparent);
        view->viewport()->render(&image);
        const qint64 elapsed = timer.nsecsElapsed();
        const QModelIndex firstAfter = view->indexAt(QPoint(1, 1));
        if (view->verticalScrollBar()->value() == oldValue
            || !firstBefore.isValid() || !firstAfter.isValid()
            || firstBefore.row() == firstAfter.row()
            || model->multiDataCalls() <= 0
            || model->multiDataCalls() > zzMaximumDataCalls
            || model->requestedRowCount() <= 0
            || model->requestedRowCount() > zzMaximumDataCalls
            || paintCounter.paintCount() <= 0 || elapsed <= 0) {
            fail(QStringLiteral(
                "integrated large-model frame exceeded locality budget"));
            return;
        }
        if (iteration >= zzWarmupIterations) {
            reporter.addSample({
                QStringLiteral("frame-time"),
                QStringLiteral("ms"),
                static_cast<double>(elapsed)
                    / zzNanosecondsPerMillisecond});
            reporter.addSample({
                QStringLiteral("multi-data-calls"),
                QStringLiteral("count"),
                static_cast<double>(model->multiDataCalls())});
            reporter.addSample({
                QStringLiteral("requested-rows"),
                QStringLiteral("count"),
                static_cast<double>(model->requestedRowCount())});
            reporter.addSample({
                QStringLiteral("viewport-paints"),
                QStringLiteral("count"),
                static_cast<double>(paintCounter.paintCount())});
        }
    }
    view->viewport()->removeEventFilter(&paintCounter);
    writeReportAndExit(reporter);
}

void ZzExamplePerformanceControllerPrivate::measureIdle(
    ZzPureTools::ZzApplicationWindow &window)
{
    ZzBenchmarks::ZzPerformanceReporter reporter;
    if (!initializeReporter(
            &reporter,
            QStringLiteral("example-idle"),
            5,
            window)) {
        return;
    }
    zzWaitWithEventLoop(zzIdleWarmupMilliseconds);
    const auto startCpu = zzReadCpuTimes();
    const auto startRss = zzReadResidentBytes();
    if (!startCpu || !startRss) {
        fail(!startCpu ? startCpu.error().technicalMessage()
                       : startRss.error().technicalMessage());
        return;
    }

    QElapsedTimer wallTimer;
    wallTimer.start();
    zzWaitWithEventLoop(zzIdleMeasurementMilliseconds);
    const qint64 wallNanoseconds = wallTimer.nsecsElapsed();
    const auto endCpu = zzReadCpuTimes();
    const auto endRss = zzReadResidentBytes();
    if (!endCpu || !endRss) {
        fail(!endCpu ? endCpu.error().technicalMessage()
                     : endRss.error().technicalMessage());
        return;
    }

#if !defined(Q_OS_LINUX)
    fail(QStringLiteral("integrated idle metrics require Linux"));
#else
    const long clockTicksPerSecond = sysconf(_SC_CLK_TCK);
    const quint64 startTicks = startCpu.value().userTicks
        + startCpu.value().systemTicks;
    const quint64 endTicks = endCpu.value().userTicks
        + endCpu.value().systemTicks;
    constexpr double nanosecondsPerSecond = 1'000'000'000.0;
    const double wallSeconds = static_cast<double>(wallNanoseconds)
        / nanosecondsPerSecond;
    if (clockTicksPerSecond <= 0 || endTicks < startTicks
        || wallSeconds <= 0.0) {
        fail(QStringLiteral("invalid integrated idle interval"));
        return;
    }
    const double cpuPercent = static_cast<double>(endTicks - startTicks)
        / static_cast<double>(clockTicksPerSecond)
        / wallSeconds * 100.0;
    const double rawGrowthPercent =
        static_cast<double>(endRss.value() - startRss.value())
        * 100.0 / static_cast<double>(startRss.value());
    reporter.addSample({
        QStringLiteral("average-cpu-percent"),
        QStringLiteral("percent"),
        cpuPercent});
    reporter.addSample({
        QStringLiteral("rss-start-bytes"),
        QStringLiteral("bytes"),
        static_cast<double>(startRss.value())});
    reporter.addSample({
        QStringLiteral("rss-end-bytes"),
        QStringLiteral("bytes"),
        static_cast<double>(endRss.value())});
    reporter.addSample({
        QStringLiteral("rss-growth-percent"),
        QStringLiteral("percent"),
        std::max(0.0, rawGrowthPercent)});
    writeReportAndExit(reporter);
#endif
}

bool ZzExamplePerformanceControllerPrivate::initializeReporter(
    ZzBenchmarks::ZzPerformanceReporter *reporter,
    const QString &reportScenario,
    qsizetype warmupIterations,
    ZzPureTools::ZzApplicationWindow &window) const
{
    Q_ASSERT(reporter != nullptr);
    reporter->setScenario(reportScenario);
    reporter->setWarmupIterations(warmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        *reporter, window.screen());
    if (!metadata) {
        fail(metadata.error().technicalMessage());
        return false;
    }
    return true;
}

void ZzExamplePerformanceControllerPrivate::writeReportAndExit(
    const ZzBenchmarks::ZzPerformanceReporter &reporter) const
{
    const auto result = reporter.write(reportPath);
    if (!result) {
        fail(result.error().technicalMessage());
        return;
    }
    application->beginShutdown();
    QCoreApplication::exit(EXIT_SUCCESS);
}

void ZzExamplePerformanceControllerPrivate::fail(
    const QString &reason) const
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << "ZzPureToolsExample performance failed: " << reason << '\n';
    stream.flush();
    QCoreApplication::exit(EXIT_FAILURE);
}

} // namespace ZzExample
