#include <ZzFluentUI/ZzCarouselView.h>

#include <algorithm>
#include <cmath>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtGui/QCursor>
#include <QtGui/QFocusEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtCore/QPointer>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QToolButton>

#include "private/ZzCarouselViewPrivate.h"

namespace ZzFluentUI {

ZzCarouselView::ZzCarouselView(QWidget *parent)
    : QAbstractItemView(parent),
      d_ptr(std::make_unique<ZzCarouselViewPrivate>(this)) {
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectItems);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(240, 160);
  viewport()->setAutoFillBackground(false);
  d_ptr->previousButton->installEventFilter(this);
  d_ptr->nextButton->installEventFilter(this);
}

ZzCarouselView::~ZzCarouselView() = default;

bool ZzCarouselView::isWrapAroundEnabled() const noexcept {
  return d_ptr->wrapAroundEnabled;
}

void ZzCarouselView::setWrapAroundEnabled(bool enabled) {
  if (d_ptr->wrapAroundEnabled == enabled) {
    return;
  }
  d_ptr->wrapAroundEnabled = enabled;
  d_ptr->updateButtons();
  Q_EMIT wrapAroundEnabledChanged(enabled);
}

int ZzCarouselView::animationDuration() const noexcept {
  return d_ptr->animationDurationMilliseconds;
}

void ZzCarouselView::setAnimationDuration(int durationMilliseconds) {
  const int boundedDuration = std::clamp(durationMilliseconds, 0, 1000);
  if (d_ptr->animationDurationMilliseconds == boundedDuration) {
    return;
  }
  d_ptr->animationDurationMilliseconds = boundedDuration;
  if (boundedDuration == 0) {
    d_ptr->finishTransition();
  }
  Q_EMIT animationDurationChanged(boundedDuration);
}

int ZzCarouselView::currentRow() const noexcept { return d_ptr->currentRow(); }

void ZzCarouselView::setCurrentRow(int row) {
  static_cast<void>(d_ptr->navigateTo(row, 0, false));
}

bool ZzCarouselView::immersive() const noexcept { return d_ptr->immersive; }

void ZzCarouselView::setImmersive(bool immersive) {
  if (d_ptr->immersive == immersive) return;
  if (immersive) {
    d_ptr->cardFrameShape = frameShape();
    setFrameShape(QFrame::NoFrame);
  } else {
    setFrameShape(d_ptr->cardFrameShape);
  }
  d_ptr->immersive = immersive;
  d_ptr->updateButtonIcons();
  d_ptr->updateButtonGeometry();
  viewport()->update();
  Q_EMIT immersiveChanged(immersive);
}

qreal ZzCarouselView::borderRadius() const noexcept { return d_ptr->borderRadius; }

void ZzCarouselView::setBorderRadius(qreal radius) {
  if (!std::isfinite(radius)) return;
  radius = std::max<qreal>(0.0, radius);
  if (d_ptr->borderRadius == radius) return;
  d_ptr->borderRadius = radius;
  viewport()->update();
  Q_EMIT borderRadiusChanged(radius);
}

bool ZzCarouselView::showNavigationButtons() const noexcept { return d_ptr->showNavigationButtons; }

void ZzCarouselView::setShowNavigationButtons(bool show) {
  if (d_ptr->showNavigationButtons == show) return;
  d_ptr->showNavigationButtons = show;
  d_ptr->updateButtons();
  Q_EMIT showNavigationButtonsChanged(show);
}

bool ZzCarouselView::showIndicators() const noexcept { return d_ptr->showIndicators; }

void ZzCarouselView::setShowIndicators(bool show) {
  if (d_ptr->showIndicators == show) return;
  d_ptr->showIndicators = show;
  d_ptr->hoveredIndicatorRow = -1;
  d_ptr->pressedIndicatorRow = -1;
  viewport()->update();
  Q_EMIT showIndicatorsChanged(show);
}

ZzCarouselView::ZzNavigationButtonTrigger ZzCarouselView::navigationButtonTrigger() const noexcept {
  return d_ptr->navigationButtonTrigger;
}

void ZzCarouselView::setNavigationButtonTrigger(ZzNavigationButtonTrigger trigger) {
  if (trigger != AlwaysVisible && trigger != OnHover) return;
  if (d_ptr->navigationButtonTrigger == trigger) return;
  d_ptr->navigationButtonTrigger = trigger;
  d_ptr->updateNavigationReveal();
  Q_EMIT navigationButtonTriggerChanged(trigger);
}

