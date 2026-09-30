#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtTest/QTest>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QScrollBar>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

class ZzDataViewsTest final : public QObject
{
    Q_OBJECT
private slots:
    void tableSelectionIsContinuous_data()
    {
        QTest::addColumn<bool>("fluent");
        QTest::addColumn<bool>("rtl");
        for (bool fluent : {false, true}) {
            for (bool rtl : {false, true}) {
                QTest::newRow(qPrintable(QStringLiteral("delegate-%1-rtl-%2").arg(fluent).arg(rtl)))
                    << fluent << rtl;
            }
        }
    }

    void tableSelectionIsContinuous()
    {
        QFETCH(bool, fluent);
        QFETCH(bool, rtl);
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTableView view;
        view.setStyle(&style);
        view.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        view.setShowGrid(false);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        QStandardItemModel model(3, 4);
        view.setModel(&model);
        if (fluent) {
            view.setItemDelegate(new ZzFluentUI::ZzFluentItemDelegate(&view));
        }
        view.setColumnHidden(1, true);
        view.horizontalHeader()->moveSection(3, 0);
        view.horizontalHeader()->setDefaultSectionSize(80);
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.selectionModel()->select(model.index(1, 0),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const QImage image = view.viewport()->grab().toImage();
        const QRect first = view.visualRect(model.index(1, 3));
        const QRect next = view.visualRect(model.index(1, 0));
        const int boundary = rtl ? first.left() : next.left();
        const QColor fill = image.pixelColor(next.center());
        QVERIFY(fill != image.pixelColor(view.visualRect(model.index(2, 0)).center()));
        for (int x = boundary - 2; x <= boundary + 2; ++x) {
            QCOMPARE(image.pixelColor(x, next.center().y()), fill);
        }
        // 行边框不能在中间单元格重新画出竖线，且必须跨越列边界。
        QCOMPARE(image.pixelColor(boundary, next.top() + 1),
            image.pixelColor(next.center().x(), next.top() + 1));
        QVERIFY(image.pixelColor(boundary, next.top() + 1) != fill);
    }

    void undecoratedTreeKeepsSelectionBackground()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTreeView view;
        view.setStyle(&style);
        view.setRootIsDecorated(false);
        view.setHeaderHidden(true);
        QStandardItemModel model(3, 1);
        view.setModel(&model);
        view.resize(240, 160);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setCurrentIndex(model.index(1, 0));
        const QImage image = view.viewport()->grab().toImage();
        QVERIFY(image.pixelColor(view.visualRect(model.index(1, 0)).center())
            != image.pixelColor(view.visualRect(model.index(2, 0)).center()));
    }

