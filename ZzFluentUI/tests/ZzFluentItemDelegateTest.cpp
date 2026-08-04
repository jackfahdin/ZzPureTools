#include <QtCore/QAbstractListModel>
#include <QtCore/QSet>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>
#include <QtWidgets/QStyleOptionViewItem>

#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzItemDensity.h>

/** @brief 记录可见 index 数据请求且不为十万行预分配 item。 */
class ZzVisibleRowsModel final : public QAbstractListModel
{
public:
    /** @brief 创建具有固定逻辑行数的即时展示模型。 */
    explicit ZzVisibleRowsModel(
        int rows,
        QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , rows_(rows)
    {
    }

    /** @brief 返回根索引下的固定逻辑行数。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : rows_;
    }

    /** @brief 即时返回当前 index 的展示角色并记录访问行。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows_) {
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

    /** @brief 批量填充当前 index 的 role span 并统计一次调用。 */
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

    /** @brief 返回去重后的所有已访问行。 */
    [[nodiscard]] QSet<int> requestedRows() const
    {
        return requestedRows_;
    }

    /** @brief 返回 multiData 调用总数。 */
    [[nodiscard]] int multiDataCalls() const noexcept
    {
        return multiDataCalls_;
    }

    /** @brief 清空请求统计，不改变模型内容。 */
    void clearStatistics() const
    {
        requestedRows_.clear();
        multiDataCalls_ = 0;
    }

private:
    int rows_ = 0;
    mutable QSet<int> requestedRows_;
    mutable int multiDataCalls_ = 0;
};

/** @brief 验证 delegate 数据访问与指定可见行数量同阶。 */
class ZzFluentItemDelegateTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyReadsExplicitlyPaintedRows()
    {
        ZzVisibleRowsModel model(100000);
        ZzFluentUI::ZzFluentItemDelegate delegate;
        QSet<int> expectedRows;
        QImage image(
            QSize(240, 40),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = image.rect();
        option.state = QStyle::State_Enabled;

        for (int visible = 0; visible < 40; ++visible) {
            const int row = 3 + (visible * 2000);
            expectedRows.insert(row);
            delegate.paint(
                &painter,
                option,
                model.index(row, 0));
        }
        painter.end();

        QCOMPARE(model.requestedRows(), expectedRows);
        QVERIFY(model.multiDataCalls() <= 120);
        QVERIFY(model.multiDataCalls() >= 40);
    }

    void returnsDeterministicDensityHeights()
    {
        ZzVisibleRowsModel model(1);
        ZzFluentUI::ZzFluentItemDelegate delegate;
        QStyleOptionViewItem option;

        QCOMPARE(
            delegate.density(),
            ZzFluentUI::ZzItemDensity::Standard);
        QCOMPARE(delegate.sizeHint(option, model.index(0, 0)).height(), 40);
        delegate.setDensity(ZzFluentUI::ZzItemDensity::Compact);
        QCOMPARE(
            delegate.density(),
            ZzFluentUI::ZzItemDensity::Compact);
        QCOMPARE(delegate.sizeHint(option, model.index(0, 0)).height(), 32);
    }
};

QTEST_MAIN(ZzFluentItemDelegateTest)

#include "ZzFluentItemDelegateTest.moc"
