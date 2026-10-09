#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <cstdlib>
#include <limits>
#include <QtGui/QAccessible>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtGui/QWheelEvent>
#include <QtTest/QSignalSpy>
#include <ZzTestEventLoop.h>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QProxyStyle>
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

/** @brief 模拟关闭小部件动画的系统样式。 */
class ZzCarouselNoAnimationStyle final : public QProxyStyle {
public:
  explicit ZzCarouselNoAnimationStyle(QObject *owner) { setParent(owner); }
  int styleHint(StyleHint hint, const QStyleOption *option,
                const QWidget *widget, QStyleHintReturn *returnData) const override {
    if (hint == QStyle::SH_Widget_Animate) return 0;
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
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
  void immersiveRemovesOnlyItsOwnFrame() {
    ZzFluentUI::ZzCarouselView view;
    const QFrame::Shape cardFrame = view.frameShape();
    QVERIFY(cardFrame != QFrame::NoFrame);
    view.setImmersive(true);
    QCOMPARE(view.frameShape(), QFrame::NoFrame);
    view.setImmersive(false);
    QCOMPARE(view.frameShape(), cardFrame);
  }

  void immersiveCaptionSitsAtReferenceBottom() {
    QImage image(420, 260, QImage::Format_RGB32);
    image.fill(Qt::black);
    QStandardItemModel model;
    auto *item = new QStandardItem(QStringLiteral("Mount Rainier"));
    item->setData(QStringLiteral("Snow, mountains and open skies."),
                  ZzFluentUI::ZzCarouselView::DescriptionRole);
    item->setData(image, Qt::DecorationRole);
    model.appendRow(item);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    view.setImmersive(true);
    view.setShowNavigationButtons(false);
    view.setShowIndicators(false);
    view.resize(420, 260);
    view.show();
    QCoreApplication::processEvents();
    const QImage rendered = view.viewport()->grab().toImage();
    int firstWhiteY = rendered.height();
    for (int y = rendered.height() - 76; y < rendered.height() - 8; ++y) {
      for (int x = 20; x < 185; ++x) {
        const QColor pixel = rendered.pixelColor(x, y);
        if (pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245) {
          firstWhiteY = std::min(firstWhiteY, y);
        }
      }
    }
    QVERIFY(firstWhiteY >= rendered.height() - 55);
    QVERIFY(firstWhiteY < rendered.height() - 30);
  }

  void cardButtonsStartFullyOpaque() {
    ZzFluentUI::ZzCarouselView view;
    QToolButton *next = zzCarouselButton(&view, QStringLiteral("下一项"));
    QVERIFY(next != nullptr);
    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(next->graphicsEffect());
    QVERIFY(effect != nullptr);
    QCOMPARE(effect->opacity(), 1.0);
  }

  void externalSelectionReportsEachIntermediateRow() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    QSignalSpy rowSpy(&view, &ZzFluentUI::ZzCarouselView::currentRowChanged);
    view.selectionModel()->setCurrentIndex(model.index(1, 0),
                                           QItemSelectionModel::ClearAndSelect);
    view.selectionModel()->setCurrentIndex(model.index(0, 0),
                                           QItemSelectionModel::ClearAndSelect);
    QCoreApplication::processEvents();
    QCOMPARE(rowSpy.count(), 2);
    QCOMPARE(rowSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(rowSpy.at(1).at(0).toInt(), 0);
  }

  void externalSelectionThenInternalNavigationKeepsSignalOrder() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    QSignalSpy rowSpy(&view, &ZzFluentUI::ZzCarouselView::currentRowChanged);
    view.selectionModel()->setCurrentIndex(model.index(1, 0),
                                           QItemSelectionModel::ClearAndSelect);
    view.setCurrentRow(2);
    QCoreApplication::processEvents();
    QCOMPARE(view.currentRow(), 2);
    QCOMPARE(rowSpy.count(), 2);
    QCOMPARE(rowSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(rowSpy.at(1).at(0).toInt(), 2);
    view.setCurrentRow(1);
    QCOMPARE(rowSpy.count(), 3);
    QCOMPARE(rowSpy.at(2).at(0).toInt(), 1);

    auto *deletingView = new ZzFluentUI::ZzCarouselView;
    QPointer<ZzFluentUI::ZzCarouselView> guard(deletingView);
    deletingView->setModel(&model);
    QObject::connect(deletingView, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                     deletingView, [deletingView](int row) {
                       if (row == 1) delete deletingView;
                     });
    deletingView->selectionModel()->setCurrentIndex(
        model.index(1, 0), QItemSelectionModel::ClearAndSelect);
    deletingView->setCurrentRow(2);
    QVERIFY(guard.isNull());
  }

  void reentrantNavigationPreservesEachObservedRow() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    QList<int> observedRows;
    QObject::connect(&view, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                     &view, [&view, &observedRows](int row) {
                       observedRows.append(row);
                       if (row == 1 && observedRows.size() == 1) {
                         view.setCurrentRow(1);
                       }
                     });
    view.selectionModel()->setCurrentIndex(model.index(1, 0),
                                           QItemSelectionModel::ClearAndSelect);
    view.setCurrentRow(2);
    QCoreApplication::processEvents();
    QCOMPARE(observedRows, QList<int>({1, 2, 1}));
    QCOMPARE(view.currentRow(), 1);
  }

