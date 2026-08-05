#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QEnterEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzActionCard.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzImageCard.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {

constexpr int zzModelRows = 100000;
constexpr int zzVisibleRows = 40;
constexpr int zzWarmupFrames = 10;
constexpr int zzMeasuredFrames = 100;
constexpr int zzProgressMeasuredFrames = 120;
constexpr int zzMaximumMultiDataCalls = 120;
constexpr int zzMaximumIconCacheBytes = 4 * 1024 * 1024;
constexpr qreal zzReferenceP95Milliseconds = 16.7;

/** @brief 记录 10 万行即时模型的批量数据访问范围与调用数量。 */
class ZzBenchmarkRowsModel final : public QAbstractListModel
{
public:
    /** @brief 创建不为各行分配对象的固定规模模型。 */
    explicit ZzBenchmarkRowsModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回根索引下的十万逻辑行。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : zzModelRows;
    }

    /** @brief 即时生成当前展示角色并记录被访问行。 */
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

    /** @brief 批量填充 role span，并只为当前索引增加一次调用统计。 */
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

    /** @brief 清空单帧统计，不改变模型规模。 */
    void resetStatistics() const
    {
        requestedRows_.clear();
        multiDataCalls_ = 0;
    }

    /** @brief 返回单帧被访问的去重行集合。 */
    [[nodiscard]] const QSet<int> &requestedRows() const noexcept
    {
        return requestedRows_;
    }

    /** @brief 返回单帧 multiData 调用数量。 */
    [[nodiscard]] int multiDataCalls() const noexcept
    {
        return multiDataCalls_;
    }

private:
    mutable QSet<int> requestedRows_;
    mutable int multiDataCalls_ = 0;
};

/** @brief 绘制指定连续四十行并返回纳秒耗时。 */
qint64 zzPaintVisibleFrame(
    ZzFluentUI::ZzFluentItemDelegate *delegate,
    ZzBenchmarkRowsModel *model,
    QWidget *styleHost,
    QImage *target,
    int firstRow)
{
    QElapsedTimer timer;
    timer.start();
    QPainter painter(target);
    QStyleOptionViewItem option;
    option.state = QStyle::State_Enabled;
    option.palette = styleHost->palette();
    option.widget = styleHost;
    for (int visible = 0; visible < zzVisibleRows; ++visible) {
        option.rect = QRect(0, visible * 40, target->width(), 40);
        delegate->paint(
            &painter,
            option,
            model->index(firstRow + visible, 0));
    }
    painter.end();
    return timer.nsecsElapsed();
}

/** @brief 按最近秩计算已排序样本的百分位毫秒值。 */
qreal zzPercentileMilliseconds(
    const std::vector<qint64> &sortedNanoseconds,
    qreal percentile)
{
    const qreal rank = std::ceil(
        percentile * static_cast<qreal>(sortedNanoseconds.size()));
    const std::size_t index = static_cast<std::size_t>(
        std::max(qreal(1.0), rank) - 1.0);
    return static_cast<qreal>(sortedNanoseconds.at(index)) / 1000000.0;
}

/** @brief 统计一组 QObject 根节点的全部后代数量。 */
template<std::size_t N>
qsizetype zzDescendantCount(const std::array<QObject *, N> &roots)
{
    qsizetype total = 0;
    for (QObject *root : roots) {
        total += root->findChildren<QObject *>().size();
    }
    return total;
}

/** @brief 切换到指定月份并将完整日历渲染到复用图像。 */
qint64 zzRenderCalendarFrame(
    ZzFluentUI::ZzCalendar *calendar,
    QImage *target,
    int sequence)
{
    const int month = (sequence % 12) + 1;
    QElapsedTimer timer;
    timer.start();
    calendar->setCurrentPage(2026, month);
    calendar->setSelectedDate(QDate(2026, month, 15));
    target->fill(Qt::transparent);
    QPainter painter(target);
    calendar->render(&painter);
    painter.end();
    return timer.nsecsElapsed();
}

