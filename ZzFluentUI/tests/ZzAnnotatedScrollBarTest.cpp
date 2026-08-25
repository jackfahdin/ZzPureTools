#include <cmath>
#include <limits>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QAbstractTableModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QHelpEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionSlider>
#include <QtWidgets/QToolTip>

#include <ZzFluentUI/ZzAnnotatedScrollBar.h>

namespace {

using ZzFluentUI::ZzAnnotatedScrollBar;
using ZzFluentUI::ZzScrollMarkerKind;
using ZzFluentUI::ZzScrollMarkerRole;

/** @brief 写入单个模型标记；这些测试会在角色解析或缓存重建损坏时失败。 */
void zzSetMarker(
    QStandardItemModel &model,
    int row,
    qreal position,
    ZzScrollMarkerKind kind = ZzScrollMarkerKind::Information,
    int priority = 0,
    const QString &toolTip = {})
{
    const QModelIndex index = model.index(row, 0);
    model.setData(index, position, static_cast<int>(ZzScrollMarkerRole::Position));
    model.setData(index, static_cast<int>(kind), static_cast<int>(ZzScrollMarkerRole::Kind));
    model.setData(index, priority, static_cast<int>(ZzScrollMarkerRole::Priority));
    model.setData(index, toolTip, Qt::ToolTipRole);
}

/** @brief 按视觉方向换算归一化标记点，锁定垂直、水平和 RTL 映射。 */
QPoint zzMarkerPointFor(const ZzAnnotatedScrollBar &bar, qreal position)
{
    const qreal bounded = qBound(0.0, position, 1.0);
    QStyleOptionSlider option;
    option.initFrom(&bar);
    option.orientation = bar.orientation();
    option.minimum = bar.minimum();
    option.maximum = bar.maximum();
    option.pageStep = bar.pageStep();
    option.sliderPosition = bar.sliderPosition();
    option.sliderValue = bar.value();
    const QRect groove = bar.style()->subControlRect(
        QStyle::CC_ScrollBar,
        &option,
        QStyle::SC_ScrollBarGroove,
        &bar);
    if (bar.orientation() == Qt::Vertical) {
        return QPoint(
            groove.center().x(),
            groove.top() + qRound(bounded * qMax(0, groove.height() - 1)));
    }

    const bool reverse = (bar.layoutDirection() == Qt::RightToLeft)
        != bar.invertedAppearance();
    const qreal visualPosition = reverse ? 1.0 - bounded : bounded;
    return QPoint(
        groove.left()
            + qRound(visualPosition * qMax(0, groove.width() - 1)),
        groove.center().y());
}

/** @brief 统计控件子对象预算，防止标记数创建额外 timer 或 animation。 */
qsizetype zzPresentationObjectCount(const QObject &object)
{
    return object.findChildren<QObject *>().size();
}

/** @brief 计数真实模型 data() 调用，用于证明 paint 只读像素桶缓存。 */
class ZzCountingMarkerModel final : public QAbstractTableModel
{
public:
    explicit ZzCountingMarkerModel(int rows, QObject *parent = nullptr)
        : QAbstractTableModel(parent)
        , rows_(rows)
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
        ++dataCalls_;
        if (!index.isValid() || role == Qt::DisplayRole) {
            return {};
        }
        if (role == static_cast<int>(ZzScrollMarkerRole::Position)) {
            return static_cast<qreal>(index.row()) / qMax(1, rows_ - 1);
        }
        if (role == static_cast<int>(ZzScrollMarkerRole::Kind)) {
            return static_cast<int>(ZzScrollMarkerKind::Information);
        }
        if (role == static_cast<int>(ZzScrollMarkerRole::Priority)) {
            return 0;
        }
        return {};
    }

    void resetDataCallCount() const noexcept
    {
        dataCalls_ = 0;
    }

    [[nodiscard]] int dataCallCount() const noexcept
    {
        return dataCalls_;
    }

private:
    int rows_ = 0;
    mutable int dataCalls_ = 0;
};

} // namespace

