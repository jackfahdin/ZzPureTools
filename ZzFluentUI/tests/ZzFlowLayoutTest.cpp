#include <algorithm>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFlowLayout.h>

namespace {

/** @brief 提供可计数、可配置 height-for-width 的轻量布局测试项。 */
class ZzMeasuredLayoutItem final : public QLayoutItem
{
public:
    /** @brief 创建具有稳定尺寸契约的非 widget item。 */
    explicit ZzMeasuredLayoutItem(
        QSize preferred,
        QSize minimum = {},
        QSize maximum = QSize(4096, 4096),
        Qt::Alignment alignment = {},
        int *destructionCounter = nullptr)
        : QLayoutItem(alignment)
        , preferredSize(preferred)
        , minimumExtent(minimum)
        , maximumExtent(maximum)
        , destructionCount(destructionCounter)
    {
    }

    /** @brief 记录所有权测试所需的析构次数。 */
    ~ZzMeasuredLayoutItem() override
    {
        if (destructionCount != nullptr) {
            ++(*destructionCount);
        }
    }

    /** @brief 返回测试指定的首选尺寸。 */
    [[nodiscard]] QSize sizeHint() const override
    {
        ++sizeHintCalls;
        return preferredSize;
    }

    /** @brief 返回测试指定的最小尺寸。 */
    [[nodiscard]] QSize minimumSize() const override
    {
        return minimumExtent;
    }

    /** @brief 返回测试指定的最大尺寸。 */
    [[nodiscard]] QSize maximumSize() const override
    {
        return maximumExtent;
    }

    /** @brief 测试项不主动扩展。 */
    [[nodiscard]] Qt::Orientations expandingDirections() const override
    {
        return {};
    }

    /** @brief 保存布局应用的最终矩形。 */
    void setGeometry(const QRect &rect) override
    {
        ++geometryWrites;
        appliedGeometry = rect;
    }

    /** @brief 返回最近一次应用的最终矩形。 */
    [[nodiscard]] QRect geometry() const override
    {
        return appliedGeometry;
    }

    /** @brief 返回测试配置的空状态。 */
    [[nodiscard]] bool isEmpty() const override
    {
        return empty;
    }

    /** @brief 返回是否启用测试 height-for-width 路径。 */
    [[nodiscard]] bool hasHeightForWidth() const override
    {
        return usesHeightForWidth;
    }

    /** @brief 按最终宽度计算高度并记录查询。 */
    [[nodiscard]] int heightForWidth(int width) const override
    {
        ++heightForWidthCalls;
        lastHeightForWidthInput = width;
        return heightOffset + width / 2;
    }

    QSize preferredSize;
    QSize minimumExtent;
    QSize maximumExtent;
    QRect appliedGeometry;
    int *destructionCount = nullptr;
    mutable int sizeHintCalls = 0;
    mutable int heightForWidthCalls = 0;
    mutable int lastHeightForWidthInput = -1;
    int geometryWrites = 0;
    int heightOffset = 0;
    bool empty = false;
    bool usesHeightForWidth = false;
};

/** @brief 把三个固定测试项加入布局并返回非拥有指针。 */
QList<ZzMeasuredLayoutItem *> zzAddMeasuredItems(
    ZzFluentUI::ZzFlowLayout *layout,
    const QList<QSize> &sizes)
{
    QList<ZzMeasuredLayoutItem *> result;
    result.reserve(sizes.size());
    for (const QSize &size : sizes) {
        auto *item = new ZzMeasuredLayoutItem(size, size, size);
        layout->addItem(item);
        result.append(item);
    }
    return result;
}

} // namespace

