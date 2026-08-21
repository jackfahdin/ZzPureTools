#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtGui/QDropEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListView>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzThemeController.h>

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
            return row.text;
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
        Qt::ItemFlags result = Qt::ItemIsSelectable;
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

} // namespace

/** @brief 验证 Activity Bar 的固定投影、键盘意图和进程内拖放契约。 */
class ZzActivityBarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
        QCOMPARE(collapseSpy.count(), 1);
        QCOMPARE(activationSpy.count(), 0);

        bar.setCurrentSourceIndex(model.index(2, 0));
        QTest::mouseClick(
            view->viewport(), Qt::LeftButton, Qt::NoModifier,
            view->visualRect(view->model()->index(0, 0)).center());
        QCOMPARE(activationSpy.count(), 1);
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
        QTest::keyClick(primary, Qt::Key_Space);
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
