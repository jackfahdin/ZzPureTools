#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtGui/QWheelEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzCarouselView.h>

namespace {

/** @brief 向测试 model 追加带标准展示和无障碍数据的一行。 */
void zzAppendCarouselRow(QStandardItemModel *model, const QString &title,
                         const QVariant &decoration = {}) {
  Q_ASSERT(model != nullptr);
  auto *item = new QStandardItem(title);
  item->setData(QStringLiteral("%1 description").arg(title),
                ZzFluentUI::ZzCarouselView::DescriptionRole);
  item->setData(QStringLiteral("Accessible %1").arg(title),
                Qt::AccessibleTextRole);
  if (decoration.isValid()) {
    item->setData(decoration, Qt::DecorationRole);
  }
  model->appendRow(item);
}

/** @brief 创建指定行数且不带逐项 QObject 以外资源的测试 model。 */
void zzPopulateCarouselModel(QStandardItemModel *model, int count) {
  Q_ASSERT(model != nullptr);
  for (int row = 0; row < count; ++row) {
    zzAppendCarouselRow(model, QStringLiteral("Item %1").arg(row));
  }
}

/** @brief 按无障碍名称定位内部固定箭头按钮。 */
QToolButton *zzCarouselButton(ZzFluentUI::ZzCarouselView *view,
                              const QString &accessibleName) {
  Q_ASSERT(view != nullptr);
  const QList<QToolButton *> buttons = view->findChildren<QToolButton *>();
  for (QToolButton *button : buttons) {
    if (button->accessibleName() == accessibleName) {
      return button;
    }
  }
  return nullptr;
}

/** @brief 记录公开 delegate option，验证 view 不绕过 Model/View 协议。 */
class ZzCarouselRecordingDelegate final : public QStyledItemDelegate {
public:
  /** @brief 创建无外部资源的记录 delegate。 */
  explicit ZzCarouselRecordingDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  /** @brief 记录最近索引和状态并绘制确定性测试色块。 */
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    ++paintCount;
    lastIndex = index;
    lastState = option.state;
    if (painter != nullptr && painter->isActive()) {
      painter->fillRect(option.rect, QColor(31, 127, 71));
    }
  }

  mutable int paintCount = 0;
  mutable QModelIndex lastIndex;
  mutable QStyle::State lastState;
};

/** @brief 构造并向 viewport 投递一个 wheel event。 */
bool zzSendCarouselWheel(ZzFluentUI::ZzCarouselView *view,
                         const QPoint &pixelDelta, const QPoint &angleDelta) {
  Q_ASSERT(view != nullptr);
  const QPointF localPosition(view->viewport()->rect().center());
  const QPointF globalPosition(
      view->viewport()->mapToGlobal(localPosition.toPoint()));
  QWheelEvent event(localPosition, globalPosition, pixelDelta, angleDelta,
                    Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
  event.setAccepted(false);
  QApplication::sendEvent(view->viewport(), &event);
  return event.isAccepted();
}

} // namespace