/** @brief 验证模型驱动标记滚动条的生命周期、交互与规模合同。 */
class ZzAnnotatedScrollBarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rebuildsForResetInsertRemoveAndDataChanges()
    {
        QStandardItemModel model(1, 1);
        zzSetMarker(model, 0, 0.25);
        ZzAnnotatedScrollBar bar;
        bar.resize(16, 240);
        bar.setMarkerModel(&model);

        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.25)), model.index(0, 0));

        model.appendRow(new QStandardItem);
        zzSetMarker(model, 1, 0.75);
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.75)), model.index(1, 0));

        model.removeRow(0);
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.25)), QModelIndex());
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.75)), model.index(0, 0));

        zzSetMarker(model, 0, 0.5);
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.5)), model.index(0, 0));
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.75)), QModelIndex());

        model.clear();
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.5)), QModelIndex());
    }

    void ignoresInvalidPositionsWithoutMutatingTheModel()
    {
        QStandardItemModel model(4, 1);
        zzSetMarker(model, 0, -0.1);
        zzSetMarker(model, 1, 1.1);
        zzSetMarker(model, 2, std::numeric_limits<qreal>::quiet_NaN());
        zzSetMarker(model, 3, std::numeric_limits<qreal>::infinity());
        ZzAnnotatedScrollBar bar;
        bar.resize(16, 240);
        bar.setMarkerModel(&model);

        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, 0);
            QVERIFY(!bar.markerAt(zzMarkerPointFor(bar, 0.5)).isValid());
            const qreal storedPosition = model.data(
                index,
                static_cast<int>(ZzScrollMarkerRole::Position)).toDouble();
            if (row == 0) {
                QCOMPARE(storedPosition, -0.1);
            } else if (row == 1) {
                QCOMPARE(storedPosition, 1.1);
            } else if (row == 2) {
                QVERIFY(std::isnan(storedPosition));
            } else {
                QVERIFY(std::isinf(storedPosition));
            }
        }
    }

    void mergesPixelCollisionsByPriorityThenKind()
    {
        QStandardItemModel model(3, 1);
        zzSetMarker(model, 0, 0.5, ZzScrollMarkerKind::Information, 1);
        zzSetMarker(model, 1, 0.5, ZzScrollMarkerKind::Error, 1);
        zzSetMarker(model, 2, 0.5, ZzScrollMarkerKind::Success, 2);
        ZzAnnotatedScrollBar bar;
        bar.resize(16, 240);
        bar.setMarkerModel(&model);

        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.5)), model.index(2, 0));

        zzSetMarker(model, 2, 0.5, ZzScrollMarkerKind::Success, 1);
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.5)), model.index(1, 0));
    }

    void mapsMarkersForVerticalHorizontalAndRtl()
    {
        QStandardItemModel model(1, 1);
        zzSetMarker(model, 0, 0.25);
        ZzAnnotatedScrollBar vertical;
        vertical.resize(16, 240);
        vertical.setMarkerModel(&model);
        QCOMPARE(
            vertical.markerAt(zzMarkerPointFor(vertical, 0.25)),
            model.index(0, 0));

        ZzAnnotatedScrollBar horizontal(Qt::Horizontal);
        horizontal.resize(240, 16);
        horizontal.setMarkerModel(&model);
        QCOMPARE(
            horizontal.markerAt(zzMarkerPointFor(horizontal, 0.25)),
            model.index(0, 0));

        horizontal.setLayoutDirection(Qt::RightToLeft);
        QCOMPARE(
            horizontal.markerAt(zzMarkerPointFor(horizontal, 0.25)),
            model.index(0, 0));
    }

    void rebuildsBucketsForRuntimeOrientationAndInvertedAppearance()
    {
        QStandardItemModel model(1, 1);
        zzSetMarker(model, 0, 0.25);
        ZzAnnotatedScrollBar bar;
        bar.resize(240, 16);
        bar.setMarkerModel(&model);

        bar.setOrientation(Qt::Horizontal);
        QCOMPARE(
            bar.markerAt(zzMarkerPointFor(bar, 0.25)),
            model.index(0, 0));

        bar.setInvertedAppearance(true);
        QCOMPARE(
            bar.markerAt(zzMarkerPointFor(bar, 0.25)),
            model.index(0, 0));
    }

    void emitsSingleNullModelChangeWhenModelIsDestroyed()
    {
        auto *model = new QStandardItemModel(1, 1);
        zzSetMarker(*model, 0, 0.5);
        ZzAnnotatedScrollBar bar;
        bar.resize(16, 240);
        QSignalSpy modelSpy(&bar, &ZzAnnotatedScrollBar::markerModelChanged);
        bar.setMarkerModel(model);
        QCOMPARE(modelSpy.count(), 1);

        delete model;
        QCOMPARE(modelSpy.count(), 2);
        QCOMPARE(bar.markerModel(), nullptr);
        QCOMPARE(bar.markerAt(zzMarkerPointFor(bar, 0.5)), QModelIndex());
    }

    void activatesHitMarkerAndDelegatesMissToScrollBar()
    {
        QStandardItemModel model(1, 1);
        zzSetMarker(model, 0, 0.75, ZzScrollMarkerKind::Warning, 3);
        ZzAnnotatedScrollBar bar;
        bar.setRange(0, 1000);
        bar.setPageStep(100);
        bar.resize(16, 240);
        bar.setMarkerModel(&model);
        bar.setMarkersInteractive(true);
        bar.show();

        const QModelIndex expected = model.index(0, 0);
        const QPoint hit = zzMarkerPointFor(bar, 0.75);
        QCOMPARE(bar.markerAt(hit), expected);
        QSignalSpy activated(&bar, &ZzAnnotatedScrollBar::markerActivated);
        QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, hit);
        QCOMPARE(activated.count(), 1);
        QVERIFY(qAbs(bar.value() - 750) <= 1);

        const int beforeMiss = bar.value();
        QTest::mouseClick(
            &bar,
            Qt::LeftButton,
            Qt::NoModifier,
            zzMarkerPointFor(bar, 0.05));
        QCOMPARE(activated.count(), 1);
        QVERIFY(bar.value() < beforeMiss);
    }

    void usesCachedSourceTooltip()
    {
        QStandardItemModel model(1, 1);
        zzSetMarker(
            model,
            0,
            0.5,
            ZzScrollMarkerKind::Information,
            0,
            QStringLiteral("来自模型的提示"));
        ZzAnnotatedScrollBar bar;
        bar.resize(16, 240);
        bar.setMarkerModel(&model);
        bar.show();

        const QPoint point = zzMarkerPointFor(bar, 0.5);
        QHelpEvent event(QEvent::ToolTip, point, bar.mapToGlobal(point));
        QCoreApplication::sendEvent(&bar, &event);
        QCOMPARE(QToolTip::text(), QStringLiteral("来自模型的提示"));
        QToolTip::hideText();
    }

    void keepsObjectsFixedAndPaintsWithoutModelAccessAtScale()
    {
        ZzAnnotatedScrollBar bar;
        bar.resize(16, 240);
        const qsizetype initialObjects = zzPresentationObjectCount(bar);
        const qsizetype initialAnimations = bar.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers = bar.findChildren<QTimer *>().size();

        ZzCountingMarkerModel small(20);
        bar.setMarkerModel(&small);
        const qsizetype smallObjects = zzPresentationObjectCount(bar);

        ZzCountingMarkerModel large(100000);
        bar.setMarkerModel(&large);
        QCOMPARE(zzPresentationObjectCount(bar), smallObjects);
        QCOMPARE(smallObjects, initialObjects);
        QCOMPARE(bar.findChildren<QAbstractAnimation *>().size(), initialAnimations);
        QCOMPARE(bar.findChildren<QTimer *>().size(), initialTimers);

        large.resetDataCallCount();
        QImage image(bar.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        bar.render(&painter);
        painter.end();
        QCOMPARE(large.dataCallCount(), 0);
    }
};

QTEST_MAIN(ZzAnnotatedScrollBarTest)

#include "ZzAnnotatedScrollBarTest.moc"