Qt::AspectRatioMode ZzCarouselView::imageAspectRatioMode() const noexcept {
  return d_ptr->imageAspectRatioMode;
}

void ZzCarouselView::setImageAspectRatioMode(Qt::AspectRatioMode mode) {
  if (mode != Qt::IgnoreAspectRatio && mode != Qt::KeepAspectRatio && mode != Qt::KeepAspectRatioByExpanding) return;
  if (d_ptr->imageAspectRatioMode == mode) return;
  d_ptr->imageAspectRatioMode = mode;
  viewport()->update();
  Q_EMIT imageAspectRatioModeChanged(mode);
}

void ZzCarouselView::setModel(QAbstractItemModel *nextModel) {
  if (nextModel == model()) {
    return;
  }

  ++d_ptr->modelRevision;
  d_ptr->changingModelContext = true;
  d_ptr->finishTransition();
  d_ptr->disconnectModel();
  QAbstractItemView::setModel(nextModel);
  d_ptr->connectModel(nextModel);
  d_ptr->initializeCurrent();
  d_ptr->changingModelContext = false;
  QPointer<ZzCarouselView> guard(this);
  d_ptr->updateButtons();
  viewport()->update();
  d_ptr->synchronizeCurrentRow();
  if (!guard) return;
}

QRect ZzCarouselView::visualRect(const QModelIndex &index) const {
  return d_ptr->isRootItem(index) && index == currentIndex()
             ? d_ptr->contentRect()
             : QRect();
}

void ZzCarouselView::scrollTo(const QModelIndex &index, ScrollHint hint) {
  Q_UNUSED(hint)
  if (!d_ptr->isRootItem(index)) {
    return;
  }
  QPointer<ZzCarouselView> guard(this);
  static_cast<void>(d_ptr->navigateTo(index.row(), 0, false));
  if (guard) viewport()->update();
}

QModelIndex ZzCarouselView::indexAt(const QPoint &point) const {
  const QModelIndex current = currentIndex();
  return d_ptr->isRootItem(current) && d_ptr->contentRect().contains(point)
             ? current
             : QModelIndex();
}

void ZzCarouselView::setRootIndex(const QModelIndex &index) {
  if (index == rootIndex()) {
    return;
  }

  ++d_ptr->modelRevision;
  d_ptr->changingModelContext = true;
  d_ptr->finishTransition();
  QAbstractItemView::setRootIndex(index);
  d_ptr->initializeCurrent();
  d_ptr->changingModelContext = false;
  QPointer<ZzCarouselView> guard(this);
  d_ptr->updateButtons();
  viewport()->update();
  d_ptr->synchronizeCurrentRow();
  if (!guard) return;
}

void ZzCarouselView::showPrevious() {
  static_cast<void>(d_ptr->navigateBy(-1));
}

void ZzCarouselView::showNext() { static_cast<void>(d_ptr->navigateBy(1)); }

QModelIndex ZzCarouselView::moveCursor(CursorAction cursorAction,
                                       Qt::KeyboardModifiers modifiers) {
  Q_UNUSED(modifiers)
  const int row = currentRow();
  const int count = d_ptr->rowCount();
  if (row < 0 || count <= 0) {
    return {};
  }

  int direction = 0;
  int targetRow = row;
  switch (cursorAction) {
  case QAbstractItemView::MoveLeft:
    direction = layoutDirection() == Qt::RightToLeft ? 1 : -1;
    break;
  case QAbstractItemView::MoveRight:
    direction = layoutDirection() == Qt::RightToLeft ? -1 : 1;
    break;
  case QAbstractItemView::MovePrevious:
  case QAbstractItemView::MovePageUp:
    direction = -1;
    break;
  case QAbstractItemView::MoveNext:
  case QAbstractItemView::MovePageDown:
    direction = 1;
    break;
  case QAbstractItemView::MoveHome:
    targetRow = 0;
    direction = targetRow == row ? 0 : -1;
    break;
  case QAbstractItemView::MoveEnd:
    targetRow = count - 1;
    direction = targetRow == row ? 0 : 1;
    break;
  default:
    return currentIndex();
  }

  if (direction != 0 && cursorAction != QAbstractItemView::MoveHome &&
      cursorAction != QAbstractItemView::MoveEnd) {
    targetRow = row + direction;
    if (targetRow < 0 || targetRow >= count) {
      if (!d_ptr->wrapAroundEnabled) {
        return currentIndex();
      }
      targetRow = targetRow < 0 ? count - 1 : 0;
    }
  }

  const QModelIndex target = d_ptr->indexForRow(targetRow);
  if (!target.isValid() || !target.flags().testFlag(Qt::ItemIsEnabled)) {
    return currentIndex();
  }
  d_ptr->pendingDirection = direction;
  return target;
}

