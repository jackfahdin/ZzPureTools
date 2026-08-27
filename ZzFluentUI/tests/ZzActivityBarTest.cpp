#include <algorithm>
#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtGui/QAction>
#include <QtGui/QColor>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

class ZzActivityRowsModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row final
    {
        ZzFluentUI::ZzActivityArea area;
        int badge = 0;
        bool enabled = true;
        bool draggable = true;
        QString text;
        ZzFluentUI::ZzIconDescriptor icon;
        bool selectable = true;
    };

    explicit ZzActivityRowsModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
        rows = {
            {ZzFluentUI::ZzActivityArea::LeftPrimary, 7, true, true,
             QStringLiteral("Left primary"), {}},
            {ZzFluentUI::ZzActivityArea::LeftSecondary, 0, false, false,
             QStringLiteral("Left disabled"), {}},
            {ZzFluentUI::ZzActivityArea::RightPrimary, 120, true, true,
             QStringLiteral("Right primary"), {}},
            {ZzFluentUI::ZzActivityArea::RightSecondary, 2, true, true,
             QStringLiteral("Right secondary"), {}},
        };
    }

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(rows.size());
    }

    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= static_cast<int>(rows.size())) {
            return {};
        }
        const Row &row = rows.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return row.text;
        case Qt::DecorationRole:
            return QVariant::fromValue(row.icon);
        case static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area):
            return QVariant::fromValue(row.area);
        case static_cast<int>(ZzFluentUI::ZzActivityItemRole::Badge):
            return row.badge;
        default:
            return {};
        }
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid()) {
            return Qt::NoItemFlags;
        }
        const Row &row = rows.at(index.row());
        Qt::ItemFlags result = Qt::NoItemFlags;
        if (row.selectable) {
            result |= Qt::ItemIsSelectable;
        }
        if (row.enabled) {
            result |= Qt::ItemIsEnabled;
        }
        if (row.draggable) {
            result |= Qt::ItemIsDragEnabled;
        }
        return result;
    }

    QList<Row> rows;
};

QListView *zzActivityView(
    ZzFluentUI::ZzActivityBar *bar,
    const QString &objectName)
{
    auto *view = bar->findChild<QListView *>(objectName);
    Q_ASSERT(view != nullptr);
    return view;
}

void zzShow(QWidget *widget)
{
    widget->resize(72, 320);
    widget->show();
    widget->activateWindow();
    QCoreApplication::processEvents();
}

[[nodiscard]] QImage zzRenderWidget(QWidget *widget)
{
    Q_ASSERT(widget != nullptr);
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    return image;
}

[[nodiscard]] int zzCountPixelsNearColor(
    const QImage &image,
    const QRect &rect,
    const QColor &expected)
{
    constexpr int tolerance = 4;
    int count = 0;
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 200
                && qAbs(pixel.red() - expected.red()) <= tolerance
                && qAbs(pixel.green() - expected.green()) <= tolerance
                && qAbs(pixel.blue() - expected.blue()) <= tolerance) {
                ++count;
            }
        }
    }
    return count;
}

/** @brief 承载活动源索引选择的测量结果。 */
struct ZzActiveSelectionMeasurement final
{
    qint64 medianNanoseconds = 0;
    qsizetype checksum = 0;
};

[[nodiscard]] QList<QModelIndex> zzSourceIndexes(
    QAbstractItemModel *model)
{
    QList<QModelIndex> indexes;
    if (model == nullptr) {
        return indexes;
    }
    indexes.reserve(model->rowCount());
    for (int row = 0; row < model->rowCount(); ++row) {
        indexes.append(model->index(row, 0));
    }
    return indexes;
}

[[nodiscard]] ZzActiveSelectionMeasurement zzMeasureActiveSelection(
    ZzFluentUI::ZzActivityBar *bar,
    const QList<QModelIndex> &indexes,
    int repetitions)
{
    ZzActiveSelectionMeasurement measurement;
    if (bar == nullptr || repetitions <= 0) {
        return measurement;
    }
    QList<qint64> samples;
    samples.reserve(7);
    for (int sample = 0; sample < 7; ++sample) {
        QElapsedTimer timer;
        timer.start();
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            bar->setActiveSourceIndexes(indexes);
            measurement.checksum += bar->activeSourceIndexes().size();
        }
        samples.append(timer.nsecsElapsed());
    }
    std::sort(samples.begin(), samples.end());
    measurement.medianNanoseconds = samples.at(samples.size() / 2);
    return measurement;
}

/** @brief 返回指定区域内接近目标颜色的实际像素边界。 */
[[nodiscard]] QRect zzPixelBoundsNearColor(
    const QImage &image,
    const QRect &rect,
    const QColor &expected)
{
    constexpr int tolerance = 4;
    QRect bounds;
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() <= 200
                || qAbs(pixel.red() - expected.red()) > tolerance
                || qAbs(pixel.green() - expected.green()) > tolerance
                || qAbs(pixel.blue() - expected.blue()) > tolerance) {
                continue;
            }
            const QRect pixelRect(x, y, 1, 1);
            bounds = bounds.isNull() ? pixelRect : bounds.united(pixelRect);
        }
    }
    return bounds;
}

[[nodiscard]] QMenu *zzOpenContextMenu(QListView *view, int row)
{
    Q_ASSERT(view != nullptr);
    const QModelIndex index = view->model()->index(row, 0);
    const QPoint localPosition = index.isValid()
        ? view->visualRect(index).center()
        : QPoint(view->viewport()->width() / 2,
                 view->viewport()->height() - 1);
    QContextMenuEvent event(
        QContextMenuEvent::Mouse,
        localPosition,
        view->viewport()->mapToGlobal(localPosition));
    QCoreApplication::sendEvent(view->viewport(), &event);
    QCoreApplication::processEvents();
    return qobject_cast<QMenu *>(QApplication::activePopupWidget());
}