/** @brief 验证 ZzFlowLayout 的所有权、几何、缓存和跨方向契约。 */
class ZzFlowLayoutTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStablePropertiesAndSignals()
    {
        ZzFluentUI::ZzFlowLayout layout;
        QSignalSpy horizontalSpy(
            &layout,
            &ZzFluentUI::ZzFlowLayout::horizontalSpacingChanged);
        QSignalSpy verticalSpy(
            &layout,
            &ZzFluentUI::ZzFlowLayout::verticalSpacingChanged);

        QCOMPARE(layout.horizontalSpacing(), -1);
        QCOMPARE(layout.verticalSpacing(), -1);
        QCOMPARE(layout.count(), 0);
        QVERIFY(layout.hasHeightForWidth());
        QCOMPARE(layout.expandingDirections(), Qt::Orientations{});
        QVERIFY(layout.itemAt(-1) == nullptr);
        QVERIFY(layout.itemAt(0) == nullptr);
        QVERIFY(layout.takeAt(-1) == nullptr);
        QVERIFY(layout.takeAt(0) == nullptr);
        QVERIFY(layout.findChildren<QTimer *>().isEmpty());
        QVERIFY(layout.findChildren<QAbstractAnimation *>().isEmpty());

        layout.setHorizontalSpacing(-20);
        layout.setVerticalSpacing(-2);
        QCOMPARE(horizontalSpy.count(), 0);
        QCOMPARE(verticalSpy.count(), 0);
        layout.setHorizontalSpacing(6);
        layout.setVerticalSpacing(9);
        layout.setHorizontalSpacing(6);
        layout.setVerticalSpacing(9);
        QCOMPARE(horizontalSpy.count(), 1);
        QCOMPARE(verticalSpy.count(), 1);
        QCOMPARE(horizontalSpy.at(0).at(0).toInt(), 6);
        QCOMPARE(verticalSpy.at(0).at(0).toInt(), 9);

        layout.setHorizontalSpacing(-3);
        layout.setVerticalSpacing(-8);
        QCOMPARE(layout.horizontalSpacing(), -1);
        QCOMPARE(layout.verticalSpacing(), -1);
        QCOMPARE(horizontalSpy.count(), 2);
        QCOMPARE(verticalSpy.count(), 2);

        const QMetaObject *metaObject = layout.metaObject();
        QVERIFY(metaObject->indexOfProperty("horizontalSpacing") >= 0);
        QVERIFY(metaObject->indexOfProperty("verticalSpacing") >= 0);
    }

    void ownsItemsUntilTheyAreTaken()
    {
        int destructionCount = 0;
        {
            ZzFluentUI::ZzFlowLayout layout;
            auto *first = new ZzMeasuredLayoutItem(
                QSize(20, 10),
                {},
                QSize(4096, 4096),
                {},
                &destructionCount);
            auto *second = new ZzMeasuredLayoutItem(
                QSize(30, 12),
                {},
                QSize(4096, 4096),
                {},
                &destructionCount);
            layout.addItem(first);
            layout.addItem(second);

            QCOMPARE(layout.count(), 2);
            QCOMPARE(layout.itemAt(0), first);
            QCOMPARE(layout.itemAt(1), second);
            QVERIFY(layout.itemAt(2) == nullptr);
            QLayoutItem *const removed = layout.takeAt(0);
            QCOMPARE(removed, first);
            QCOMPARE(layout.count(), 1);
            QCOMPARE(layout.itemAt(0), second);
            delete removed;
            QCOMPARE(destructionCount, 1);
        }
        QCOMPARE(destructionCount, 2);
    }

    void wrapsAtExactContentBoundary()
    {
        ZzFluentUI::ZzFlowLayout layout(5, 7);
        layout.setContentsMargins(2, 3, 4, 5);
        const QList<ZzMeasuredLayoutItem *> items = zzAddMeasuredItems(
            &layout,
            {QSize(40, 20), QSize(40, 20), QSize(40, 20)});

        layout.setGeometry(QRect(10, 20, 91, 100));
        QCOMPARE(items.at(0)->geometry(), QRect(12, 23, 40, 20));
        QCOMPARE(items.at(1)->geometry(), QRect(57, 23, 40, 20));
        QCOMPARE(items.at(2)->geometry(), QRect(12, 50, 40, 20));
        QCOMPARE(layout.heightForWidth(91), 55);

        layout.setGeometry(QRect(10, 20, 90, 120));
        QCOMPARE(items.at(0)->geometry(), QRect(12, 23, 40, 20));
        QCOMPARE(items.at(1)->geometry(), QRect(12, 50, 40, 20));
        QCOMPARE(items.at(2)->geometry(), QRect(12, 77, 40, 20));
        QCOMPARE(layout.heightForWidth(90), 82);
    }

    void keepsOversizedItemsOnTheirOwnRows()
    {
        ZzFluentUI::ZzFlowLayout layout(4, 6);
        auto *oversized = new ZzMeasuredLayoutItem(
            QSize(80, 20),
            QSize(80, 20),
            QSize(80, 20));
        auto *following = new ZzMeasuredLayoutItem(
            QSize(20, 10),
            QSize(20, 10),
            QSize(20, 10));
        layout.addItem(oversized);
        layout.addItem(following);

        layout.setGeometry(QRect(0, 0, 50, 80));
        QCOMPARE(oversized->geometry(), QRect(0, 0, 80, 20));
        QCOMPARE(following->geometry(), QRect(0, 26, 20, 10));
        QCOMPARE(layout.heightForWidth(50), 36);
    }

    void excludesOnlyHiddenWidgetsFromFlow()
    {
        QWidget host;
        auto *layout = new ZzFluentUI::ZzFlowLayout(5, 7, &host);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *first = new QWidget(&host);
        auto *hidden = new QWidget(&host);
        auto *last = new QWidget(&host);
        first->setFixedSize(30, 10);
        hidden->setFixedSize(30, 10);
        last->setFixedSize(30, 10);
        layout->addWidget(first);
        layout->addWidget(hidden);
        layout->addWidget(last);

        layout->setGeometry(QRect(0, 0, 65, 80));
        QCOMPARE(first->geometry(), QRect(0, 0, 30, 10));
        QCOMPARE(hidden->geometry(), QRect(35, 0, 30, 10));
        QCOMPARE(last->geometry(), QRect(0, 17, 30, 10));
        hidden->hide();
        layout->invalidate();
        layout->setGeometry(QRect(0, 0, 65, 80));
        QCOMPARE(first->geometry(), QRect(0, 0, 30, 10));
        QCOMPARE(last->geometry(), QRect(35, 0, 30, 10));
        QCOMPARE(layout->heightForWidth(65), 10);

        hidden->show();
        layout->invalidate();
        layout->setGeometry(QRect(0, 0, 65, 80));
        QCOMPARE(hidden->geometry(), QRect(35, 0, 30, 10));
        QCOMPARE(last->geometry(), QRect(0, 17, 30, 10));
    }

    void supportsSpacersAndNestedLayouts()
    {
        ZzFluentUI::ZzFlowLayout layout(4, 6);
        auto *spacer = new QSpacerItem(
            24,
            12,
            QSizePolicy::Fixed,
            QSizePolicy::Fixed);
        auto *nested = new QHBoxLayout;
        nested->setContentsMargins(0, 0, 0, 0);
        nested->setSpacing(0);
        nested->addSpacerItem(new QSpacerItem(
            18,
            14,
            QSizePolicy::Fixed,
            QSizePolicy::Fixed));
        layout.addItem(spacer);
        layout.addItem(nested);

        layout.setGeometry(QRect(0, 0, 46, 80));
        QCOMPARE(spacer->geometry(), QRect(0, 0, 24, 12));
        QCOMPARE(nested->geometry(), QRect(28, 0, 18, 14));
        QCOMPARE(layout.heightForWidth(46), 14);
    }

    void mirrorsGeometryWithoutChangingLogicalOrder()
    {
        QWidget host;
        host.setLayoutDirection(Qt::RightToLeft);
        auto *layout = new ZzFluentUI::ZzFlowLayout(5, 7, &host);
        layout->setContentsMargins(0, 0, 0, 0);
        const QList<ZzMeasuredLayoutItem *> items = zzAddMeasuredItems(
            layout,
            {QSize(30, 10), QSize(20, 10), QSize(40, 12)});

        layout->setGeometry(QRect(10, 20, 100, 80));
        QCOMPARE(layout->itemAt(0), items.at(0));
        QCOMPARE(layout->itemAt(1), items.at(1));
        QCOMPARE(layout->itemAt(2), items.at(2));
        QCOMPARE(items.at(0)->geometry(), QRect(80, 20, 30, 10));
        QCOMPARE(items.at(1)->geometry(), QRect(55, 20, 20, 10));
        QCOMPARE(items.at(2)->geometry(), QRect(10, 20, 40, 12));

        host.setLayoutDirection(Qt::LeftToRight);
        layout->invalidate();
        layout->setGeometry(QRect(10, 20, 100, 80));
        QCOMPARE(items.at(0)->geometry(), QRect(10, 20, 30, 10));
        QCOMPARE(items.at(1)->geometry(), QRect(45, 20, 20, 10));
        QCOMPARE(items.at(2)->geometry(), QRect(70, 20, 40, 12));
    }

    void respectsHeightForWidthAndVerticalAlignment()
    {
        ZzFluentUI::ZzFlowLayout layout(5, 7);
        auto *top = new ZzMeasuredLayoutItem(
            QSize(20, 10),
            QSize(20, 10),
            QSize(20, 10),
            Qt::AlignTop);
        auto *center = new ZzMeasuredLayoutItem(
            QSize(20, 20),
            QSize(20, 20),
            QSize(20, 20),
            Qt::AlignVCenter);
        auto *bottom = new ZzMeasuredLayoutItem(
            QSize(20, 12),
            QSize(20, 12),
            QSize(20, 12),
            Qt::AlignBottom);
        layout.addItem(top);
        layout.addItem(center);
        layout.addItem(bottom);
        layout.setGeometry(QRect(0, 0, 70, 40));

        QCOMPARE(top->geometry(), QRect(0, 0, 20, 10));
        QCOMPARE(center->geometry(), QRect(25, 0, 20, 20));
        QCOMPARE(bottom->geometry(), QRect(50, 8, 20, 12));

        ZzFluentUI::ZzFlowLayout heightLayout(0, 0);
        auto *heightItem = new ZzMeasuredLayoutItem(
            QSize(80, 5),
            QSize(10, 5),
            QSize(100, 80));
        heightItem->usesHeightForWidth = true;
        heightItem->heightOffset = 3;
        heightLayout.addItem(heightItem);
        QCOMPARE(heightLayout.heightForWidth(40), 23);
        QCOMPARE(heightItem->lastHeightForWidthInput, 40);
        QCOMPARE(heightItem->heightForWidthCalls, 1);
        QCOMPARE(heightLayout.heightForWidth(40), 23);
        QCOMPARE(heightItem->heightForWidthCalls, 1);
        heightLayout.setHorizontalSpacing(2);
        QCOMPARE(heightLayout.heightForWidth(40), 23);
        QCOMPARE(heightItem->heightForWidthCalls, 2);
    }

    void cachesGeometryUntilInvalidated()
    {
        ZzFluentUI::ZzFlowLayout layout(4, 6);
        auto *first = new ZzMeasuredLayoutItem(
            QSize(20, 10),
            QSize(20, 10),
            QSize(20, 10));
        layout.addItem(first);
        layout.setGeometry(QRect(0, 0, 100, 40));
        QCOMPARE(first->geometryWrites, 1);
        layout.setGeometry(QRect(0, 0, 100, 40));
        QCOMPARE(first->geometryWrites, 1);

        auto *second = new ZzMeasuredLayoutItem(
            QSize(20, 10),
            QSize(20, 10),
            QSize(20, 10));
        layout.addItem(second);
        layout.setGeometry(QRect(0, 0, 100, 40));
        QCOMPARE(first->geometryWrites, 2);
        QCOMPARE(second->geometryWrites, 1);
        layout.setVerticalSpacing(8);
        layout.setGeometry(QRect(0, 0, 100, 40));
        QCOMPARE(first->geometryWrites, 3);
        QCOMPARE(second->geometryWrites, 2);
    }

    void reportsStableSizeHintsAndEmptyMargins()
    {
        ZzFluentUI::ZzFlowLayout layout(5, 7);
        layout.setContentsMargins(2, 3, 4, 5);
        QCOMPARE(layout.minimumSize(), QSize(6, 8));
        QCOMPARE(layout.sizeHint(), QSize(6, 8));
        QCOMPARE(layout.heightForWidth(-10), 8);

        auto *first = new ZzMeasuredLayoutItem(
            QSize(30, 12),
            QSize(10, 8),
            QSize(40, 20));
        auto *second = new ZzMeasuredLayoutItem(
            QSize(20, 16),
            QSize(14, 6),
            QSize(50, 30));
        layout.addItem(first);
        layout.addItem(second);
        QCOMPARE(layout.minimumSize(), QSize(20, 16));
        QCOMPARE(layout.sizeHint(), QSize(61, 24));
    }
};

QTEST_MAIN(ZzFlowLayoutTest)

#include "ZzFlowLayoutTest.moc"