int ZzCarouselView::horizontalOffset() const { return 0; }

int ZzCarouselView::verticalOffset() const { return 0; }

bool ZzCarouselView::isIndexHidden(const QModelIndex &index) const {
  return !d_ptr->isRootItem(index) || index != currentIndex();
}

void ZzCarouselView::setSelection(const QRect &rect,
                                  QItemSelectionModel::SelectionFlags flags) {
  if (selectionModel() == nullptr) {
    return;
  }
  const QModelIndex current = currentIndex();
  if (d_ptr->isRootItem(current) && rect.intersects(d_ptr->contentRect())) {
    selectionModel()->select(QItemSelection(current, current), flags);
    return;
  }
  selectionModel()->select(QItemSelection(), flags);
}

QRegion ZzCarouselView::visualRegionForSelection(
    const QItemSelection &selection) const {
  QRegion region;
  const QModelIndex current = currentIndex();
  if (d_ptr->isRootItem(current) && selection.contains(current)) {
    region += d_ptr->contentRect();
  }
  return region;
}

void ZzCarouselView::currentChanged(const QModelIndex &current,
                                    const QModelIndex &previous) {
  QPointer<ZzCarouselView> guard(this);
  QAbstractItemView::currentChanged(current, previous);
  if (!guard) return;
  d_ptr->startTransition(current, previous);
  d_ptr->updateButtons();
  viewport()->update();
  if (!d_ptr->changingModelContext) {
    const int row = d_ptr->isRootItem(current) ? current.row() : -1;
    d_ptr->enqueueRowChange(row, d_ptr->navigationDepth == 0);
  }
}

void ZzCarouselView::paintEvent(QPaintEvent *event) {
  QPainter painter(viewport());
  if (event != nullptr) {
    painter.setClipRegion(event->region());
  }
  d_ptr->paint(&painter);
}

void ZzCarouselView::resizeEvent(QResizeEvent *event) {
  QAbstractItemView::resizeEvent(event);
  d_ptr->updateButtonGeometry();
  viewport()->update();
}

void ZzCarouselView::wheelEvent(QWheelEvent *event) {
  if (event == nullptr) {
    return;
  }
  const QPoint pixelDelta = event->pixelDelta();
  const QPoint angleDelta = event->angleDelta();
  int delta = 0;
  if (!pixelDelta.isNull()) {
    delta = std::abs(pixelDelta.y()) >= std::abs(pixelDelta.x())
                ? pixelDelta.y()
                : pixelDelta.x();
  } else if (!angleDelta.isNull()) {
    delta = std::abs(angleDelta.y()) >= std::abs(angleDelta.x())
                ? angleDelta.y()
                : angleDelta.x();
  }

  const bool moved =
      delta > 0 ? d_ptr->navigateBy(-1) : delta < 0 && d_ptr->navigateBy(1);
  if (moved) {
    event->accept();
  } else {
    event->ignore();
  }
}

void ZzCarouselView::keyPressEvent(QKeyEvent *event) {
  if (event == nullptr) {
    return;
  }
  CursorAction action = MoveNext;
  bool navigationKey = true;
  switch (event->key()) {
  case Qt::Key_Left: action = MoveLeft; break;
  case Qt::Key_Right: action = MoveRight; break;
  case Qt::Key_Up: action = MovePrevious; break;
  case Qt::Key_Down: action = MoveNext; break;
  case Qt::Key_PageUp: action = MovePageUp; break;
  case Qt::Key_PageDown: action = MovePageDown; break;
  case Qt::Key_Home: action = MoveHome; break;
  case Qt::Key_End: action = MoveEnd; break;
  default: navigationKey = false; break;
  }
  if (navigationKey) {
    const QModelIndex target = moveCursor(action, event->modifiers());
    const int direction = d_ptr->pendingDirection;
    d_ptr->pendingDirection = 0;
    event->accept();
    if (target.isValid()) {
      static_cast<void>(d_ptr->navigateTo(target.row(), direction, true));
    }
    return;
  }
  const bool activatesCurrent =
      (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) &&
      event->modifiers() == Qt::NoModifier;
  if (activatesCurrent) {
    const QModelIndex current = currentIndex();
    if (current.isValid() && current.flags().testFlag(Qt::ItemIsEnabled)) {
      Q_EMIT activated(current);
    }
    event->accept();
    return;
  }
  QAbstractItemView::keyPressEvent(event);
}

