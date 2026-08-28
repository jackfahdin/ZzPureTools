#include <ZzFluentUI/ZzCarouselView.h>

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui/QWheelEvent>

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

void ZzCarouselView::setModel(QAbstractItemModel *nextModel) {
  if (nextModel == model()) {
    return;
  }

  d_ptr->changingModelContext = true;
  d_ptr->finishTransition();
  d_ptr->disconnectModel();
  QAbstractItemView::setModel(nextModel);
  d_ptr->connectModel(nextModel);
  d_ptr->initializeCurrent();
  d_ptr->changingModelContext = false;
  d_ptr->synchronizeCurrentRow();
  d_ptr->updateButtons();
  viewport()->update();
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
  static_cast<void>(d_ptr->navigateTo(index.row(), 0, false));
  viewport()->update();
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

  d_ptr->changingModelContext = true;
  d_ptr->finishTransition();
  QAbstractItemView::setRootIndex(index);
  d_ptr->initializeCurrent();
  d_ptr->changingModelContext = false;
  d_ptr->synchronizeCurrentRow();
  d_ptr->updateButtons();
  viewport()->update();
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
  QAbstractItemView::currentChanged(current, previous);
  d_ptr->startTransition(current, previous);
  d_ptr->synchronizeCurrentRow();
  d_ptr->updateButtons();
  viewport()->update();
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
    viewport()->update();
    break;
  case QEvent::EnabledChange:
    if (!isEnabled()) {
      d_ptr->finishTransition();
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
  QAbstractItemView::hideEvent(event);
}

} // namespace ZzFluentUI