  void narrowImmersiveCaptionClearsVisibleIndicators() {
    QImage image(820, 260, QImage::Format_RGB32);
    image.fill(Qt::black);
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 8);
    model.item(0)->setText(QStringLiteral("Landscape 1"));
    model.item(0)->setData(QStringLiteral("Explore the next destination."),
                           ZzFluentUI::ZzCarouselView::DescriptionRole);
    model.item(0)->setData(image, Qt::DecorationRole);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    view.setImmersive(true);
    view.setShowNavigationButtons(false);

    // 说明文字是否上移取决于标题/说明的真实宽度与指示点位置的重叠判定，
    // 按当前平台字体度量推导宽/窄两种尺寸，避免硬编码宽度随字体漂移。
    QFont titleFont = view.font();
    titleFont.setWeight(QFont::Bold);
    titleFont.setPixelSize(16);
    QFont descriptionFont = view.font();
    descriptionFont.setPixelSize(12);
    const int captionWidth = std::max(
        QFontMetrics(titleFont).horizontalAdvance(model.item(0)->text()),
        QFontMetrics(descriptionFont)
            .horizontalAdvance(
                model.item(0)
                    ->data(ZzFluentUI::ZzCarouselView::DescriptionRole)
                    .toString()));
    const int wideWidth = 2 * captionWidth + 240;
    const int narrowWidth = 2 * captionWidth + 100;
    view.resize(wideWidth, 260);
    view.show();
    QCoreApplication::processEvents();

