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
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace {

constexpr int zzModelRows = 100000;
constexpr int zzVisibleRows = 40;
constexpr int zzWarmupFrames = 10;
constexpr int zzMeasuredFrames = 100;
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
int zzDescendantCount(const std::array<QObject *, 4> &roots)
{
    int total = 0;
    for (QObject *root : roots) {
        total += root->findChildren<QObject *>().size();
    }
    return total;
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
        toggle->setStyle(&style);
        button->setStyle(&style);
        icon->setStyle(&style);
        message->setStyle(&style);
        layout->addWidget(toggle);
        layout->addWidget(button);
        layout->addWidget(icon);
        layout->addWidget(message);
        host.resize(320, 240);
        host.show();
        QCoreApplication::processEvents();

        for (int warmup = 0; warmup < 10; ++warmup) {
            toggle->setChecked((warmup % 2) != 0);
            icon->setEnabled((warmup % 2) == 0);
            zzSendHoverCycle(button);
            zzSendHoverCycle(icon);
            zzSendHoverCycle(message);
        }
        icon->setEnabled(true);
        QCoreApplication::processEvents();

        const std::array<QObject *, 4> roots{
            toggle,
            button,
            icon,
            message};
        const int initialDescendants = zzDescendantCount(roots);
        const int initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const int initialTimers = host.findChildren<QTimer *>().size();
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
};

QTEST_MAIN(ZzBasicControlsBenchmark)

#include "ZzBasicControlsBenchmark.moc"