/** @brief 更新全部卡片状态并将预构造卡片面渲染到复用图像。 */
qint64 zzRenderCardFrame(
    QWidget *host,
    const std::vector<ZzFluentUI::ZzActionCard *> &actionCards,
    const std::vector<ZzFluentUI::ZzImageCard *> &imageCards,
    QImage *target,
    int sequence)
{
    QElapsedTimer timer;
    timer.start();
    for (std::size_t index = 0; index < actionCards.size(); ++index) {
        auto *card = actionCards[index];
        const int offset = sequence + static_cast<int>(index);
        card->setChecked((offset % 7) == 0);
        card->setDown((offset % 11) == 0);
        card->setEnabled((offset % 13) != 0);
    }
    for (std::size_t index = 0; index < imageCards.size(); ++index) {
        auto *card = imageCards[index];
        const int offset = sequence + static_cast<int>(index);
        card->setChecked((offset % 5) == 0);
        card->setDown((offset % 9) == 0);
        card->setEnabled((offset % 11) != 0);
        card->setAspectRatioMode(
            static_cast<Qt::AspectRatioMode>(offset % 3));
    }
    target->fill(Qt::transparent);
    QPainter painter(target);
    host->render(&painter);
    painter.end();
    return timer.nsecsElapsed();
}

/** @brief 向 QWidget 同步发送一次进入和离开序列。 */
void zzSendHoverCycle(QWidget *widget)
{
    QEnterEvent enter(
        QPointF(1.0, 1.0),
        QPointF(1.0, 1.0),
        QPointF(1.0, 1.0));
    QCoreApplication::sendEvent(widget, &enter);
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(widget, &leave);
}

} // namespace