[[nodiscard]] QMenu *zzMoveSubmenu(QMenu *root)
{
    if (root == nullptr) {
        return nullptr;
    }
    const QList<QMenu *> menus = root->findChildren<QMenu *>(
        QString(), Qt::FindDirectChildrenOnly);
    return menus.isEmpty() ? nullptr : menus.constFirst();
}

void zzCloseContextMenu(QMenu *root)
{
    if (root == nullptr) {
        return;
    }
    root->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

} // namespace

/** @brief 验证 Activity Bar 的固定投影、键盘意图和进程内拖放契约。 */
class ZzActivityBarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsBadgeSeparateFromEntryVisual_data()
    {
        QTest::addColumn<ZzFluentUI::ZzIconDescriptor>("descriptor");
        QTest::addColumn<int>("badge");

        QTest::newRow("font-icon-and-single-digit")
            << ZzFluentUI::ZzIconDescriptor::fromFontIcon(
                   ZzFluentUI::ZzFontIcon::House,
                   false,
                   ZzFluentUI::ZzIconColorMode::Custom,
                   QColor(QStringLiteral("#ff00cc")))
            << 7;
        QTest::newRow("svg-icon-and-capped-badge")
            << ZzFluentUI::ZzIconDescriptor::fromBundledSvg(
                   ZzFluentUI::ZzBundledSvgIcon::PinFill,
                   false,
                   ZzFluentUI::ZzIconColorMode::Custom,
                   QColor(QStringLiteral("#ff00cc")))
            << 120;
        QTest::newRow("fallback-and-capped-badge")
            << ZzFluentUI::ZzIconDescriptor{}
            << 120;
    }

    void keepsBadgeSeparateFromEntryVisual()
    {
        QFETCH(ZzFluentUI::ZzIconDescriptor, descriptor);
        QFETCH(int, badge);

        const QColor visualColor(QStringLiteral("#ff00cc"));
        const QColor badgeColor(QStringLiteral("#00ff55"));
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzActivityRowsModel model;
        model.rows[0].badge = badge;
        model.rows[0].icon = descriptor;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const view = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        QPalette palette = view->palette();
        palette.setColor(QPalette::Text, visualColor);
        palette.setColor(QPalette::Highlight, badgeColor);
        view->setPalette(palette);
        view->setStyle(&style);
        view->viewport()->setStyle(&style);
        zzShow(&bar);

        const QRect rowRect = view->visualRect(view->model()->index(0, 0));
        QVERIFY(!rowRect.isEmpty());
        const QImage rendered = zzRenderWidget(view->viewport());
        const QRect visualBounds = zzPixelBoundsNearColor(
            rendered, rowRect, visualColor);
        const QRect badgeBounds = zzPixelBoundsNearColor(
            rendered, rowRect, badgeColor);

        QVERIFY2(!visualBounds.isEmpty(),
                 "Activity Bar 的图标或首字符被 badge 完全覆盖");
        QVERIFY2(!badgeBounds.isEmpty(),
                 "Activity Bar 没有绘制 badge 背板");
        QVERIFY2(!visualBounds.intersects(badgeBounds),
                 "Activity Bar 的 badge 覆盖了图标或首字符区域");
    }

    void rendersFontAndSvgDescriptors_data()
    {
        QTest::addColumn<ZzFluentUI::ZzIconDescriptor>("descriptor");
        QTest::addColumn<QColor>("expectedColor");

        const QColor fontColor(QStringLiteral("#ff00cc"));
        QTest::newRow("font-glyph")
            << ZzFluentUI::ZzIconDescriptor::fromFontIcon(
                   ZzFluentUI::ZzFontIcon::House,
                   false,
                   ZzFluentUI::ZzIconColorMode::Custom,
                   fontColor)
            << fontColor;

        const QColor svgColor(QStringLiteral("#00c8ff"));
        QTest::newRow("bundled-svg")
            << ZzFluentUI::ZzIconDescriptor::fromBundledSvg(
                   ZzFluentUI::ZzBundledSvgIcon::PinFill,
                   false,
                   ZzFluentUI::ZzIconColorMode::Custom,
                   svgColor)
            << svgColor;
    }

    void rendersFontAndSvgDescriptors()
    {
        QFETCH(ZzFluentUI::ZzIconDescriptor, descriptor);
        QFETCH(QColor, expectedColor);

        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzActivityRowsModel model;
        model.rows[0].badge = 0;
        model.rows[0].icon = descriptor;
        QWidget host;
        auto *layout = new QHBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto *bar = new ZzFluentUI::ZzActivityBar(
            ZzFluentUI::ZzSidePaneEdge::Left, &host);
        auto *sibling = new QWidget(&host);
        sibling->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(bar);
        layout->addWidget(sibling, 1);
        bar->setModel(&model);

        QListView *const view = zzActivityView(
            bar, QStringLiteral("zzActivityPrimaryView"));
        view->setStyle(&style);
        view->viewport()->setStyle(&style);
        host.resize(480, 320);
        host.show();
        QCoreApplication::processEvents();

        QCOMPARE(bar->width(), 48);
        const QRect rowRect = view->visualRect(view->model()->index(0, 0));
        QVERIFY(!rowRect.isEmpty());
        QCOMPARE(rowRect.left(), 0);
        QCOMPARE(rowRect.width(), view->viewport()->width());
        const QRect visibleRow(
            0,
            rowRect.top(),
            view->viewport()->width(),
            rowRect.height());
        const QRect iconRect(
            visibleRow.center().x() - 12,
            visibleRow.center().y() - 12,
            24,
            24);
        const QImage rendered = zzRenderWidget(view->viewport());

        QVERIFY2(
            zzCountPixelsNearColor(rendered, iconRect, expectedColor) > 3,
            "Activity Bar 没有通过 Fluent 图标缓存绘制 descriptor");
    }

    void keepsFixedWidthInsideExpandingHorizontalLayout()
    {
        QWidget host;
        auto *layout = new QHBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto *bar = new ZzFluentUI::ZzActivityBar(
            ZzFluentUI::ZzSidePaneEdge::Left, &host);
        auto *sibling = new QWidget(&host);
        sibling->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(bar);
        layout->addWidget(sibling, 1);

        host.resize(480, 320);
        host.show();
        QCoreApplication::processEvents();

        QCOMPARE(bar->minimumWidth(), 48);
        QCOMPARE(bar->maximumWidth(), 48);
        QCOMPARE(bar->width(), 48);
        QCOMPARE(sibling->width(), 432);
    }

    void projectsOnlyTheConfiguredPhysicalSide()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar left(
            ZzFluentUI::ZzSidePaneEdge::Left);
        ZzFluentUI::ZzActivityBar right(
            ZzFluentUI::ZzSidePaneEdge::Right);
        left.setModel(&model);
        right.setModel(&model);

        QCOMPARE(
            zzActivityView(&left, QStringLiteral("zzActivityPrimaryView"))
                ->model()->rowCount(),
            1);
        QCOMPARE(
            zzActivityView(&left, QStringLiteral("zzActivitySecondaryView"))
                ->model()->rowCount(),
            1);
        QCOMPARE(
            zzActivityView(&right, QStringLiteral("zzActivityPrimaryView"))
                ->model()->rowCount(),
            1);
        QCOMPARE(
            zzActivityView(&right, QStringLiteral("zzActivitySecondaryView"))
                ->model()->rowCount(),
            1);
        QCOMPARE(left.findChildren<QListView *>().size(), 2);
    }

    void keepsOrderedValidMultipleActiveSourceIndexes()
    {
        QStandardItemModel model(3, 2);
        QStandardItemModel otherModel(1, 1);
        model.setItem(0, 0, new QStandardItem);
        model.item(0, 0)->appendRow(new QStandardItem);
        const QModelIndex first = model.index(0, 0);
        const QModelIndex second = model.index(1, 0);
        const QModelIndex third = model.index(2, 0);
        const QModelIndex child = model.index(0, 0, first);
        model.setData(
            first,
            QVariant::fromValue(ZzFluentUI::ZzActivityArea::LeftPrimary),
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));
        model.setData(
            second,
            QVariant::fromValue(ZzFluentUI::ZzActivityArea::LeftSecondary),
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));
        model.setData(
            third,
            QVariant::fromValue(ZzFluentUI::ZzActivityArea::LeftPrimary),
            static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));

        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(third);
        bar.setMultiActiveEnabled(true);
        bar.setActiveSourceIndexes(
            {first, second, first, child, model.index(0, 1), otherModel.index(0, 0)});

        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({first, second}));
        QCOMPARE(bar.currentSourceIndex(), third);

        QSignalSpy activeSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activeSourceIndexesChanged);
        model.clear();

        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>());
        QCOMPARE(activeSpy.count(), 1);
    }

    void keepsFirstActiveOccurrenceAndRebuildsAfterReset()
    {
        QStandardItemModel model(6, 2);
        QStandardItemModel otherModel(1, 1);
        for (int row = 0; row < model.rowCount(); ++row) {
            for (int column = 0; column < model.columnCount(); ++column) {
                model.setItem(row, column, new QStandardItem);
            }
        }
        model.item(0, 0)->appendRow(new QStandardItem);
        const auto setAreas = [](QStandardItemModel *target) {
            const QList<ZzFluentUI::ZzActivityArea> areas = {
                ZzFluentUI::ZzActivityArea::LeftPrimary,
                ZzFluentUI::ZzActivityArea::RightPrimary,
                ZzFluentUI::ZzActivityArea::LeftPrimary,
                ZzFluentUI::ZzActivityArea::LeftSecondary,
                ZzFluentUI::ZzActivityArea::LeftPrimary,
                ZzFluentUI::ZzActivityArea::LeftSecondary,
            };
            for (int row = 0; row < areas.size(); ++row) {
                target->setData(
                    target->index(row, 0),
                    QVariant::fromValue(areas.at(row)),
                    static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));
            }
        };
        setAreas(&model);

        ZzFluentUI::ZzActivityBar bar(ZzFluentUI::ZzSidePaneEdge::Left);
        bar.setModel(&model);
        bar.setMultiActiveEnabled(true);
        const QModelIndex row0 = model.index(0, 0);
        const QModelIndex row1 = model.index(1, 0);
        const QModelIndex row2 = model.index(2, 0);
        const QModelIndex row4 = model.index(4, 0);
        const QModelIndex child = model.index(0, 0, row0);
        const QList<QModelIndex> indexes = {
            row4,
            row0,
            row4,
            row1,
            child,
            model.index(0, 1),
            otherModel.index(0, 0),
            row2,
            row0,
        };

        bar.setActiveSourceIndexes(indexes);
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({row4, row0, row2}));

        QSignalSpy activeSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activeSourceIndexesChanged);
        bar.setActiveSourceIndexes(indexes);
        QCOMPARE(activeSpy.count(), 0);

        model.clear();
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>());
        QCOMPARE(activeSpy.count(), 1);

        model.setRowCount(6);
        model.setColumnCount(2);
        for (int row = 0; row < model.rowCount(); ++row) {
            for (int column = 0; column < model.columnCount(); ++column) {
                model.setItem(row, column, new QStandardItem);
            }
        }
        setAreas(&model);
        const QModelIndex newRow0 = model.index(0, 0);
        const QModelIndex newRow2 = model.index(2, 0);
        const QModelIndex newRow3 = model.index(3, 0);
        const QModelIndex newRow4 = model.index(4, 0);
        bar.setActiveSourceIndexes({newRow3, newRow4, newRow3, newRow2, newRow0});

        QCOMPARE(
            zzActivityView(&bar, QStringLiteral("zzActivityPrimaryView"))
                ->model()->rowCount(),
            3);
        QCOMPARE(
            zzActivityView(&bar, QStringLiteral("zzActivitySecondaryView"))
                ->model()->rowCount(),
            2);
        QCOMPARE(
            bar.activeSourceIndexes(),
            QList<QModelIndex>({newRow3, newRow4, newRow2, newRow0}));
    }

    void activeSourceSelectionScalesBelowQuadraticGrowth()
    {
        QStandardItemModel smallModel(128, 1);
        QStandardItemModel largeModel(512, 1);
        for (QStandardItemModel *model : {&smallModel, &largeModel}) {
            for (int row = 0; row < model->rowCount(); ++row) {
                model->setData(
                    model->index(row, 0),
                    QVariant::fromValue(
                        ZzFluentUI::ZzActivityArea::LeftPrimary),
                    static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area));
            }
        }
        ZzFluentUI::ZzActivityBar smallBar(ZzFluentUI::ZzSidePaneEdge::Left);
        ZzFluentUI::ZzActivityBar largeBar(ZzFluentUI::ZzSidePaneEdge::Left);
        smallBar.setModel(&smallModel);
        largeBar.setModel(&largeModel);
        smallBar.setMultiActiveEnabled(true);
        largeBar.setMultiActiveEnabled(true);
        const QList<QModelIndex> smallIndexes = zzSourceIndexes(&smallModel);
        const QList<QModelIndex> largeIndexes = zzSourceIndexes(&largeModel);
        smallBar.setActiveSourceIndexes(smallIndexes);
        largeBar.setActiveSourceIndexes(largeIndexes);

        const ZzActiveSelectionMeasurement small = zzMeasureActiveSelection(
            &smallBar, smallIndexes, 3);
        const ZzActiveSelectionMeasurement large = zzMeasureActiveSelection(
            &largeBar, largeIndexes, 3);

        QVERIFY(small.medianNanoseconds > 0);
        QVERIFY(large.medianNanoseconds > 0);
        QVERIFY(small.checksum > 0);
        QVERIFY(large.checksum > 0);
        QCOMPARE(smallBar.activeSourceIndexes(), smallIndexes);
        QCOMPARE(largeBar.activeSourceIndexes(), largeIndexes);
        const double ratio = static_cast<double>(large.medianNanoseconds)
            / static_cast<double>(small.medianNanoseconds);
        QVERIFY2(
            large.medianNanoseconds < small.medianNanoseconds * 10,
            qPrintable(QStringLiteral("small=%1ns large=%2ns ratio=%3x")
                           .arg(small.medianNanoseconds)
                           .arg(large.medianNanoseconds)
                           .arg(ratio, 0, 'f', 2)));
    }

    void synchronizesSingleActiveIndexWithCurrentSourceIndex()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        const QModelIndex first = model.index(0, 0);
        const QModelIndex second = model.index(1, 0);
        bar.setModel(&model);

        QVERIFY(!bar.isMultiActiveEnabled());
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>());
        bar.setActiveSourceIndexes({first, second});
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>());

        bar.setCurrentSourceIndex(first);
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({first}));
        bar.setActiveSourceIndexes({second, first});
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({first}));

        bar.setCurrentSourceIndex(second);
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({second}));
        bar.setMultiActiveEnabled(true);
        bar.setActiveSourceIndexes({first, second});
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({first, second}));
        bar.setMultiActiveEnabled(false);
        QCOMPARE(bar.activeSourceIndexes(), QList<QModelIndex>({second}));
    }

    void indicatorUsesSingleShortPhysicalEdgeInLtrAndRtl()
    {
        const QColor indicatorColor(QStringLiteral("#00ff55"));
        for (const ZzFluentUI::ZzSidePaneEdge edge : {
                 ZzFluentUI::ZzSidePaneEdge::Left,
                 ZzFluentUI::ZzSidePaneEdge::Right}) {
            for (const Qt::LayoutDirection direction : {
                     Qt::LeftToRight, Qt::RightToLeft}) {
                ZzFluentUI::ZzThemeController controller;
                controller.setAccentColor(indicatorColor);
                ZzFluentUI::ZzFluentStyle style(&controller);
                ZzActivityRowsModel model;
                const int sourceRow =
                    edge == ZzFluentUI::ZzSidePaneEdge::Left ? 0 : 2;
                model.rows[sourceRow].badge = 0;
                ZzFluentUI::ZzActivityBar bar(edge);
                bar.setLayoutDirection(direction);
                bar.setModel(&model);
                bar.setCurrentSourceIndex(model.index(sourceRow, 0));
                QListView *const view = zzActivityView(
                    &bar, QStringLiteral("zzActivityPrimaryView"));
                QPalette palette = view->palette();
                palette.setColor(QPalette::Highlight, indicatorColor);
                view->setPalette(palette);
                view->setStyle(&style);
                view->viewport()->setStyle(&style);
                zzShow(&bar);

                const QRect rowRect =
                    view->visualRect(view->model()->index(0, 0));
                const QImage rendered = zzRenderWidget(view->viewport());
                const QRect indicator = zzPixelBoundsNearColor(
                    rendered, rowRect, indicatorColor);
                const int expectedHeight = qCeil(
                    controller.snapshot()->metric(
                        ZzFluentUI::ZzMetricToken::SelectionIndicatorExtent));
                const int expectedWidth = qCeil(
                    controller.snapshot()->metric(
                        ZzFluentUI::ZzMetricToken::SelectionIndicatorThickness));

                QVERIFY2(!indicator.isEmpty(),
                         "Activity Bar 没有绘制当前入口短指示条");
                QVERIFY2(qAbs(indicator.height() - expectedHeight) <= 2,
                         "Activity Bar 指示条高度没有遵循 Fluent 令牌");
                QVERIFY2(indicator.height() <= expectedHeight + 2,
                         "Activity Bar 仍在绘制全行高的第二条指示");
                QVERIFY2(qAbs(indicator.width() - expectedWidth) <= 2,
                         "Activity Bar 指示条厚度没有遵循 Fluent 令牌");
                QVERIFY2(indicator.width() <= expectedWidth + 2,
                         "Activity Bar 指示条厚度超出 Fluent 令牌");
                if (edge == ZzFluentUI::ZzSidePaneEdge::Left) {
                    QVERIFY2(indicator.left() <= rowRect.left() + 6,
                             "左 Activity Bar 指示条没有贴物理左边");
                    QVERIFY(indicator.right() < rowRect.center().x());
                } else {
                    QVERIFY2(indicator.right() >= rowRect.right() - 6,
                             "右 Activity Bar 指示条没有贴物理右边");
                    QVERIFY(indicator.left() > rowRect.center().x());
                }
            }
        }
    }

    void enabledNonSelectableRowOnlyRequestsActivation()
    {
        ZzActivityRowsModel model;
        model.rows[1].enabled = true;
        model.rows[1].draggable = false;
        model.rows[1].selectable = false;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *const secondary = zzActivityView(
            &bar, QStringLiteral("zzActivitySecondaryView"));
        zzShow(&bar);

        QTest::mouseClick(
            secondary->viewport(), Qt::LeftButton, Qt::NoModifier,
            secondary->visualRect(secondary->model()->index(0, 0)).center());

        QTRY_COMPARE(activationSpy.count(), 1);
        QCOMPARE(collapseSpy.count(), 0);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
        QCOMPARE(
            activationSpy.constFirst().at(0).value<QModelIndex>(),
            model.index(1, 0));
    }

    void selectableActivationStopsWhenCurrentSignalDestroysBar()
    {
        ZzActivityRowsModel model;
        model.rows[1].area = ZzFluentUI::ZzActivityArea::LeftPrimary;
        model.rows[1].enabled = true;
        auto *bar = new ZzFluentUI::ZzActivityBar;
        QPointer<ZzFluentUI::ZzActivityBar> barGuard(bar);
        bar->setModel(&model);
        bar->setCurrentSourceIndex(model.index(0, 0));
        int activationCount = 0;
        QObject::connect(
            bar,
            &ZzFluentUI::ZzActivityBar::activationRequested,
            qApp,
            [&activationCount] { ++activationCount; });
        QObject::connect(
            bar,
            &ZzFluentUI::ZzActivityBar::currentSourceIndexChanged,
            qApp,
            [bar](const QModelIndex &) { delete bar; });
        QListView *const primary = zzActivityView(
            bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(bar);

        QTest::mouseClick(
            primary->viewport(), Qt::LeftButton, Qt::NoModifier,
            primary->visualRect(primary->model()->index(1, 0)).center());

        QTRY_VERIFY(barGuard.isNull());
        QCOMPARE(activationCount, 0);
    }

    void selectableActivationStopsWhenCurrentSignalDestroysModel()
    {
        auto model = std::make_unique<ZzActivityRowsModel>();
        model->rows[1].area = ZzFluentUI::ZzActivityArea::LeftPrimary;
        model->rows[1].enabled = true;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(model.get());
        bar.setCurrentSourceIndex(model->index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QObject::connect(
            &bar,
            &ZzFluentUI::ZzActivityBar::currentSourceIndexChanged,
            &bar,
            [&model](const QModelIndex &) { model.reset(); });
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QTest::mouseClick(
            primary->viewport(), Qt::LeftButton, Qt::NoModifier,
            primary->visualRect(primary->model()->index(1, 0)).center());

        QTRY_VERIFY(model == nullptr);
        QCOMPARE(bar.model(), nullptr);
        QCOMPARE(activationSpy.count(), 0);
    }

    void nonSelectableActivationMayDestroyBarAfterMouseRelease()
    {
        ZzActivityRowsModel model;
        model.rows[1].enabled = true;
        model.rows[1].draggable = false;
        model.rows[1].selectable = false;
        auto *bar = new ZzFluentUI::ZzActivityBar;
        QPointer<ZzFluentUI::ZzActivityBar> barGuard(bar);
        bar->setModel(&model);
        bar->setCurrentSourceIndex(model.index(0, 0));
        QObject::connect(
            bar,
            &ZzFluentUI::ZzActivityBar::activationRequested,
            qApp,
            [bar](const QModelIndex &) { delete bar; });
        QListView *const secondary = zzActivityView(
            bar, QStringLiteral("zzActivitySecondaryView"));
        zzShow(bar);

        QTest::mouseClick(
            secondary->viewport(), Qt::LeftButton, Qt::NoModifier,
            secondary->visualRect(secondary->model()->index(0, 0)).center());

        QTRY_VERIFY(barGuard.isNull());
    }

    void collapseActivationMayDestroyBarAfterMouseRelease()
    {
        ZzActivityRowsModel model;
        auto *bar = new ZzFluentUI::ZzActivityBar;
        QPointer<ZzFluentUI::ZzActivityBar> barGuard(bar);
        bar->setModel(&model);
        bar->setCurrentSourceIndex(model.index(0, 0));
        QObject::connect(
            bar,
            &ZzFluentUI::ZzActivityBar::collapseRequested,
            qApp,
            [bar](const QModelIndex &) { delete bar; });
        QListView *const primary = zzActivityView(
            bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(bar);

        QTest::mouseClick(
            primary->viewport(), Qt::LeftButton, Qt::NoModifier,
            primary->visualRect(primary->model()->index(0, 0)).center());

        QTRY_VERIFY(barGuard.isNull());
    }

    void queuedMouseActivationRejectsChangedRowState()
    {
        ZzActivityRowsModel model;
        model.rows[1].area = ZzFluentUI::ZzActivityArea::LeftPrimary;
        model.rows[1].enabled = true;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QTest::mouseClick(
            primary->viewport(), Qt::LeftButton, Qt::NoModifier,
            primary->visualRect(primary->model()->index(1, 0)).center());
        model.rows[1].enabled = false;
        Q_EMIT model.dataChanged(
            model.index(1, 0), model.index(1, 0), {Qt::DisplayRole});
        QCoreApplication::processEvents();

        QCOMPARE(activationSpy.count(), 0);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
    }

    void queuedMouseActivationRejectsChangedCurrentState()
    {
        ZzActivityRowsModel model;
        model.rows[1].area = ZzFluentUI::ZzActivityArea::LeftPrimary;
        model.rows[1].enabled = true;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QTest::mouseClick(
            primary->viewport(), Qt::LeftButton, Qt::NoModifier,
            primary->visualRect(primary->model()->index(1, 0)).center());
        bar.setCurrentSourceIndex(model.index(1, 0));
        QCoreApplication::processEvents();

        QCOMPARE(activationSpy.count(), 0);
        QCOMPARE(collapseSpy.count(), 0);
        QCOMPARE(bar.currentSourceIndex(), model.index(1, 0));
    }

    void keyboardActivatesEnabledNonSelectableRowWithoutSelection()
    {
        ZzActivityRowsModel model;
        model.rows[1].enabled = true;
        model.rows[1].draggable = false;
        model.rows[1].selectable = false;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        primary->setFocus();

        QTest::keyClick(primary, Qt::Key_End);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
        QTest::keyClick(
            zzActivityView(&bar, QStringLiteral("zzActivitySecondaryView")),
            Qt::Key_Enter);

        QCOMPARE(activationSpy.count(), 1);
        QCOMPARE(collapseSpy.count(), 0);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
        QCOMPARE(
            activationSpy.constFirst().at(0).value<QModelIndex>(),
            model.index(1, 0));
    }

    void keyboardDoesNotFallBackWhenFocusedRowBecomesDisabled()
    {
        ZzActivityRowsModel model;
        model.rows[1].enabled = true;
        model.rows[1].draggable = false;
        model.rows[1].selectable = false;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        QListView *const secondary = zzActivityView(
            &bar, QStringLiteral("zzActivitySecondaryView"));
        zzShow(&bar);
        primary->setFocus();

        QTest::keyClick(primary, Qt::Key_End);
        model.rows[1].enabled = false;
        Q_EMIT model.dataChanged(
            model.index(1, 0), model.index(1, 0), {Qt::DisplayRole});
        QTest::keyClick(secondary, Qt::Key_Enter);

        QCOMPARE(activationSpy.count(), 0);
        QCOMPARE(collapseSpy.count(), 0);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
    }

    void contextMenuListsOnlyThreeOtherAreas()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QMenu *const root = zzOpenContextMenu(primary, 0);
        QVERIFY(root != nullptr);
        QMenu *const moveMenu = zzMoveSubmenu(root);
        QVERIFY(moveMenu != nullptr);
        QList<ZzFluentUI::ZzActivityArea> actual;
        for (QAction *action : moveMenu->actions()) {
            QVERIFY(action->data().canConvert<ZzFluentUI::ZzActivityArea>());
            actual.append(
                action->data().value<ZzFluentUI::ZzActivityArea>());
        }
        const QList<ZzFluentUI::ZzActivityArea> expected = {
            ZzFluentUI::ZzActivityArea::LeftSecondary,
            ZzFluentUI::ZzActivityArea::RightPrimary,
            ZzFluentUI::ZzActivityArea::RightSecondary,
        };
        QCOMPARE(actual, expected);
        zzCloseContextMenu(root);
    }

    void keyboardContextMenuTargetsFocusedRow()
    {
        ZzActivityRowsModel model;
        model.rows[1].area = ZzFluentUI::ZzActivityArea::LeftPrimary;
        model.rows[1].enabled = true;
        model.rows[1].draggable = true;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        primary->setCurrentIndex(primary->model()->index(1, 0));
        primary->setFocus();
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);
        const QPoint eventPosition = primary->visualRect(
            primary->model()->index(0, 0)).center();

        QContextMenuEvent event(
            QContextMenuEvent::Keyboard,
            eventPosition,
            primary->viewport()->mapToGlobal(eventPosition));
        QCoreApplication::sendEvent(primary->viewport(), &event);
        QCoreApplication::processEvents();

        QMenu *const root = qobject_cast<QMenu *>(
            QApplication::activePopupWidget());
        QVERIFY(root != nullptr);
        QMenu *const moveMenu = zzMoveSubmenu(root);
        QVERIFY(moveMenu != nullptr);
        QCOMPARE(moveMenu->actions().size(), 3);
        moveMenu->actions().constFirst()->trigger();
        QCOMPARE(moveSpy.count(), 1);
        QCOMPARE(
            moveSpy.constFirst().at(0).value<QModelIndex>(),
            model.index(1, 0));
        zzCloseContextMenu(root);
    }

    void keyboardContextMenuRejectsMissingFocusedRow()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        primary->setCurrentIndex({});
        primary->setFocus();
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);
        const QPoint eventPosition = primary->visualRect(
            primary->model()->index(0, 0)).center();

        QContextMenuEvent event(
            QContextMenuEvent::Keyboard,
            eventPosition,
            primary->viewport()->mapToGlobal(eventPosition));
        QCoreApplication::sendEvent(primary->viewport(), &event);
        QCoreApplication::processEvents();

        QVERIFY(QApplication::activePopupWidget() == nullptr);
        QCOMPARE(moveSpy.count(), 0);
    }

    void contextMenuMoveMatchesDragMoveArguments()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        QListView *const secondary = zzActivityView(
            &bar, QStringLiteral("zzActivitySecondaryView"));
        zzShow(&bar);
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);

        QMenu *const root = zzOpenContextMenu(primary, 0);
        QVERIFY(root != nullptr);
        QMenu *const moveMenu = zzMoveSubmenu(root);
        QVERIFY(moveMenu != nullptr);
        QAction *menuAction = nullptr;
        for (QAction *action : moveMenu->actions()) {
            if (action->data().value<ZzFluentUI::ZzActivityArea>()
                == ZzFluentUI::ZzActivityArea::LeftSecondary) {
                menuAction = action;
                break;
            }
        }
        QVERIFY(menuAction != nullptr);
        menuAction->trigger();
        QCOMPARE(moveSpy.count(), 1);
        const QList<QVariant> menuArguments = moveSpy.constFirst();
        zzCloseContextMenu(root);
        moveSpy.clear();

        const QModelIndex projected = primary->model()->index(0, 0);
        std::unique_ptr<QMimeData> mime(primary->model()->mimeData({projected}));
        QVERIFY(mime != nullptr);
        QDragEnterEvent enter(
            secondary->viewport()->rect().center(), Qt::MoveAction, mime.get(),
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(secondary, &enter);
        QDropEvent drop(
            QPointF(0, secondary->viewport()->height() - 1),
            Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(secondary, &drop);

        QCOMPARE(moveSpy.count(), 1);
        QCOMPARE(moveSpy.constFirst(), menuArguments);
    }

    void contextMenuRejectsFixedDisabledAndInvalidRows()
    {
        ZzActivityRowsModel model;
        model.rows[1].enabled = true;
        model.rows[1].draggable = false;
        model.rows[1].selectable = false;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        QListView *const secondary = zzActivityView(
            &bar, QStringLiteral("zzActivitySecondaryView"));
        zzShow(&bar);

        QVERIFY(zzOpenContextMenu(secondary, 0) == nullptr);
        model.rows[0].enabled = false;
        Q_EMIT model.dataChanged(
            model.index(0, 0), model.index(0, 0), {Qt::DisplayRole});
        QVERIFY(zzOpenContextMenu(primary, 0) == nullptr);
        QVERIFY(zzOpenContextMenu(primary, -1) == nullptr);
    }

    void contextMenuDoesNotIncreaseSteadyObjectBudget()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        const qsizetype objectCount = bar.findChildren<QObject *>().size();
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);

        for (int iteration = 0; iteration < 8; ++iteration) {
            QPointer<QMenu> menu = zzOpenContextMenu(primary, 0);
            QVERIFY(menu != nullptr);
            zzCloseContextMenu(menu);
            QTRY_VERIFY(menu.isNull());
            QCOMPARE(bar.findChildren<QObject *>().size(), objectCount);
            QCOMPARE(moveSpy.count(), 0);
        }
    }

    void contextMenuCancelsMoveWhenSourceModelIsDestroyed()
    {
        auto model = std::make_unique<ZzActivityRowsModel>();
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(model.get());
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);

        QPointer<QMenu> root = zzOpenContextMenu(primary, 0);
        QVERIFY(root != nullptr);
        QMenu *const moveMenu = zzMoveSubmenu(root);
        QVERIFY(moveMenu != nullptr);
        QAction *const targetAction = moveMenu->actions().constFirst();
        QVERIFY(targetAction != nullptr);

        model.reset();
        targetAction->trigger();

        QCOMPARE(moveSpy.count(), 0);
        zzCloseContextMenu(root);
        QTRY_VERIFY(root.isNull());
    }

    void contextMenuCancelsMoveWhenSourceAreaChanges()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *const primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);

        QPointer<QMenu> root = zzOpenContextMenu(primary, 0);
        QVERIFY(root != nullptr);
        QMenu *const moveMenu = zzMoveSubmenu(root);
        QVERIFY(moveMenu != nullptr);
        QAction *targetAction = nullptr;
        for (QAction *action : moveMenu->actions()) {
            if (action->data().value<ZzFluentUI::ZzActivityArea>()
                == ZzFluentUI::ZzActivityArea::LeftSecondary) {
                targetAction = action;
                break;
            }
        }
        QVERIFY(targetAction != nullptr);

        model.rows[0].area = ZzFluentUI::ZzActivityArea::RightPrimary;
        Q_EMIT model.dataChanged(
            model.index(0, 0), model.index(0, 0),
            {static_cast<int>(ZzFluentUI::ZzActivityItemRole::Area)});
        targetAction->trigger();

        QCOMPARE(moveSpy.count(), 0);
        zzCloseContextMenu(root);
        QTRY_VERIFY(root.isNull());
    }

    void activatesOtherRowsAndCollapsesTheCurrentRow()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *view = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QTest::mouseClick(
            view->viewport(), Qt::LeftButton, Qt::NoModifier,
            view->visualRect(view->model()->index(0, 0)).center());
        QTRY_COMPARE(collapseSpy.count(), 1);
        QCOMPARE(activationSpy.count(), 0);

        bar.setCurrentSourceIndex(model.index(2, 0));
        QTest::mouseClick(
            view->viewport(), Qt::LeftButton, Qt::NoModifier,
            view->visualRect(view->model()->index(0, 0)).center());
        QTRY_COMPARE(activationSpy.count(), 1);
        QCOMPARE(
            activationSpy.first().at(0).value<QModelIndex>(), model.index(0, 0));
    }

    void keyboardTraversesBothGroupsAndSkipsDisabledRows()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy activationSpy(
            &bar, &ZzFluentUI::ZzActivityBar::activationRequested);
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        primary->setFocus();

        QTest::keyClick(primary, Qt::Key_End);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
        QTest::keyClick(primary, Qt::Key_Home);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
        QTest::keyClick(primary, Qt::Key_Down);
        QCOMPARE(bar.currentSourceIndex(), model.index(0, 0));
        QTest::keyClick(primary, Qt::Key_Enter);
        QCOMPARE(activationSpy.count(), 0);
        QCOMPARE(collapseSpy.count(), 1);
        collapseSpy.clear();

        model.rows[1].enabled = true;
        Q_EMIT model.dataChanged(
            model.index(1, 0), model.index(1, 0), {Qt::DisplayRole});
        QTest::keyClick(primary, Qt::Key_Down);
        QCOMPARE(bar.currentSourceIndex(), model.index(1, 0));
        QTest::keyClick(
            zzActivityView(&bar, QStringLiteral("zzActivitySecondaryView")),
            Qt::Key_Space);
        QCOMPARE(collapseSpy.count(), 1);
    }

    void rejectsForgedMimePayloadAndOnlyEmitsMoveIntent()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);
        QListView *view = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QMimeData fakeData;
        fakeData.setData(
            QStringLiteral("application/x-zzfluentui-activity-move"),
            QByteArrayLiteral("forged"));
        QDropEvent event(
            view->viewport()->rect().center(), Qt::MoveAction, &fakeData,
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view->viewport(), &event);

        QCOMPARE(moveSpy.count(), 0);
        QCOMPARE(model.rows.size(), 4);
    }

    void handlesKeyboardWhenTheViewportOwnsFocus()
    {
        ZzActivityRowsModel model;
        model.rows[1].area = ZzFluentUI::ZzActivityArea::LeftPrimary;
        model.rows[1].enabled = true;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        bar.setCurrentSourceIndex(model.index(0, 0));
        QSignalSpy collapseSpy(
            &bar, &ZzFluentUI::ZzActivityBar::collapseRequested);
        QListView *primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);
        primary->viewport()->setFocus();
        QVERIFY(primary->viewport()->hasFocus());

        QTest::keyClick(primary->viewport(), Qt::Key_Down);
        QCOMPARE(bar.currentSourceIndex(), model.index(1, 0));
        QTest::keyClick(primary->viewport(), Qt::Key_Enter);
        QCOMPARE(collapseSpy.count(), 1);
        QCOMPARE(
            collapseSpy.first().at(0).value<QModelIndex>(), model.index(1, 0));
    }

    void keepsDragTokenWhenMovingFromPrimaryToSecondary()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *primary = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        QListView *secondary = zzActivityView(
            &bar, QStringLiteral("zzActivitySecondaryView"));
        zzShow(&bar);

        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);
        const QModelIndex projected = primary->model()->index(0, 0);
        std::unique_ptr<QMimeData> mime(primary->model()->mimeData({projected}));
        QVERIFY(mime != nullptr);
        QDragEnterEvent enter(
            primary->viewport()->rect().center(), Qt::MoveAction, mime.get(),
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(primary, &enter);
        QDragLeaveEvent leave;
        QCoreApplication::sendEvent(primary, &leave);
        QCoreApplication::processEvents();
        QDragEnterEvent targetEnter(
            secondary->viewport()->rect().center(), Qt::MoveAction, mime.get(),
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(secondary, &targetEnter);
        auto *drop = new QDropEvent(
            secondary->viewport()->rect().center(), Qt::MoveAction, mime.get(),
            Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::postEvent(secondary, drop);
        QCoreApplication::processEvents();

        QCOMPARE(moveSpy.count(), 1);
        QCOMPARE(
            moveSpy.first().at(1).value<ZzFluentUI::ZzActivityArea>(),
            ZzFluentUI::ZzActivityArea::LeftSecondary);
    }

    void acceptsOnlyComponentIssuedMimeAndKeepsFixedObjectBudget()
    {
        ZzActivityRowsModel model;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&model);
        QListView *view = zzActivityView(
            &bar, QStringLiteral("zzActivityPrimaryView"));
        zzShow(&bar);

        QSignalSpy moveSpy(&bar, &ZzFluentUI::ZzActivityBar::moveRequested);
        const int widgetCount = static_cast<int>(
            bar.findChildren<QWidget *>().size());
        const QModelIndex projected = view->model()->index(0, 0);
        std::unique_ptr<QMimeData> mime(view->model()->mimeData({projected}));
        QVERIFY(mime != nullptr);
        QDropEvent drop(
            view->viewport()->rect().center(),
            Qt::MoveAction,
            mime.get(),
            Qt::LeftButton,
            Qt::NoModifier);
        QDragEnterEvent enter(
            view->viewport()->rect().center(),
            Qt::MoveAction,
            mime.get(),
            Qt::LeftButton,
            Qt::NoModifier);
        QCoreApplication::sendEvent(view, &enter);
        QCoreApplication::sendEvent(view, &drop);

        QCOMPARE(moveSpy.count(), 1);
        QCOMPARE(moveSpy.first().at(1).value<ZzFluentUI::ZzActivityArea>(),
                 ZzFluentUI::ZzActivityArea::LeftPrimary);
        QCOMPARE(bar.findChildren<QWidget *>().size(), widgetCount);

        for (int iteration = 0; iteration < 100; ++iteration) {
            bar.setCurrentSourceIndex(model.index(0, 0));
            bar.setCurrentSourceIndex(model.index(2, 0));
        }
        QCOMPARE(bar.findChildren<QWidget *>().size(), widgetCount);
    }

    void clearsSourceIndexesWhenTheObservedModelIsDestroyed()
    {
        ZzFluentUI::ZzActivityBar bar;
        auto model = std::make_unique<ZzActivityRowsModel>();
        bar.setModel(model.get());
        bar.setCurrentSourceIndex(model->index(0, 0));
        QVERIFY(bar.currentSourceIndex().isValid());

        model.reset();

        QCOMPARE(bar.model(), nullptr);
        QVERIFY(!bar.currentSourceIndex().isValid());
        QCOMPARE(
            zzActivityView(&bar, QStringLiteral("zzActivityPrimaryView"))
                ->model()->rowCount(),
            0);
    }

    void notifiesWhenChangingOrDestroyingTheCurrentModelClearsSelection()
    {
        ZzActivityRowsModel first;
        ZzActivityRowsModel second;
        ZzFluentUI::ZzActivityBar bar;
        bar.setModel(&first);
        bar.setCurrentSourceIndex(first.index(0, 0));
        QSignalSpy currentSpy(
            &bar, &ZzFluentUI::ZzActivityBar::currentSourceIndexChanged);

        bar.setModel(&second);

        QCOMPARE(currentSpy.count(), 1);
        QVERIFY(!currentSpy.at(0).at(0).value<QModelIndex>().isValid());
        bar.setCurrentSourceIndex(second.index(0, 0));
        currentSpy.clear();
        auto ownedModel = std::make_unique<ZzActivityRowsModel>();
        bar.setModel(ownedModel.get());
        bar.setCurrentSourceIndex(ownedModel->index(0, 0));
        currentSpy.clear();

        ownedModel.reset();

        QCOMPARE(currentSpy.count(), 1);
        QVERIFY(!currentSpy.at(0).at(0).value<QModelIndex>().isValid());
    }
};

QTEST_MAIN(ZzActivityBarTest)

#include "ZzActivityBarTest.moc"