    void listBackgroundRoleCannotCoverSelection()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QListView view;
        view.setStyle(&style);
        QStandardItemModel model(2, 1);
        model.setData(model.index(0, 0), QColor(Qt::red), Qt::BackgroundRole);
        model.setData(model.index(1, 0), QColor(Qt::red), Qt::BackgroundRole);
        view.setModel(&model);
        view.resize(240, 160);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setCurrentIndex(model.index(0, 0));
        const QImage image = view.viewport()->grab().toImage();
        QVERIFY(image.pixelColor(view.visualRect(model.index(0, 0)).center()) != QColor(Qt::red));
        QCOMPARE(image.pixelColor(view.visualRect(model.index(1, 0)).center()), QColor(Qt::red));
    }

    void tableHoverFollowsWholeRowAndClearsOnLeave()
    {
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTableView view;
        view.setStyle(&style);
        view.setShowGrid(false);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        QStandardItemModel model(3, 3);
        view.setModel(&model);
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const auto sample = [&view, &model](int row, int column) {
            return view.viewport()->grab().toImage().pixelColor(view.visualRect(model.index(row, column)).center());
        };
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(view.viewport(), &leave);
        const QColor base = sample(1, 0);
        QTest::mouseMove(view.viewport(), view.visualRect(model.index(1, 1)).center());
        const QColor hover = sample(1, 0);
        QVERIFY(hover != base);
        QCOMPARE(sample(1, 2), hover);
        QCOMPARE(sample(2, 0), base);
        QApplication::sendEvent(view.viewport(), &leave);
        QCOMPARE(sample(1, 0), base);
        QCOMPARE(sample(1, 2), base);
    }

    void tableEditorKeepsFillWithoutSelectionOutline()
    {
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTableView view;
        view.setStyle(&style);
        view.setShowGrid(false);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        QStandardItemModel model(3, 3);
        view.setModel(&model);
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setCurrentIndex(model.index(1, 0));
        view.selectRow(1);
        const QRect cell = view.visualRect(model.index(1, 2));
        const QImage before = view.viewport()->grab().toImage();
        const QPoint edge(cell.center().x(), cell.top() + 1);
        QVERIFY(before.pixelColor(edge) != before.pixelColor(cell.center()));
        view.edit(model.index(1, 0));
        auto *editor = view.findChild<QLineEdit *>();
        QVERIFY(editor != nullptr);
        editor->setFocus();
        const QImage after = view.viewport()->grab().toImage();
        QCOMPARE(after.pixelColor(edge), after.pixelColor(cell.center()));
        QCOMPARE(after.pixelColor(cell.center()), before.pixelColor(cell.center()));
    }

    void tableHoverStaysUnderPointerAfterScroll()
    {
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTableView view;
        view.setStyle(&style);
        view.setShowGrid(false);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        view.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        QStandardItemModel model(30, 3);
        view.setModel(&model);
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const QPoint pointer = view.visualRect(model.index(1, 1)).center();
        QTest::mouseMove(view.viewport(), pointer);
        const QColor hover = view.viewport()->grab().toImage().pixelColor(pointer);
        view.verticalScrollBar()->setValue(64);
        QCoreApplication::processEvents();
        const QModelIndex hovered = view.indexAt(pointer);
        QVERIFY(hovered.row() > 1);
        const QImage image = view.viewport()->grab().toImage();
        QCOMPARE(image.pixelColor(pointer), hover);
        const QPoint adjacent(view.visualRect(hovered.siblingAtColumn(2)).center());
        QCOMPARE(image.pixelColor(adjacent), hover);
    }

    void highContrastHoverUsesReadableTextAcrossRow()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTableView view;
        view.setStyle(&style);
        view.setShowGrid(false);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        QPalette palette = style.standardPalette();
        palette.setColor(QPalette::Highlight, Qt::yellow);
        palette.setColor(QPalette::HighlightedText, Qt::black);
        palette.setColor(QPalette::Text, Qt::white);
        view.setPalette(palette);
        QStandardItemModel model(3, 3);
        model.setData(model.index(1, 2), QStringLiteral("Readable"));
        view.setModel(&model);
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTest::mouseMove(view.viewport(), view.visualRect(model.index(1, 0)).center());
        const QImage image = view.viewport()->grab().toImage();
        const QRect cell = view.visualRect(model.index(1, 2)).adjusted(4, 4, -4, -4);
        int blackPixels = 0;
        int whitePixels = 0;
        for (int y = cell.top(); y <= cell.bottom(); ++y) {
            for (int x = cell.left(); x <= cell.right(); ++x) {
                blackPixels += image.pixelColor(x, y) == QColor(Qt::black) ? 1 : 0;
                whitePixels += image.pixelColor(x, y) == QColor(Qt::white) ? 1 : 0;
            }
        }
        QVERIFY(blackPixels > 0);
        QCOMPARE(whitePixels, 0);
    }

    void narrowTreeColumnKeepsDeepSelectionVisible()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTreeView tree;
        tree.setStyle(&style);
        tree.setIndentation(30);
        QStandardItemModel model(0, 2);
        auto *parent = new QStandardItem(QStringLiteral("Parent"));
        parent->appendRow({new QStandardItem(QStringLiteral("Child")), new QStandardItem});
        model.appendRow({parent, new QStandardItem});
        tree.setModel(&model);
        tree.setColumnWidth(0, 50);
        tree.resize(260, 180);
        tree.expandAll();
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        const QModelIndex child = model.index(0, 0, model.index(0, 0));
        tree.selectionModel()->select(child, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const QRect row = tree.visualRect(child.siblingAtColumn(1));
        const QImage image = tree.viewport()->grab().toImage();
        QCOMPARE(image.pixelColor(15, row.center().y()), image.pixelColor(100, row.center().y()));
        QVERIFY(image.pixelColor(15, row.center().y()) != tree.palette().color(QPalette::Base));
    }

    void checkIndicatorHitAreaMatchesPainting_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::newRow("ltr") << false;
        QTest::newRow("rtl") << true;
    }

    void highContrastBranchUsesHighlightedText()
    {
        ZzFluentUI::ZzThemeController theme;
        theme.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&theme);
        QTreeView tree;
        tree.setStyle(&style);
        QStyleOption option;
        option.rect = QRect(0, 0, 30, 32);
        option.palette = style.standardPalette();
        option.palette.setColor(QPalette::Text, Qt::white);
        option.palette.setColor(QPalette::HighlightedText, Qt::black);
        for (bool open : {false, true}) {
            option.state = QStyle::State_Enabled | QStyle::State_Children | QStyle::State_Selected;
            option.state.setFlag(QStyle::State_Open, open);
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::yellow);
            QPainter painter(&image);
            style.drawPrimitive(QStyle::PE_IndicatorBranch, &option, &painter, &tree);
            painter.end();
            bool black = false;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    black = black || image.pixelColor(x, y) == QColor(Qt::black);
                    QVERIFY(image.pixelColor(x, y) != QColor(Qt::white));
                }
            }
            QVERIFY(black);
        }
    }

    void checkIndicatorHitAreaMatchesPainting()
    {
        QFETCH(bool, rtl);
        ZzFluentUI::ZzThemeController theme;
        ZzFluentUI::ZzFluentStyle style(&theme);
        QListView view;
        view.setStyle(&style);
        view.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        QStandardItemModel model;
        auto *item = new QStandardItem(QStringLiteral("Toggle this item"));
        item->setCheckable(true);
        model.appendRow(item);
        view.setModel(&model);
        view.resize(260, 160);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QStyleOptionViewItem option;
        option.initFrom(&view);
        option.widget = &view;
        option.rect = view.visualRect(model.index(0, 0));
        option.features = QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDisplay;
        option.text = item->text();
        const QRect check = style.subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, &view);
        QVERIFY(option.rect.contains(check));
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, check.center());
        QCOMPARE(item->checkState(), Qt::Checked);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, check.center());
        QCOMPARE(item->checkState(), Qt::Unchecked);
    }
};

QTEST_MAIN(ZzDataViewsTest)
#include "ZzDataViewsTest.moc"