    const auto firstWhiteY = [&view]() {
      const QImage rendered = view.viewport()->grab().toImage();
      int top = rendered.height();
      for (int y = rendered.height() - 110; y < rendered.height() - 8; ++y) {
        for (int x = 20; x < 180; ++x) {
          const QColor pixel = rendered.pixelColor(x, y);
          if (pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245) {
            top = std::min(top, y);
          }
        }
      }
      return top;
    };
    const int wideTop = firstWhiteY();
    view.resize(narrowWidth, 260);
    QCoreApplication::processEvents();
    const int narrowTop = firstWhiteY();
    QVERIFY(narrowTop <= wideTop - 18);
    view.setShowIndicators(false);
    QCoreApplication::processEvents();
    QCOMPARE(firstWhiteY(), wideTop);
  }

  void immersiveCaptionKeepsVerticalPositionDuringSlide() {
    QImage image(476, 260, QImage::Format_RGB32);
    image.fill(Qt::black);
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 8);
    model.item(1)->setText(QStringLiteral("Landscape 1"));
    model.item(1)->setData(QStringLiteral("Explore the next destination."),
                           ZzFluentUI::ZzCarouselView::DescriptionRole);
    model.item(1)->setData(image, Qt::DecorationRole);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    view.setImmersive(true);
    view.setShowNavigationButtons(false);
    view.setAnimationDuration(1000);
    view.resize(476, 260);
    view.show();
    QCoreApplication::processEvents();

    view.setCurrentRow(1);
    QVariantAnimation *slide = nullptr;
    for (QVariantAnimation *animation : view.findChildren<QVariantAnimation *>()) {
      if (animation->duration() == 1000) slide = animation;
    }
    QVERIFY(slide != nullptr);
    QCOMPARE(slide->state(), QAbstractAnimation::Running);

    const auto firstWhiteY = [&view](int left, int right) {
      const QImage rendered = view.viewport()->grab().toImage();
      for (int y = rendered.height() - 110; y < rendered.height() - 40; ++y) {
        for (int x = left; x < right; ++x) {
          const QColor pixel = rendered.pixelColor(x, y);
          if (pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245) {
            return y;
          }
        }
      }
      return rendered.height();
    };
    slide->setCurrentTime(100);
    const int movingTop = firstWhiteY(360, view.viewport()->width());
    slide->setCurrentTime(1000);
    const int settledTop = firstWhiteY(20, 180);
    QVERIFY(movingTop < view.viewport()->height());
    QVERIFY(settledTop < view.viewport()->height());
    QVERIFY(std::abs(movingTop - settledTop) <= 2);
  }

  void modelMutationSignalCanDeleteView() {
    {
      QStandardItemModel model;
      zzPopulateCarouselModel(&model, 1);
      auto *view = new ZzFluentUI::ZzCarouselView;
      QPointer<ZzFluentUI::ZzCarouselView> guard(view);
      view->setModel(&model);
      QObject::connect(view, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                       view, [view](int row) { if (row == -1) delete view; });
      model.clear();
      QVERIFY(guard.isNull());
    }
    {
      QStandardItemModel model;
      auto *view = new ZzFluentUI::ZzCarouselView;
      QPointer<ZzFluentUI::ZzCarouselView> guard(view);
      view->setModel(&model);
      QObject::connect(view, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                       view, [view](int row) { if (row == 0) delete view; });
      model.appendRow(new QStandardItem(QStringLiteral("Inserted")));
      QVERIFY(guard.isNull());
    }
    {
      QStandardItemModel model;
      zzPopulateCarouselModel(&model, 2);
      auto *view = new ZzFluentUI::ZzCarouselView;
      QPointer<ZzFluentUI::ZzCarouselView> guard(view);
      view->setModel(&model);
      view->setCurrentRow(1);
      QObject::connect(view, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                       view, [view](int row) { if (row == 0) delete view; });
      model.removeRow(1);
      QVERIFY(guard.isNull());
    }
    {
      QStandardItemModel model;
      model.appendRow(new QStandardItem(QStringLiteral("A")));
      model.appendRow(new QStandardItem(QStringLiteral("B")));
      auto *view = new ZzFluentUI::ZzCarouselView;
      QPointer<ZzFluentUI::ZzCarouselView> guard(view);
      view->setModel(&model);
      QObject::connect(view, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                       view, [view](int row) { if (row == 1) delete view; });
      model.sort(0, Qt::DescendingOrder);
      QVERIFY(guard.isNull());
    }
  }
  void configuresImmersivePresentationWithSingleChangeSignals() {
    ZzFluentUI::ZzCarouselView view;
    QSignalSpy immersiveSpy(&view, &ZzFluentUI::ZzCarouselView::immersiveChanged);
    QSignalSpy radiusSpy(&view, &ZzFluentUI::ZzCarouselView::borderRadiusChanged);
    QSignalSpy buttonsSpy(&view, &ZzFluentUI::ZzCarouselView::showNavigationButtonsChanged);
    QSignalSpy indicatorsSpy(&view, &ZzFluentUI::ZzCarouselView::showIndicatorsChanged);
    QSignalSpy triggerSpy(&view, &ZzFluentUI::ZzCarouselView::navigationButtonTriggerChanged);
    QSignalSpy aspectSpy(&view, &ZzFluentUI::ZzCarouselView::imageAspectRatioModeChanged);
    QCOMPARE(view.immersive(), false);
    QCOMPARE(view.borderRadius(), 6.0);
    QCOMPARE(view.showNavigationButtons(), true);
    QCOMPARE(view.showIndicators(), true);
    QCOMPARE(view.navigationButtonTrigger(), ZzFluentUI::ZzCarouselView::AlwaysVisible);
    QCOMPARE(view.imageAspectRatioMode(), Qt::KeepAspectRatioByExpanding);

    view.setImmersive(true);
    view.setImmersive(true);
    view.setBorderRadius(-5);
    view.setBorderRadius(std::numeric_limits<qreal>::quiet_NaN());
    view.setShowNavigationButtons(false);
    view.setShowNavigationButtons(false);
    view.setShowIndicators(false);
    view.setShowIndicators(false);
    view.setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::OnHover);
    const int invalidEnum = view.width();
    view.setNavigationButtonTrigger(static_cast<ZzFluentUI::ZzCarouselView::ZzNavigationButtonTrigger>(invalidEnum));
    view.setImageAspectRatioMode(Qt::KeepAspectRatio);
    view.setImageAspectRatioMode(static_cast<Qt::AspectRatioMode>(invalidEnum));
    QCOMPARE(view.borderRadius(), 0.0);
    QCOMPARE(view.navigationButtonTrigger(), ZzFluentUI::ZzCarouselView::OnHover);
    QCOMPARE(view.imageAspectRatioMode(), Qt::KeepAspectRatio);
    QCOMPARE(immersiveSpy.count(), 1);
    QCOMPARE(radiusSpy.count(), 1);
    QCOMPARE(buttonsSpy.count(), 1);
    QCOMPARE(indicatorsSpy.count(), 1);
    QCOMPARE(triggerSpy.count(), 1);
    QCOMPARE(aspectSpy.count(), 1);
  }

  void releasesIndicatorSpaceAndNavigatesVisibleWindow() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 20);
    ZzFluentUI::ZzCarouselView view;
    view.setAnimationDuration(0);
    view.setModel(&model);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();
    const int oldBottom = view.visualRect(view.currentIndex()).bottom();
    view.setShowIndicators(false);
    QVERIFY(view.visualRect(view.currentIndex()).bottom() > oldBottom);
    view.setImmersive(true);
    QCOMPARE(view.visualRect(view.currentIndex()), view.viewport()->rect());
    view.setShowIndicators(true);
    view.setCurrentRow(10);
    QSignalSpy activatedSpy(&view, &QAbstractItemView::activated);
    const int centerX = view.viewport()->width() / 2;
    const int dotY = view.viewport()->height() - 20;
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(centerX + 16, dotY));
    QCOMPARE(view.currentRow(), 11);
    QCOMPARE(activatedSpy.count(), 0);
    view.setLayoutDirection(Qt::RightToLeft);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(centerX + 16, dotY));
    QCOMPARE(view.currentRow(), 10);
    model.item(9)->setEnabled(false);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(centerX + 16, dotY));
    QCOMPARE(view.currentRow(), 10);
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(centerX, dotY));
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                        QPoint(centerX + 32, dotY));
    QCOMPARE(view.currentRow(), 10);
  }

  void revealsHoverNavigationOnlyWhileEligible() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    view.setImmersive(true);
    view.setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::OnHover);
    view.resize(420, 240);
    view.show();
    QCoreApplication::processEvents();
    view.clearFocus();
    QTest::mouseMove(&view, QPoint(-10, -10));
    QEvent initialLeave(QEvent::Leave);
    QCoreApplication::sendEvent(view.viewport(), &initialLeave);
    QToolButton *next = zzCarouselButton(&view, QStringLiteral("下一项"));
    QVERIFY(next != nullptr);
    QVERIFY(!next->isVisible());
    QVERIFY(next->focusPolicy() == Qt::NoFocus);
    view.setFocus(Qt::TabFocusReason);
    ZZ_VERIFY_EVENTUALLY(next->isVisible());
    view.clearFocus();
    QTest::mouseMove(view.viewport(), QPoint(210, 120));
    ZZ_VERIFY_EVENTUALLY(next->isVisible());
    QTest::mouseMove(&view, QPoint(-10, -10));
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(view.viewport(), &leave);
    ZZ_VERIFY_EVENTUALLY(!next->isVisible());
    view.setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::AlwaysVisible);
    view.hide();
    view.show();
    QCoreApplication::processEvents();
    QVERIFY(next->isVisible());
    QVERIFY(next->graphicsEffect() != nullptr);
    ZZ_COMPARE_EVENTUALLY(static_cast<QGraphicsOpacityEffect *>(next->graphicsEffect())->opacity(), 1.0);
    view.setShowNavigationButtons(false);
    view.setFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(!next->isVisible());
  }

  void hoverNavigationRemainsClickableAcrossViewportButtonBoundary() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    QWidget host;
    host.resize(500, 300);
    ZzFluentUI::ZzCarouselView view(&host);
    view.setStyle(new ZzCarouselNoAnimationStyle(&view));
    view.setModel(&model);
    view.setImmersive(true);
    view.setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::OnHover);
    view.setGeometry(20, 20, 420, 240);
    host.show();
    QCoreApplication::processEvents();
    view.clearFocus();
    QToolButton *next = zzCarouselButton(&view, QStringLiteral("下一项"));
    QVERIFY(next != nullptr);

    QTest::mouseMove(view.viewport(), QPoint(210, 120));
    ZZ_VERIFY_EVENTUALLY(next->isVisible());
    QVERIFY(next->isEnabled());
    QTest::mouseMove(next, next->rect().center());
    QVERIFY(next->isVisible());
    QVERIFY(next->isEnabled());
    QVERIFY(!next->testAttribute(Qt::WA_TransparentForMouseEvents));
    QTest::mouseClick(next, Qt::LeftButton, Qt::NoModifier,
                      next->rect().center());
    QCOMPARE(view.currentRow(), 1);

    next->clearFocus();
    view.clearFocus();
    QTest::mouseMove(&host, QPoint(5, 5));
    ZZ_VERIFY_EVENTUALLY(!next->isVisible());
    QCOMPARE(next->focusPolicy(), Qt::NoFocus);
  }

  void cardHoverNavigationHidesAfterLeavingFrame() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    QWidget host;
    host.resize(500, 300);
    ZzFluentUI::ZzCarouselView view(&host);
    view.setStyle(new ZzCarouselNoAnimationStyle(&view));
    view.setModel(&model);
    view.setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::OnHover);
    view.setGeometry(20, 20, 420, 240);
    host.show();
    QCoreApplication::processEvents();
    view.clearFocus();
    QVERIFY(view.viewport()->geometry().left() > 0);
    QToolButton *next = zzCarouselButton(&view, QStringLiteral("下一项"));
    QVERIFY(next != nullptr);

    QTest::mouseMove(view.viewport(), view.viewport()->rect().center());
    ZZ_VERIFY_EVENTUALLY(next->isVisible());
    QTest::mouseMove(&view, QPoint(0, 120));
    QVERIFY(next->isVisible());
    QTest::mouseMove(&host, QPoint(5, 140));
    ZZ_VERIFY_EVENTUALLY(!next->isVisible());
  }

  void stopsAnimationsForReducedMotionAndSynchronousDeletion() {
    QStandardItemModel model;
    zzPopulateCarouselModel(&model, 3);
    auto *view = new ZzFluentUI::ZzCarouselView;
    QPointer<ZzFluentUI::ZzCarouselView> guard(view);
    view->setStyle(new ZzCarouselNoAnimationStyle(view));
    view->setModel(&model);
    view->resize(420, 240);
    view->show();
    QCoreApplication::processEvents();
    view->showNext();
    QCOMPARE(view->currentRow(), 1);
    for (QAbstractAnimation *animation : view->findChildren<QAbstractAnimation *>()) {
      QCOMPARE(animation->state(), QAbstractAnimation::Stopped);
    }
    QObject::connect(view, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                     view, [view](int row) {
                       if (row == 2) delete view;
                     });
    view->showNext();
    QVERIFY(guard.isNull());

    auto *scrollView = new ZzFluentUI::ZzCarouselView;
    QPointer<ZzFluentUI::ZzCarouselView> scrollGuard(scrollView);
    scrollView->setModel(&model);
    QObject::connect(scrollView, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                     scrollView, [scrollView](int row) {
                       if (row == 2) delete scrollView;
                     });
    scrollView->scrollTo(model.index(2, 0));
    QVERIFY(scrollGuard.isNull());

    auto *keyboardView = new ZzFluentUI::ZzCarouselView;
    QPointer<ZzFluentUI::ZzCarouselView> keyboardGuard(keyboardView);
    keyboardView->setModel(&model);
    QObject::connect(keyboardView, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                     keyboardView, [keyboardView](int row) {
                       if (row == 1) delete keyboardView;
                     });
    QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QApplication::sendEvent(keyboardView, &right);
    QVERIFY(keyboardGuard.isNull());

    auto *externalView = new ZzFluentUI::ZzCarouselView;
    QPointer<ZzFluentUI::ZzCarouselView> externalGuard(externalView);
    externalView->setModel(&model);
    QObject::connect(externalView, &ZzFluentUI::ZzCarouselView::currentRowChanged,
                     externalView, [externalView](int row) {
                       if (row == 1) delete externalView;
                     });
    externalView->selectionModel()->setCurrentIndex(
        model.index(1, 0), QItemSelectionModel::ClearAndSelect);
    QCoreApplication::processEvents();
    QVERIFY(externalGuard.isNull());
  }

  void fitsImagesAndClipsImmersiveCorners() {
    QImage image(200, 100, QImage::Format_RGB32);
    image.fill(Qt::red);
    QPainter sourcePainter(&image);
    sourcePainter.fillRect(QRect(50, 0, 100, 100), Qt::green);
    sourcePainter.end();
    QStandardItemModel model;
    auto *item = new QStandardItem;
    item->setData(image, Qt::DecorationRole);
    model.appendRow(item);
    ZzFluentUI::ZzCarouselView view;
    view.setModel(&model);
    view.setImmersive(true);
    view.setBorderRadius(24);
    view.resize(300, 300);
    view.show();
    QCoreApplication::processEvents();
    const QImage cover = view.viewport()->grab().toImage();
    QCOMPARE(cover.pixelColor(150, 10), QColor(Qt::green));
    QVERIFY(cover.pixelColor(0, 0) != QColor(Qt::green));
    view.setImageAspectRatioMode(Qt::KeepAspectRatio);
    const QImage contain = view.viewport()->grab().toImage();
    QVERIFY(contain.pixelColor(150, 10) != QColor(Qt::green));
    QCOMPARE(contain.pixelColor(150, 150), QColor(Qt::green));
    view.setImageAspectRatioMode(Qt::IgnoreAspectRatio);
    const QImage stretch = view.viewport()->grab().toImage();
    QCOMPARE(stretch.pixelColor(150, 10), QColor(Qt::green));
  }
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
    QCOMPARE(initialAnimations, 2);
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