void ZzCarouselView::changeEvent(QEvent *event) {
  QAbstractItemView::changeEvent(event);
  if (event == nullptr) {
    return;
  }
  switch (event->type()) {
  case QEvent::LanguageChange:
    d_ptr->updateButtonText();
    break;
  case QEvent::LayoutDirectionChange:
  case QEvent::StyleChange:
    d_ptr->updateButtonIcons();
    d_ptr->updateButtonGeometry();
    d_ptr->updateButtons();
    d_ptr->updateNavigationReveal();
    viewport()->update();
    break;
  case QEvent::EnabledChange:
    if (!isEnabled()) {
      d_ptr->finishTransition();
      d_ptr->finishNavigationReveal();
    } else {
      d_ptr->updateNavigationReveal();
    }
    d_ptr->updateButtons();
    viewport()->update();
    break;
  case QEvent::FontChange:
  case QEvent::PaletteChange:
    viewport()->update();
    break;
  default:
    break;
  }
}

void ZzCarouselView::hideEvent(QHideEvent *event) {
  d_ptr->finishTransition();
  d_ptr->finishNavigationReveal();
  QAbstractItemView::hideEvent(event);
}

void ZzCarouselView::showEvent(QShowEvent *event) {
  QAbstractItemView::showEvent(event);
  d_ptr->updateNavigationReveal();
}

bool ZzCarouselView::viewportEvent(QEvent *event) {
  if (event != nullptr) {
    if (event->type() == QEvent::Enter) {
      d_ptr->hoveringView = true;
      d_ptr->updateNavigationReveal();
    } else if (event->type() == QEvent::Leave) {
      d_ptr->hoveringView = rect().contains(mapFromGlobal(QCursor::pos()));
      d_ptr->hoveredIndicatorRow = -1;
      d_ptr->updateNavigationReveal();
      viewport()->update();
    }
  }
  return QAbstractItemView::viewportEvent(event);
}

void ZzCarouselView::leaveEvent(QEvent *event) {
  QAbstractItemView::leaveEvent(event);
  d_ptr->hoveringView = rect().contains(mapFromGlobal(QCursor::pos()));
  d_ptr->updateNavigationReveal();
}

void ZzCarouselView::focusInEvent(QFocusEvent *event) {
  QAbstractItemView::focusInEvent(event);
  d_ptr->updateNavigationReveal();
}

void ZzCarouselView::focusOutEvent(QFocusEvent *event) {
  QAbstractItemView::focusOutEvent(event);
  d_ptr->updateNavigationReveal();
}

bool ZzCarouselView::eventFilter(QObject *watched, QEvent *event) {
  if ((watched == d_ptr->previousButton || watched == d_ptr->nextButton) &&
      event != nullptr && (event->type() == QEvent::FocusIn ||
                           event->type() == QEvent::FocusOut ||
                           event->type() == QEvent::Enter ||
                           event->type() == QEvent::Leave)) {
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
      d_ptr->hoveringView = rect().contains(mapFromGlobal(QCursor::pos()));
    }
    d_ptr->updateNavigationReveal();
  }
  return QAbstractItemView::eventFilter(watched, event);
}

void ZzCarouselView::mousePressEvent(QMouseEvent *event) {
  if (event != nullptr && event->button() == Qt::LeftButton) {
    const int row = d_ptr->indicatorRowAt(event->position().toPoint());
    if (row >= 0) {
      d_ptr->pressedIndicatorRow = row;
      event->accept();
      return;
    }
  }
  QAbstractItemView::mousePressEvent(event);
}

void ZzCarouselView::mouseReleaseEvent(QMouseEvent *event) {
  if (d_ptr->pressedIndicatorRow >= 0) {
    const int pressed = d_ptr->pressedIndicatorRow;
    d_ptr->pressedIndicatorRow = -1;
    if (event != nullptr && event->button() == Qt::LeftButton &&
        d_ptr->indicatorRowAt(event->position().toPoint()) == pressed) {
      static_cast<void>(d_ptr->navigateTo(pressed, 0, true));
    }
    if (event != nullptr) event->accept();
    return;
  }
  QAbstractItemView::mouseReleaseEvent(event);
}

void ZzCarouselView::mouseMoveEvent(QMouseEvent *event) {
  if (event != nullptr) {
    const int row = d_ptr->indicatorRowAt(event->position().toPoint());
    if (row != d_ptr->hoveredIndicatorRow) {
      d_ptr->hoveredIndicatorRow = row;
      viewport()->update();
    }
  }
  QAbstractItemView::mouseMoveEvent(event);
}

} // namespace ZzFluentUI