/** @brief 验证轮播视图的模型、输入、绘制、无障碍和对象稳定性契约。 */
class ZzCarouselViewTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void exposesStablePropertiesAndModelOwnership() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    QSignalSpy rowSpy(&view, &ZzFluentUI::ZzCarouselView::currentRowChanged);
    QSignalSpy wrapSpy(&view,
                       &ZzFluentUI::ZzCarouselView::wrapAroundEnabledChanged);
    QSignalSpy durationSpy(
        &view, &ZzFluentUI::ZzCarouselView::animationDurationChanged);

    QCOMPARE(view.currentRow(), -1);
    QCOMPARE(view.animationDuration(), 220);
    QVERIFY(!view.isWrapAroundEnabled());
    view.setModel(&model);
    QCOMPARE(view.model(), &model);
    QCOMPARE(model.parent(), nullptr);
    QCOMPARE(view.currentRow(), 0);
    QCOMPARE(rowSpy.count(), 1);

    view.setModel(&model);
    QCOMPARE(rowSpy.count(), 1);
    view.setCurrentRow(2);
    QCOMPARE(view.currentRow(), 2);
    QCOMPARE(rowSpy.count(), 2);
    view.setCurrentRow(2);
    view.setCurrentRow(-1);
    view.setCurrentRow(3);
    QCOMPARE(view.currentRow(), 2);
    QCOMPARE(rowSpy.count(), 2);

    view.setWrapAroundEnabled(true);
    view.setWrapAroundEnabled(true);
    QCOMPARE(wrapSpy.count(), 1);
    QVERIFY(view.isWrapAroundEnabled());

    view.setAnimationDuration(-20);
    QCOMPARE(view.animationDuration(), 0);
    view.setAnimationDuration(-1);
    QCOMPARE(durationSpy.count(), 1);
    view.setAnimationDuration(4000);
    QCOMPARE(view.animationDuration(), 1000);
    QCOMPARE(durationSpy.count(), 2);

    view.setModel(nullptr);
    QCOMPARE(view.currentRow(), -1);
    QCOMPARE(rowSpy.count(), 3);
    QCOMPARE(model.rowCount(), 3);
  }

  void followsRootAndModelMutationsWithoutDuplicateSignals() {
    QStandardItemModel model;
    auto *firstRoot = new QStandardItem(QStringLiteral("First root"));
    auto *secondRoot = new QStandardItem(QStringLiteral("Second root"));
    firstRoot->appendRow(new QStandardItem(QStringLiteral("A")));
    firstRoot->appendRow(new QStandardItem(QStringLiteral("B")));
    secondRoot->appendRow(new QStandardItem(QStringLiteral("C")));
    model.appendRow(firstRoot);
    model.appendRow(secondRoot);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    QSignalSpy rowSpy(&view, &ZzFluentUI::ZzCarouselView::currentRowChanged);

    view.setRootIndex(model.index(0, 0));
    QCOMPARE(view.rootIndex(), model.index(0, 0));
    QCOMPARE(view.currentIndex().data().toString(), QStringLiteral("A"));
    QCOMPARE(view.currentRow(), 0);
    view.setCurrentRow(1);
    QCOMPARE(view.currentIndex().data().toString(), QStringLiteral("B"));
    QCOMPARE(rowSpy.count(), 1);

    firstRoot->insertRow(0, new QStandardItem(QStringLiteral("Before")));
    QCOMPARE(view.currentIndex().data().toString(), QStringLiteral("B"));
    QCOMPARE(view.currentRow(), 2);
    QCOMPARE(rowSpy.count(), 2);
    firstRoot->removeRow(0);
    QCOMPARE(view.currentIndex().data().toString(), QStringLiteral("B"));
    QCOMPARE(view.currentRow(), 1);
    QCOMPARE(rowSpy.count(), 3);

    view.setRootIndex(model.index(1, 0));
    QCOMPARE(view.currentIndex().data().toString(), QStringLiteral("C"));
    QCOMPARE(view.currentRow(), 0);
    QCOMPARE(rowSpy.count(), 4);
    secondRoot->removeRow(0);
    QCOMPARE(view.currentRow(), -1);
    QCOMPARE(rowSpy.count(), 5);
    secondRoot->appendRow(new QStandardItem(QStringLiteral("D")));
    QCOMPARE(view.currentIndex().data().toString(), QStringLiteral("D"));
    QCOMPARE(view.currentRow(), 0);
    QCOMPARE(rowSpy.count(), 6);
  }

  void navigatesBoundariesWrappingAndDisabledNeighbors() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);

    view.showPrevious();
    QCOMPARE(view.currentRow(), 0);
    view.showNext();
    QCOMPARE(view.currentRow(), 1);
    view.showPrevious();
    QCOMPARE(view.currentRow(), 0);
    view.setCurrentRow(2);
    view.showNext();
    QCOMPARE(view.currentRow(), 2);

    view.setWrapAroundEnabled(true);
    view.showNext();
    QCOMPARE(view.currentRow(), 0);
    view.showPrevious();
    QCOMPARE(view.currentRow(), 2);

    model.item(1)->setEnabled(false);
    view.setCurrentRow(0);
    view.showNext();
    QCOMPARE(view.currentRow(), 0);
    view.setCurrentRow(1);
    QCOMPARE(view.currentRow(), 1);
    view.showNext();
    QCOMPARE(view.currentRow(), 2);
  }

  void keepsKeyboardAndActivationSemantics() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *view = new ZzFluentUI::ZzCarouselView(&window);
    view->setAnimationDuration(0);
    view->setModel(&model);
    layout->addWidget(view);
    window.resize(420, 260);
    window.show();
    view->setFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QSignalSpy activatedSpy(view, &QAbstractItemView::activated);

    QTest::keyClick(view, Qt::Key_Right);
    QCOMPARE(view->currentRow(), 1);
    QTest::keyClick(view, Qt::Key_Left);
    QCOMPARE(view->currentRow(), 0);
    QTest::keyClick(view, Qt::Key_End);
    QCOMPARE(view->currentRow(), 2);
    QTest::keyClick(view, Qt::Key_Home);
    QCOMPARE(view->currentRow(), 0);
    QTest::keyClick(view, Qt::Key_PageDown);
    QCOMPARE(view->currentRow(), 1);
    QTest::keyClick(view, Qt::Key_Return);
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.at(0).at(0).toModelIndex(), model.index(1, 0));
    QTest::keyClick(view, Qt::Key_Enter);
    QCOMPARE(activatedSpy.count(), 2);
    QCOMPARE(activatedSpy.at(1).at(0).toModelIndex(), model.index(1, 0));
    model.item(1)->setEnabled(false);
    QTest::keyClick(view, Qt::Key_Return);
    QCOMPARE(activatedSpy.count(), 2);
    model.item(1)->setEnabled(true);

    view->setLayoutDirection(Qt::RightToLeft);
    QTest::keyClick(view, Qt::Key_Right);
    QCOMPARE(view->currentRow(), 0);
    QTest::keyClick(view, Qt::Key_Left);
    QCOMPARE(view->currentRow(), 1);
  }

  void acceptsOnlyWheelEventsThatMoveTheCurrentItem() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();

    QVERIFY(!zzSendCarouselWheel(&view, {}, QPoint(0, 120)));
    QCOMPARE(view.currentRow(), 0);
    QVERIFY(zzSendCarouselWheel(&view, {}, QPoint(0, -120)));
    QCOMPARE(view.currentRow(), 1);
    QVERIFY(zzSendCarouselWheel(&view, QPoint(20, 0), {}));
    QCOMPARE(view.currentRow(), 0);
    QVERIFY(!zzSendCarouselWheel(&view, {}, {}));
    QCOMPARE(view.currentRow(), 0);
  }

  void exposesFixedAccessibleArrowButtons() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();

    QToolButton *previous = zzCarouselButton(&view, QStringLiteral("上一项"));
    QToolButton *next = zzCarouselButton(&view, QStringLiteral("下一项"));
    QVERIFY(previous != nullptr);
    QVERIFY(next != nullptr);
    if (previous == nullptr || next == nullptr) {
      return;
    }
    QVERIFY(!previous->isEnabled());
    QVERIFY(next->isEnabled());
    QVERIFY(!previous->icon().isNull());
    QVERIFY(!next->icon().isNull());
    QCOMPARE(previous->toolTip(), previous->accessibleName());
    QCOMPARE(next->toolTip(), next->accessibleName());

    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(view.currentRow(), 1);
    QVERIFY(previous->isEnabled());
    const int previousLtrX = previous->x();
    const int nextLtrX = next->x();
    QVERIFY(previousLtrX < nextLtrX);

    view.setLayoutDirection(Qt::RightToLeft);
    QCoreApplication::processEvents();
    QVERIFY(previous->x() > next->x());
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&view, &languageChange);
    QCOMPARE(previous->accessibleName(), QStringLiteral("上一项"));
    QCOMPARE(next->accessibleName(), QStringLiteral("下一项"));
  }

  void mapsOnlyTheCurrentItemToVisualGeometry() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    view.setCurrentRow(1);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();
    QSignalSpy clickedSpy(&view, &QAbstractItemView::clicked);

    const QRect currentRect = view.visualRect(model.index(1, 0));
    QVERIFY(!currentRect.isEmpty());
    QVERIFY(view.visualRect(model.index(0, 0)).isEmpty());
    QCOMPARE(view.indexAt(currentRect.center()), model.index(1, 0));
    QVERIFY(!view.indexAt(QPoint(0, view.viewport()->height() - 1)).isValid());
    QVERIFY(view.selectionModel()->isSelected(model.index(1, 0)));

    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      currentRect.center());
    QCOMPARE(clickedSpy.count(), 1);
    QCOMPARE(clickedSpy.at(0).at(0).toModelIndex(), model.index(1, 0));
  }

  void rendersSupportedDecorationTypesAndCustomDelegateState() {
    QPixmap pixmap(160, 90);
    pixmap.fill(QColor(17, 91, 173));
    QImage image(160, 90, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(181, 61, 52));
    QIcon icon(pixmap);
    QStandardItemModel model;
    zzAppendCarouselRow(&model, QStringLiteral("Pixmap"), pixmap);
    zzAppendCarouselRow(&model, QStringLiteral("Image"), image);
    zzAppendCarouselRow(&model, QStringLiteral("Icon"), icon);
    zzAppendCarouselRow(&model, QStringLiteral("Empty"));
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();
    QImage target(view.size(), QImage::Format_ARGB32_Premultiplied);

    for (int row = 0; row < model.rowCount(); ++row) {
      view.setCurrentRow(row);
      target.fill(Qt::transparent);
      QPainter painter(&target);
      view.render(&painter);
      painter.end();
      QVERIFY(target.pixelColor(target.rect().center()).alpha() > 0);
    }

    ZzCarouselRecordingDelegate delegate;
    view.setItemDelegate(&delegate);
    view.setCurrentRow(1);
    view.setFocus(Qt::TabFocusReason);
    target.fill(Qt::transparent);
    QPainter painter(&target);
    view.render(&painter);
    painter.end();
    QCOMPARE(delegate.paintCount, 1);
    QCOMPARE(delegate.lastIndex, model.index(1, 0));
    QVERIFY(delegate.lastState.testFlag(QStyle::State_Selected));
    QVERIFY(delegate.lastState.testFlag(QStyle::State_Enabled));
  }

  void preservesItemViewAccessibility() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAccessibleName(QStringLiteral("Featured projects"));
    view.setAnimationDuration(0);
    view.setModel(&model);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();

    QAccessibleInterface *interface =
        QAccessible::queryAccessibleInterface(&view);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::List);
    QCOMPARE(interface->text(QAccessible::Name),
             QStringLiteral("Featured projects"));
    QCOMPARE(interface->childCount(), 3);
    QAccessibleInterface *itemInterface = interface->child(0);
    QVERIFY(itemInterface != nullptr);
    QCOMPARE(itemInterface->role(), QAccessible::ListItem);
    QCOMPARE(itemInterface->parent(), interface);
    QCOMPARE(interface->indexOfChild(itemInterface), 0);
    QCOMPARE(itemInterface->text(QAccessible::Name),
             QStringLiteral("Accessible Item 0"));
    QCOMPARE(itemInterface->text(QAccessible::Description),
             QStringLiteral("Item 0 description"));
    QCOMPARE(itemInterface->rect(),
             view.visualRect(view.currentIndex())
                 .translated(view.viewport()->mapToGlobal(QPoint())));
    QVERIFY(itemInterface->state().selectable);
    QVERIFY(itemInterface->state().selected);

    model.item(1)->setData(QStringLiteral("Standard description"),
                           Qt::AccessibleDescriptionRole);
    view.setCurrentRow(1);
    QCOMPARE(interface->child(0), itemInterface);
    QCOMPARE(itemInterface->text(QAccessible::Name),
             QStringLiteral("Accessible Item 1"));
    QCOMPARE(itemInterface->text(QAccessible::Description),
             QStringLiteral("Standard description"));
    model.item(1)->setEnabled(false);
    QVERIFY(itemInterface->state().disabled);

    QToolButton *next = zzCarouselButton(&view, QStringLiteral("下一项"));
    QVERIFY(next != nullptr);
    if (next == nullptr) {
      return;
    }
    QAccessibleInterface *buttonInterface =
        QAccessible::queryAccessibleInterface(next);
    QVERIFY(buttonInterface != nullptr);
    QCOMPARE(buttonInterface->role(), QAccessible::Button);
    QCOMPARE(buttonInterface->text(QAccessible::Name),
             QStringLiteral("下一项"));
    QVERIFY(interface->indexOfChild(buttonInterface) > 0);
  }

  void releasesAccessibleInterfacesWithTheView() {
    QAccessible::Id viewInterfaceId = 0;
    QAccessible::Id itemInterfaceId = 0;
    {
      QStandardItemModel model;
      zzPopulateCarouselModel(&model, 1);
      ZzFluentUI::ZzCarouselView view;
      view.setModel(&model);
      QAccessibleInterface *interface =
          QAccessible::queryAccessibleInterface(&view);
      QVERIFY(interface != nullptr);
      QAccessibleInterface *itemInterface = interface->child(0);
      QVERIFY(itemInterface != nullptr);
      viewInterfaceId = QAccessible::uniqueId(interface);
      itemInterfaceId = QAccessible::uniqueId(itemInterface);
      QVERIFY(viewInterfaceId != 0);
      QVERIFY(itemInterfaceId != 0);
    }

    QCOMPARE(QAccessible::accessibleInterface(viewInterfaceId), nullptr);
    QCOMPARE(QAccessible::accessibleInterface(itemInterfaceId), nullptr);
  }

  void repeatedUpdatesDoNotGrowTheObjectGraph() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 20);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    view.setWrapAroundEnabled(true);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();
    const qsizetype initialDescendants = view.findChildren<QObject *>().size();
    const qsizetype initialAnimations =
        view.findChildren<QAbstractAnimation *>().size();
    const qsizetype initialTimers = view.findChildren<QTimer *>().size();
    QCOMPARE(initialAnimations, 1);
    QCOMPARE(initialTimers, 0);

    for (int iteration = 0; iteration < 1000; ++iteration) {
      view.setCurrentRow(iteration % model.rowCount());
      view.resize(420 + iteration % 3, 240 + iteration % 2);
      view.setLayoutDirection((iteration % 2) == 0 ? Qt::LeftToRight
                                                   : Qt::RightToLeft);
    }
    view.setAnimationDuration(0);
    QCoreApplication::processEvents();

    QCOMPARE(view.findChildren<QObject *>().size(), initialDescendants);
    QCOMPARE(view.findChildren<QAbstractAnimation *>().size(),
             initialAnimations);
    QCOMPARE(view.findChildren<QTimer *>().size(), initialTimers);
  }
};

QTEST_MAIN(ZzCarouselViewTest)

#include "ZzCarouselViewTest.moc"