/** @brief 记录基础控件局部绘制耗时并锁定对象与缓存预算。 */
class ZzBasicControlsBenchmark final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void measuresVisibleRows()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget styleHost;
        styleHost.setStyle(&style);
        styleHost.setPalette(style.standardPalette());
        ZzBenchmarkRowsModel model;
        ZzFluentUI::ZzFluentItemDelegate delegate;
        QImage target(
            QSize(320, zzVisibleRows * 40),
            QImage::Format_ARGB32_Premultiplied);
        target.fill(Qt::transparent);
        std::vector<qint64> samples;
        samples.reserve(zzMeasuredFrames);
        int maximumMultiDataCalls = 0;

        for (int frame = -zzWarmupFrames;
             frame < zzMeasuredFrames;
             ++frame) {
            const int sequence = frame + zzWarmupFrames;
            const int firstRow = (sequence * 997)
                % (zzModelRows - zzVisibleRows);
            QSet<int> expectedRows;
            for (int visible = 0; visible < zzVisibleRows; ++visible) {
                expectedRows.insert(firstRow + visible);
            }
            model.resetStatistics();
            const qint64 elapsed = zzPaintVisibleFrame(
                &delegate,
                &model,
                &styleHost,
                &target,
                firstRow);
            QCOMPARE(model.requestedRows(), expectedRows);
            QVERIFY(model.multiDataCalls() >= zzVisibleRows);
            QVERIFY(model.multiDataCalls() <= zzMaximumMultiDataCalls);
            maximumMultiDataCalls = std::max(
                maximumMultiDataCalls,
                model.multiDataCalls());
            if (frame >= 0) {
                samples.push_back(elapsed);
            }
        }

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-basic-controls visibleRows=40 P50=%1 ms "
                   "P95=%2 ms max=%3 ms maxMultiData=%4")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3)
                   .arg(maximumMultiDataCalls);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }

    void keepsObjectAndCacheBudgetsStable()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        auto *layout = new QVBoxLayout(&host);
        auto *toggle = new ZzFluentUI::ZzToggleSwitch(
            QStringLiteral("Sync"), &host);
        auto *button = new ZzFluentUI::ZzPushButton(
            QStringLiteral("Apply"), &host);
        auto *icon = new ZzFluentUI::ZzIconButton(&host);
        icon->setAccessibleName(QStringLiteral("Refresh"));
        icon->setIconDescriptor({
            QStringLiteral(
                ":/zzfluent/benchmarks/ZzFluentTestSquare.svg"),
            true});
        icon->setFixedSize(40, 40);
        auto *message = new ZzFluentUI::ZzMessageBar(&host);
        message->setText(QStringLiteral("Saved"));
        message->setTimeoutMilliseconds(60000);
        auto *calendar = new ZzFluentUI::ZzCalendar(&host);
        calendar->setDateRange(QDate(2026, 1, 1), QDate(2026, 12, 31));
        calendar->setSelectedDate(QDate(2026, 8, 5));
        auto *picker = new ZzFluentUI::ZzCalendarPicker(&host);
        picker->setDateRange(QDate(2026, 1, 1), QDate(2026, 12, 31));
        picker->setDate(QDate(2026, 8, 5));
        toggle->setStyle(&style);
        button->setStyle(&style);
        icon->setStyle(&style);
        message->setStyle(&style);
        calendar->setStyle(&style);
        picker->setStyle(&style);
        layout->addWidget(toggle);
        layout->addWidget(button);
        layout->addWidget(icon);
        layout->addWidget(message);
        layout->addWidget(calendar);
        layout->addWidget(picker);
        host.resize(360, 640);
        host.show();
        QCoreApplication::processEvents();

        for (int warmup = 0; warmup < 10; ++warmup) {
            toggle->setChecked((warmup % 2) != 0);
            icon->setEnabled((warmup % 2) == 0);
            zzSendHoverCycle(button);
            zzSendHoverCycle(icon);
            zzSendHoverCycle(message);
            const int month = (warmup % 12) + 1;
            calendar->setCurrentPage(2026, month);
            picker->calendar()->setCurrentPage(2026, month);
        }
        icon->setEnabled(true);
        QCoreApplication::processEvents();

        const std::array<QObject *, 6> roots{
            toggle,
            button,
            icon,
            message,
            calendar,
            picker};
        const qsizetype initialDescendants = zzDescendantCount(roots);
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        const int initialCacheBytes = style.iconCacheBytes();
        QVERIFY(initialAnimations >= 1);
        QVERIFY(initialTimers >= 1);
        QVERIFY(initialCacheBytes > 0);
        QVERIFY(initialCacheBytes <= zzMaximumIconCacheBytes);

        for (int iteration = 0; iteration < 1000; ++iteration) {
            toggle->setChecked((iteration % 2) != 0);
            icon->setEnabled((iteration % 2) == 0);
            zzSendHoverCycle(button);
            zzSendHoverCycle(icon);
            zzSendHoverCycle(message);
            const int month = (iteration % 12) + 1;
            calendar->setCurrentPage(2026, month);
            calendar->setSelectedDate(QDate(2026, month, 15));
            picker->calendar()->setCurrentPage(2026, month);
            picker->setDate(QDate(2026, month, 15));
        }
        icon->setEnabled(true);
        QCoreApplication::processEvents();

        QCOMPARE(zzDescendantCount(roots), initialDescendants);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        QCOMPARE(style.iconCacheBytes(), initialCacheBytes);
        QVERIFY(style.iconCacheBytes() <= zzMaximumIconCacheBytes);
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-basic-controls descendants=%1 animations=%2 "
                   "timers=%3 iconCacheBytes=%4")
                   .arg(initialDescendants)
                   .arg(initialAnimations)
                   .arg(initialTimers)
                   .arg(initialCacheBytes);
    }

    void measuresCalendarMonthRendering()
    {
        ZzFluentUI::ZzCalendar calendar;
        calendar.setLocale(QLocale::c());
        calendar.setFirstDayOfWeek(Qt::Monday);
        calendar.setDateRange(QDate(2026, 1, 1), QDate(2026, 12, 31));
        calendar.setSelectedDate(QDate(2026, 8, 5));
        calendar.resize(320, 280);
        calendar.show();
        QCoreApplication::processEvents();
        QImage target(
            calendar.size(),
            QImage::Format_ARGB32_Premultiplied);
        std::vector<qint64> samples;
        samples.reserve(zzMeasuredFrames);

        for (int frame = -zzWarmupFrames;
             frame < zzMeasuredFrames;
             ++frame) {
            const int sequence = frame + zzWarmupFrames;
            const qint64 elapsed = zzRenderCalendarFrame(
                &calendar,
                &target,
                sequence);
            if (frame >= 0) {
                samples.push_back(elapsed);
            }
        }

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-calendar monthRender P50=%1 ms "
                   "P95=%2 ms max=%3 ms")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机日历 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }

    void measuresTabRenderingAndTransferStability()
    {
        constexpr int pageCount = 100;
        constexpr int transferRoundTrips = 1000;
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        host.setPalette(style.standardPalette());
        auto *layout = new QVBoxLayout(&host);
        auto *source = new ZzFluentUI::ZzTabWidget(&host);
        auto *targetTabs = new ZzFluentUI::ZzTabWidget(&host);
        source->setTabsClosable(true);
        targetTabs->setTabsClosable(true);
        layout->addWidget(source);
        layout->addWidget(targetTabs);

        QSet<QWidget *> expectedPages;
        for (int index = 0; index < pageCount; ++index) {
            auto *page = new QWidget(source);
            page->setAutoFillBackground(true);
            source->addTab(
                page,
                QStringLiteral("Workspace %1").arg(index + 1));
            expectedPages.insert(page);
        }
        host.resize(1000, 480);
        host.show();
        QCoreApplication::processEvents();
        QImage targetImage(
            host.size(),
            QImage::Format_ARGB32_Premultiplied);

        const auto renderFrame = [source, &host, &targetImage](int sequence) {
            const int from = (sequence * 7) % source->count();
            const int slot = (from + 5) % (source->count() + 1);
            if (!source->transferTabTo(source, from, slot)) {
                return qint64(-1);
            }
            source->setCurrentIndex(sequence % source->count());
            QElapsedTimer timer;
            timer.start();
            targetImage.fill(Qt::transparent);
            QPainter painter(&targetImage);
            host.render(&painter);
            painter.end();
            return timer.nsecsElapsed();
        };

        for (int warmup = 0; warmup < zzWarmupFrames; ++warmup) {
            QVERIFY(renderFrame(warmup) >= 0);
        }
        QCoreApplication::processEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();

        std::vector<qint64> samples;
        samples.reserve(zzMeasuredFrames);
        for (int frame = 0; frame < zzMeasuredFrames; ++frame) {
            const qint64 elapsed = renderFrame(frame + zzWarmupFrames);
            QVERIFY(elapsed >= 0);
            samples.push_back(elapsed);
        }

        QElapsedTimer transferTimer;
        transferTimer.start();
        for (int iteration = 0;
             iteration < transferRoundTrips;
             ++iteration) {
            QWidget *page = source->widget(iteration % source->count());
            QVERIFY(page != nullptr);
            QVERIFY(source->transferTabTo(targetTabs, source->indexOf(page)));
            QVERIFY(targetTabs->transferTabTo(
                source,
                targetTabs->indexOf(page)));
            QCoreApplication::sendPostedEvents(
                nullptr,
                QEvent::DeferredDelete);
        }
        const qint64 transferNanoseconds = transferTimer.nsecsElapsed();
        QCoreApplication::processEvents();

        QSet<QWidget *> actualPages;
        for (int index = 0; index < source->count(); ++index) {
            actualPages.insert(source->widget(index));
        }
        QCOMPARE(actualPages, expectedPages);
        QCOMPARE(source->count(), pageCount);
        QCOMPARE(targetTabs->count(), 0);
        QCOMPARE(
            host.findChildren<QObject *>().size(),
            initialDescendants);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        const qreal transferAverageMicroseconds =
            static_cast<qreal>(transferNanoseconds)
            / static_cast<qreal>(transferRoundTrips)
            / 1000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-tabs pages=100 P50=%1 ms P95=%2 ms max=%3 ms "
                   "roundTrips=1000 average=%4 us descendants=%5")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3)
                   .arg(transferAverageMicroseconds, 0, 'f', 3)
                   .arg(initialDescendants);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机标签页 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }

    void measuresProgressRingRenderingAndStability()
    {
        constexpr int ringCount = 100;
        constexpr int determinateCount = 80;
        constexpr int indeterminateCount = ringCount - determinateCount;
        constexpr int columnCount = 10;
        constexpr int cellExtent = 56;
        constexpr int stateChangeCount = 1000;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        host.setPalette(style.standardPalette());
        std::vector<ZzFluentUI::ZzProgressRing *> rings;
        rings.reserve(ringCount);

        for (int index = 0; index < ringCount; ++index) {
            auto *ring = new ZzFluentUI::ZzProgressRing(&host);
            ring->setGeometry(
                (index % columnCount) * cellExtent,
                (index / columnCount) * cellExtent,
                cellExtent,
                cellExtent);
            ring->setTextVisible((index % 4) == 0);
            ring->setRingWidth(3 + (index % 4));
            if (index < determinateCount) {
                ring->setValue((index * 17) % 101);
            } else {
                ring->setRange(0, 0);
            }
            rings.push_back(ring);
        }

        host.resize(columnCount * cellExtent, columnCount * cellExtent);
        host.show();
        QCoreApplication::processEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        QCOMPARE(initialAnimations, ringCount);
        QCOMPARE(initialTimers, 0);
        const bool animationsEnabled = style.styleHint(
            QStyle::SH_Widget_Animate,
            nullptr,
            rings.front()) != 0;
        const auto runningAnimationCount = [&host] {
            const auto animations =
                host.findChildren<QAbstractAnimation *>();
            return std::count_if(
                animations.cbegin(),
                animations.cend(),
                [](const QAbstractAnimation *animation) {
                    return animation->state()
                        == QAbstractAnimation::Running;
                });
        };
        QCOMPARE(
            runningAnimationCount(),
            animationsEnabled ? indeterminateCount : 0);

        QImage target(host.size(), QImage::Format_ARGB32_Premultiplied);
        std::vector<qint64> samples;
        samples.reserve(zzProgressMeasuredFrames);
        for (int frame = -zzWarmupFrames;
             frame < zzProgressMeasuredFrames;
             ++frame) {
            const int sequence = frame + zzWarmupFrames;
            for (int index = 0; index < determinateCount; ++index) {
                rings[static_cast<std::size_t>(index)]->setValue(
                    (sequence + (index * 17)) % 101);
            }
            QElapsedTimer timer;
            timer.start();
            target.fill(Qt::transparent);
            QPainter painter(&target);
            host.render(&painter);
            painter.end();
            if (frame >= 0) {
                samples.push_back(timer.nsecsElapsed());
            }
        }

        for (int iteration = 0;
             iteration < stateChangeCount;
             ++iteration) {
            auto *ring = rings[static_cast<std::size_t>(
                iteration % ringCount)];
            if ((iteration % 2) == 0) {
                ring->setRange(0, 0);
            } else {
                ring->setRange(20, 120);
                ring->setValue(20 + (iteration % 101));
            }
        }
        for (int index = 0; index < ringCount; ++index) {
            auto *ring = rings[static_cast<std::size_t>(index)];
            if (index < determinateCount) {
                ring->setRange(0, 100);
                ring->setValue((index * 17) % 101);
            } else {
                ring->setRange(0, 0);
            }
        }
        QCoreApplication::processEvents();

        QCOMPARE(
            host.findChildren<QObject *>().size(),
            initialDescendants);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        QCOMPARE(
            runningAnimationCount(),
            animationsEnabled ? indeterminateCount : 0);

        host.hide();
        QCOMPARE(runningAnimationCount(), 0);

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-progress-rings controls=100 indeterminate=20 "
                   "P50=%1 ms P95=%2 ms max=%3 ms descendants=%4 "
                   "animations=%5 timers=%6")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3)
                   .arg(initialDescendants)
                   .arg(initialAnimations)
                   .arg(initialTimers);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机环形进度 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }

    void measuresScrollBarRenderingAndStability()
    {
        constexpr int scrollBarCount = 100;
        constexpr int columnCount = 10;
        constexpr int cellWidth = 88;
        constexpr int cellHeight = 20;
        constexpr int stateChangeRounds = 1000;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        host.setPalette(style.standardPalette());
        std::vector<ZzFluentUI::ZzScrollBar *> scrollBars;
        scrollBars.reserve(scrollBarCount);

        for (int index = 0; index < scrollBarCount; ++index) {
            auto *scrollBar = new ZzFluentUI::ZzScrollBar(
                Qt::Horizontal,
                &host);
            scrollBar->setGeometry(
                (index % columnCount) * cellWidth,
                (index / columnCount) * cellHeight,
                cellWidth - 8,
                12);
            scrollBar->setRange(0, 1000 + index);
            scrollBar->setPageStep(20 + (index % 61));
            scrollBar->setValue((index * 37) % 1001);
            scrollBars.push_back(scrollBar);
        }

        host.resize(
            columnCount * cellWidth,
            (scrollBarCount / columnCount) * cellHeight);
        host.show();
        QCoreApplication::processEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        QCOMPARE(initialAnimations, scrollBarCount);
        QCOMPARE(initialTimers, 0);

        QImage target(host.size(), QImage::Format_ARGB32_Premultiplied);
        std::vector<qint64> samples;
        samples.reserve(zzProgressMeasuredFrames);
        for (int frame = -zzWarmupFrames;
             frame < zzProgressMeasuredFrames;
             ++frame) {
            const int sequence = frame + zzWarmupFrames;
            for (int index = 0; index < scrollBarCount; ++index) {
                scrollBars[static_cast<std::size_t>(index)]->setValue(
                    (sequence * 13 + index * 37) % 1001);
            }
            QElapsedTimer timer;
            timer.start();
            target.fill(Qt::transparent);
            QPainter painter(&target);
            host.render(&painter);
            painter.end();
            if (frame >= 0) {
                samples.push_back(timer.nsecsElapsed());
            }
        }

        for (int round = 0; round < stateChangeRounds; ++round) {
            for (int index = 0; index < scrollBarCount; ++index) {
                auto *scrollBar = scrollBars[static_cast<std::size_t>(index)];
                const int maximum = 100 + ((round + index) % 1000);
                scrollBar->setRange(0, maximum);
                scrollBar->setValue((round * 7 + index) % (maximum + 1));
                scrollBar->setOrientation(
                    ((round + index) % 2) == 0
                        ? Qt::Horizontal
                        : Qt::Vertical);
                zzSendHoverCycle(scrollBar);
            }
        }
        for (ZzFluentUI::ZzScrollBar *scrollBar : scrollBars) {
            scrollBar->setOrientation(Qt::Horizontal);
        }

        QCOMPARE(
            host.findChildren<QObject *>().size(),
            initialDescendants);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);

        const auto runningAnimationCount = [&host] {
            const auto animations =
                host.findChildren<QAbstractAnimation *>();
            return std::count_if(
                animations.cbegin(),
                animations.cend(),
                [](const QAbstractAnimation *animation) {
                    return animation->state()
                        == QAbstractAnimation::Running;
                });
        };
        for (ZzFluentUI::ZzScrollBar *scrollBar : scrollBars) {
            QEnterEvent enter(
                QPointF(1.0, 1.0),
                QPointF(1.0, 1.0),
                QPointF(1.0, 1.0));
            QCoreApplication::sendEvent(scrollBar, &enter);
        }
        host.hide();
        QCOMPARE(runningAnimationCount(), 0);

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-scroll-bars controls=100 frames=120 "
                   "P50=%1 ms P95=%2 ms max=%3 ms descendants=%4 "
                   "animations=%5 timers=%6")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3)
                   .arg(initialDescendants)
                   .arg(initialAnimations)
                   .arg(initialTimers);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机滚动条 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }

    void measuresSpinBoxRenderingAndStability()
    {
        constexpr int integerCount = 50;
        constexpr int floatingCount = 50;
        constexpr int controlCount = integerCount + floatingCount;
        constexpr int columnCount = 10;
        constexpr int cellWidth = 132;
        constexpr int cellHeight = 40;
        constexpr int stateChangeRounds = 1000;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        host.setPalette(style.standardPalette());
        std::vector<ZzFluentUI::ZzSpinBox *> integers;
        std::vector<ZzFluentUI::ZzDoubleSpinBox *> floating;
        integers.reserve(integerCount);
        floating.reserve(floatingCount);

        for (int index = 0; index < controlCount; ++index) {
            const QRect geometry(
                (index % columnCount) * cellWidth,
                (index / columnCount) * cellHeight,
                cellWidth - 8,
                32);
            if (index < integerCount) {
                auto *spinBox = new ZzFluentUI::ZzSpinBox(&host);
                spinBox->setGeometry(geometry);
                spinBox->setRange(-1000, 1000);
                spinBox->setValue((index * 37) % 1001);
                spinBox->setSuffix(QStringLiteral(" u"));
                integers.push_back(spinBox);
            } else {
                auto *spinBox = new ZzFluentUI::ZzDoubleSpinBox(&host);
                spinBox->setGeometry(geometry);
                spinBox->setRange(-100.0, 100.0);
                spinBox->setDecimals(2);
                spinBox->setValue(
                    static_cast<qreal>(index - integerCount) / 4.0);
                spinBox->setSuffix(QStringLiteral(" ms"));
                floating.push_back(spinBox);
            }
        }

        host.resize(
            columnCount * cellWidth,
            (controlCount / columnCount) * cellHeight);
        host.show();
        QCoreApplication::processEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        QCOMPARE(initialAnimations, 0);
        QCOMPARE(initialTimers, 0);

        QImage target(host.size(), QImage::Format_ARGB32_Premultiplied);
        std::vector<qint64> samples;
        samples.reserve(zzProgressMeasuredFrames);
        for (int frame = -zzWarmupFrames;
             frame < zzProgressMeasuredFrames;
             ++frame) {
            const int sequence = frame + zzWarmupFrames;
            for (int index = 0; index < integerCount; ++index) {
                integers[static_cast<std::size_t>(index)]->setValue(
                    ((sequence * 13 + index * 37) % 2001) - 1000);
            }
            for (int index = 0; index < floatingCount; ++index) {
                floating[static_cast<std::size_t>(index)]->setValue(
                    static_cast<qreal>(
                        ((sequence * 7 + index * 19) % 2001) - 1000)
                    / 10.0);
            }
            QElapsedTimer timer;
            timer.start();
            target.fill(Qt::transparent);
            QPainter painter(&target);
            host.render(&painter);
            painter.end();
            if (frame >= 0) {
                samples.push_back(timer.nsecsElapsed());
            }
        }

        for (int round = 0; round < stateChangeRounds; ++round) {
            const QAbstractSpinBox::ButtonSymbols symbols =
                static_cast<QAbstractSpinBox::ButtonSymbols>(round % 3);
            const Qt::LayoutDirection direction = round % 2 == 0
                ? Qt::LeftToRight
                : Qt::RightToLeft;
            for (int index = 0; index < integerCount; ++index) {
                auto *spinBox = integers[static_cast<std::size_t>(index)];
                const int maximum = 100 + ((round + index) % 1000);
                spinBox->setRange(-maximum, maximum);
                spinBox->setValue((round * 7 + index) % maximum);
                spinBox->setButtonSymbols(symbols);
                spinBox->setLayoutDirection(direction);
            }
            for (int index = 0; index < floatingCount; ++index) {
                auto *spinBox = floating[static_cast<std::size_t>(index)];
                const qreal maximum = static_cast<qreal>(
                    100 + ((round + index) % 1000));
                spinBox->setRange(-maximum, maximum);
                spinBox->setValue(
                    static_cast<qreal>((round * 11 + index) % 1000)
                    / 10.0);
                spinBox->setButtonSymbols(symbols);
                spinBox->setLayoutDirection(direction);
            }
        }

        QCOMPARE(
            host.findChildren<QObject *>().size(),
            initialDescendants);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-spin-boxes controls=100 frames=120 "
                   "P50=%1 ms P95=%2 ms max=%3 ms descendants=%4 "
                   "animations=%5 timers=%6")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3)
                   .arg(initialDescendants)
                   .arg(initialAnimations)
                   .arg(initialTimers);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机数值输入 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }

    void measuresPreconstructedCardGrid()
    {
        constexpr int actionCardCount = 100;
        constexpr int imageCardCount = 40;
        constexpr int columnCount = 5;
        constexpr QSize actionCardSize(180, 64);
        constexpr QSize imageCardSize(200, 180);
        constexpr int actionRows = actionCardCount / columnCount;
        constexpr int imageRows = imageCardCount / columnCount;
        QWidget host;
        host.setAutoFillBackground(true);
        std::vector<ZzFluentUI::ZzActionCard *> actionCards;
        std::vector<ZzFluentUI::ZzImageCard *> imageCards;
        actionCards.reserve(actionCardCount);
        imageCards.reserve(imageCardCount);

        for (int index = 0; index < actionCardCount; ++index) {
            auto *card = new ZzFluentUI::ZzActionCard(
                QStringLiteral("Action %1").arg(index),
                QStringLiteral("Local card description"),
                &host);
            card->setCheckable(true);
            card->setGeometry(
                (index % columnCount) * actionCardSize.width(),
                (index / columnCount) * actionCardSize.height(),
                actionCardSize.width(),
                actionCardSize.height());
            actionCards.push_back(card);
        }

        QPixmap sharedPixmap(320, 180);
        sharedPixmap.fill(host.palette().color(QPalette::Highlight));
        const int imageTop = actionRows * actionCardSize.height();
        for (int index = 0; index < imageCardCount; ++index) {
            auto *card = new ZzFluentUI::ZzImageCard(
                QStringLiteral("Image %1").arg(index),
                QStringLiteral("Shared pixmap"),
                &host);
            card->setCheckable(true);
            card->setPixmap(sharedPixmap);
            card->setGeometry(
                (index % columnCount) * imageCardSize.width(),
                imageTop + (index / columnCount) * imageCardSize.height(),
                imageCardSize.width(),
                imageCardSize.height());
            imageCards.push_back(card);
        }

        host.resize(
            columnCount * imageCardSize.width(),
            imageTop + imageRows * imageCardSize.height());
        host.show();
        QCoreApplication::processEvents();
        const qsizetype initialDescendants =
            host.findChildren<QObject *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qint64 sharedCacheKey = sharedPixmap.cacheKey();
        QImage target(
            host.size(),
            QImage::Format_ARGB32_Premultiplied);
        std::vector<qint64> samples;
        samples.reserve(zzMeasuredFrames);

        for (int frame = -zzWarmupFrames;
             frame < zzMeasuredFrames;
             ++frame) {
            const int sequence = frame + zzWarmupFrames;
            const qint64 elapsed = zzRenderCardFrame(
                &host,
                actionCards,
                imageCards,
                &target,
                sequence);
            if (frame >= 0) {
                samples.push_back(elapsed);
            }
        }
        for (int iteration = 0; iteration < 1000; ++iteration) {
            const int actionIndex = iteration % actionCardCount;
            const int imageIndex = iteration % imageCardCount;
            actionCards[static_cast<std::size_t>(actionIndex)]
                ->setChecked((iteration % 2) != 0);
            imageCards[static_cast<std::size_t>(imageIndex)]
                ->setAspectRatioMode(
                    static_cast<Qt::AspectRatioMode>(iteration % 3));
        }

        QCOMPARE(
            host.findChildren<QObject *>().size(),
            initialDescendants);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QVERIFY(std::all_of(
            imageCards.cbegin(),
            imageCards.cend(),
            [sharedCacheKey](const ZzFluentUI::ZzImageCard *card) {
                return card->pixmap().cacheKey() == sharedCacheKey;
            }));

        std::sort(samples.begin(), samples.end());
        const qreal p50 = zzPercentileMilliseconds(samples, 0.50);
        const qreal p95 = zzPercentileMilliseconds(samples, 0.95);
        const qreal maximum =
            static_cast<qreal>(samples.back()) / 1000000.0;
        qInfo().noquote()
            << QStringLiteral(
                   "fluent-cards action=100 image=40 P50=%1 ms "
                   "P95=%2 ms max=%3 ms descendants=%4")
                   .arg(p50, 0, 'f', 3)
                   .arg(p95, 0, 'f', 3)
                   .arg(maximum, 0, 'f', 3)
                   .arg(initialDescendants);

        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= zzReferenceP95Milliseconds,
                qPrintable(QStringLiteral(
                    "参考机卡片 P95 %1 ms 超过 16.7 ms 帧预算")
                               .arg(p95, 0, 'f', 3)));
        }
    }
};

QTEST_MAIN(ZzBasicControlsBenchmark)

#include "ZzBasicControlsBenchmark.moc"
