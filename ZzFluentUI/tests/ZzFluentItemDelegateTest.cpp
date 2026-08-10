#include <QtCore/QAbstractListModel>
#include <QtCore/QEvent>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QImage>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QTreeView>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzItemDensity.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

/** @brief 记录基础样式收到的 item 内容矩形。 */
class ZzItemRectRecordingStyle final : public QProxyStyle
{
public:
    /** @brief 清空此前记录的 item 内容矩形。 */
    void clearItemRects() const
    {
        itemRects_.clear();
    }

    /** @brief 返回按绘制顺序记录的 item 内容矩形。 */
    [[nodiscard]] const QList<QRect> &itemRects() const noexcept
    {
        return itemRects_;
    }

    /** @brief 记录 item 内容矩形后委托平台基础样式绘制。 */
    void drawControl(
        ControlElement element,
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget = nullptr) const override
    {
        if (element == CE_ItemViewItem) {
            const auto *item = qstyleoption_cast<
                const QStyleOptionViewItem *>(option);
            if (item != nullptr) {
                itemRects_.append(item->rect);
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

private:
    mutable QList<QRect> itemRects_;
};

/** @brief 统计树形选择变化后覆盖完整 viewport 的重绘事件。 */
class ZzViewportPaintProbe final : public QObject
{
public:
    /** @brief 绑定待观察的非空 viewport。 */
    explicit ZzViewportPaintProbe(QWidget *viewport)
        : QObject(viewport)
        , viewport_(viewport)
    {
        Q_ASSERT(viewport_ != nullptr);
        viewport_->installEventFilter(this);
    }

    /** @brief 清空完整 viewport 重绘次数。 */
    void reset() noexcept
    {
        fullPaintCount_ = 0;
    }

    /** @brief 返回覆盖完整 viewport 的重绘次数。 */
    [[nodiscard]] int fullPaintCount() const noexcept
    {
        return fullPaintCount_;
    }

protected:
    /** @brief 统计完整绘制区域并保留 viewport 默认事件处理。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (viewport_ != nullptr
            && watched == viewport_
            && event != nullptr
            && event->type() == QEvent::Paint) {
            const auto *paintEvent = static_cast<const QPaintEvent *>(event);
            if (paintEvent->region().contains(viewport_->rect())) {
                ++fullPaintCount_;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QWidget> viewport_;
    int fullPaintCount_ = 0;
};

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

    void offsetsAndMarksOnlyTheTreeColumn_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::addColumn<int>("treeColumn");

        QTest::newRow("ltr-first-column") << Qt::LeftToRight << 0;
        QTest::newRow("ltr-second-column") << Qt::LeftToRight << 1;
        QTest::newRow("rtl-first-column") << Qt::RightToLeft << 0;
        QTest::newRow("rtl-second-column") << Qt::RightToLeft << 1;
    }

    void offsetsAndMarksOnlyTheTreeColumn()
    {
        QFETCH(Qt::LayoutDirection, direction);
        QFETCH(int, treeColumn);

        ZzFluentUI::ZzThemeController controller;
        auto *recordingStyle = new ZzItemRectRecordingStyle;
        ZzFluentUI::ZzFluentStyle style(&controller, recordingStyle);
        QTreeView tree;
        tree.setStyle(&style);
        tree.setLayoutDirection(direction);
        tree.setTreePosition(treeColumn);
        QStandardItemModel model(1, 2);
        model.setData(model.index(0, 0), QStringLiteral("Root"));
        model.setData(model.index(0, 1), QStringLiteral("Details"));
        tree.setModel(&model);
        ZzFluentUI::ZzFluentItemDelegate delegate;

        QImage image(
            QSize(240, 40),
            QImage::Format_ARGB32_Premultiplied);
        image.fill(style.standardPalette().color(QPalette::Base));
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.state = QStyle::State_Enabled | QStyle::State_Selected;
        option.direction = direction;
        option.widget = &tree;

        for (int column = 0; column < 2; ++column) {
            option.rect = QRect(column * 120, 0, 120, 40);
            delegate.paint(&painter, option, model.index(0, column));
        }
        painter.end();

        QCOMPARE(recordingStyle->itemRects().size(), 2);
        for (int column = 0; column < 2; ++column) {
            QRect expected(column * 120, 0, 120, 40);
            if (column == treeColumn) {
                expected.adjust(
                    direction == Qt::RightToLeft ? 0 : 10,
                    0,
                    direction == Qt::RightToLeft ? -10 : 0,
                    0);
            }
            QCOMPARE(recordingStyle->itemRects().at(column), expected);
        }

        const QColor accent = controller.snapshot()->color(
            ZzFluentUI::ZzColorToken::Accent);
        for (int column = 0; column < 2; ++column) {
            const QRect cell(column * 120, 0, 120, 40);
            int accentPixels = 0;
            for (int y = cell.top(); y <= cell.bottom(); ++y) {
                for (int x = cell.left(); x <= cell.right(); ++x) {
                    if (image.pixelColor(x, y) == accent) {
                        ++accentPixels;
                    }
                }
            }
            if (column == treeColumn) {
                QVERIFY(accentPixels > 0);
            } else {
                QCOMPARE(accentPixels, 0);
            }
        }
    }

    void repaintsCompleteTreeViewportWhenSelectionChanges()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QTreeView tree;
        tree.setStyle(&style);
        tree.resize(320, 180);
        tree.setSelectionBehavior(QAbstractItemView::SelectRows);
        QStandardItemModel model(3, 2);
        for (int row = 0; row < model.rowCount(); ++row) {
            model.setData(
                model.index(row, 0),
                QStringLiteral("Node %1").arg(row));
            model.setData(
                model.index(row, 1),
                QStringLiteral("Value %1").arg(row));
        }
        tree.setModel(&model);
        auto *delegate = new ZzFluentUI::ZzFluentItemDelegate(&tree);
        tree.setItemDelegate(delegate);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));

        ZzViewportPaintProbe probe(tree.viewport());
        QCoreApplication::processEvents();
        probe.reset();
        tree.selectionModel()->select(
            model.index(0, 0),
            QItemSelectionModel::ClearAndSelect
                | QItemSelectionModel::Rows);

        QTRY_VERIFY_WITH_TIMEOUT(probe.fullPaintCount() > 0, 1000);

        // setModel/setSelectionModel 可在控件生命周期内替换选择模型；
        // 下一次绘制必须自动迁移连接，不能继续监听已经过期的模型。
        QItemSelectionModel replacementSelection(&model, &tree);
        tree.setSelectionModel(&replacementSelection);
        tree.viewport()->update();
        QCoreApplication::processEvents();
        probe.reset();
        replacementSelection.select(
            model.index(1, 0),
            QItemSelectionModel::ClearAndSelect
                | QItemSelectionModel::Rows);

        QTRY_VERIFY_WITH_TIMEOUT(probe.fullPaintCount() > 0, 1000);
    }

    void drawsTreeRowFromGeometryWhenPrimitiveOmitsIndexAndSelection_data()
    {
        QTest::addColumn<bool>("useViewportWidget");

        QTest::newRow("tree-widget") << false;
        QTest::newRow("viewport-widget") << true;
    }

    void drawsTreeRowFromGeometryWhenPrimitiveOmitsIndexAndSelection()
    {
        QFETCH(bool, useViewportWidget);

        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QTreeView tree;
        tree.setStyle(&style);
        tree.resize(240, 100);
        tree.header()->hide();
        QStandardItemModel model(1, 1);
        model.setData(model.index(0, 0), QStringLiteral("Workspace"));
        tree.setModel(&model);
        tree.setCurrentIndex(model.index(0, 0));
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));

        const QRect itemRect = tree.visualRect(model.index(0, 0));
        QVERIFY(itemRect.isValid());
        const QRect rowRect(
            tree.viewport()->rect().left(),
            itemRect.top(),
            tree.viewport()->rect().width(),
            itemRect.height());

        QImage image(
            tree.viewport()->size(),
            QImage::Format_ARGB32_Premultiplied);
        const QColor base = style.standardPalette().color(QPalette::Base);
        image.fill(base);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = QRect(
            rowRect.left(),
            rowRect.top(),
            tree.indentation(),
            rowRect.height());
        option.state = QStyle::State_Enabled;
        option.features = QStyleOptionViewItem::IsDecorationForRootColumn;
        option.palette = style.standardPalette();
        option.widget = useViewportWidget ? tree.viewport() : &tree;
        QVERIFY(!option.index.isValid());

        style.drawPrimitive(
            QStyle::PE_PanelItemViewRow,
            &option,
            &painter,
            option.widget);
        painter.end();

        QCOMPARE(
            image.pixelColor(rowRect.center()),
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlFillPressed));
    }
};

QTEST_MAIN(ZzFluentItemDelegateTest)

#include "ZzFluentItemDelegateTest.moc"
