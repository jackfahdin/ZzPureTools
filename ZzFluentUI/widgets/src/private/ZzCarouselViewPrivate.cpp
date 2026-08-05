#include "ZzCarouselViewPrivate.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEasingCurve>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QVariantAnimation>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtWidgets/QAbstractItemDelegate>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzCarouselView.h>

namespace ZzFluentUI {

namespace {

constexpr int zzCarouselOuterMargin = 8;
constexpr int zzCarouselIndicatorExtent = 24;
constexpr int zzCarouselButtonExtent = 32;
constexpr int zzCarouselButtonMargin = 12;
constexpr int zzCarouselMaximumIndicators = 7;
constexpr qreal zzCarouselCornerRadius = 6.0;

/**
 * @brief 计算 KeepAspectRatioByExpanding 对应的源裁剪矩形。
 * @param sourceSize 源图片像素尺寸。
 * @param targetSize 目标逻辑尺寸。
 * @return 位于源图片坐标内的裁剪矩形。
 */
[[nodiscard]] QRectF zzCarouselSourceRect(const QSizeF &sourceSize,
                                          const QSizeF &targetSize) {
  if (sourceSize.isEmpty() || targetSize.isEmpty()) {
    return {};
  }
  QRectF source(QPointF(0.0, 0.0), sourceSize);
  const qreal sourceRatio = sourceSize.width() / sourceSize.height();
  const qreal targetRatio = targetSize.width() / targetSize.height();
  if (sourceRatio > targetRatio) {
    const qreal width = sourceSize.height() * targetRatio;
    source.setLeft((sourceSize.width() - width) / 2.0);
    source.setWidth(width);
  } else if (sourceRatio < targetRatio) {
    const qreal height = sourceSize.width() / targetRatio;
    source.setTop((sourceSize.height() - height) / 2.0);
    source.setHeight(height);
  }
  return source;
}

/** @brief 返回 option 状态对应的 palette color group。 */
[[nodiscard]] QPalette::ColorGroup
zzCarouselColorGroup(const QStyleOptionViewItem &option) {
  if (!option.state.testFlag(QStyle::State_Enabled)) {
    return QPalette::Disabled;
  }
  return option.state.testFlag(QStyle::State_Active) ? QPalette::Active
                                                     : QPalette::Inactive;
}

/** @brief 使用标准 model role 绘制无逐项 QWidget 的轮播 item。 */
class ZzCarouselItemDelegate final : public QStyledItemDelegate {
public:
  /** @brief 创建由 view QObject 所有的默认 delegate。 */
  explicit ZzCarouselItemDelegate(QObject *parent)
      : QStyledItemDelegate(parent) {}

  /** @brief 绘制图片、标题、说明、边框和焦点，不保存 item 状态。 */
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (painter == nullptr || !painter->isActive() || !index.isValid() ||
        option.rect.isEmpty()) {
      return;
    }

    const QRectF cardRect = QRectF(option.rect).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPalette::ColorGroup group = zzCarouselColorGroup(option);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const bool focused = option.state.testFlag(QStyle::State_HasFocus);
    const bool selected = option.state.testFlag(QStyle::State_Selected);

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing |
                                QPainter::TextAntialiasing |
                                QPainter::SmoothPixmapTransform,
                            true);
    QPainterPath clip;
    clip.addRoundedRect(cardRect, zzCarouselCornerRadius,
                        zzCarouselCornerRadius);
    painter->setClipPath(clip);
    painter->fillRect(option.rect,
                      option.palette.color(group, QPalette::AlternateBase));

    const QVariant decoration = index.data(Qt::DecorationRole);
    bool paintedDecoration = false;
    painter->setOpacity(enabled ? 1.0 : 0.45);
    if (decoration.metaType().id() == QMetaType::QPixmap) {
      const QPixmap pixmap = decoration.value<QPixmap>();
      if (!pixmap.isNull()) {
        painter->drawPixmap(
            cardRect, pixmap,
            zzCarouselSourceRect(pixmap.size(), cardRect.size()));
        paintedDecoration = true;
      }
    } else if (decoration.metaType().id() == QMetaType::QImage) {
      const QImage image = decoration.value<QImage>();
      if (!image.isNull()) {
        painter->drawImage(cardRect, image,
                           zzCarouselSourceRect(image.size(), cardRect.size()));
        paintedDecoration = true;
      }
    } else if (decoration.metaType().id() == QMetaType::QIcon) {
      const QIcon icon = decoration.value<QIcon>();
      if (!icon.isNull()) {
        icon.paint(painter, option.rect, Qt::AlignCenter,
                   enabled ? QIcon::Normal : QIcon::Disabled);
        paintedDecoration = true;
      }
    }
    painter->setOpacity(1.0);

    if (!paintedDecoration && option.widget != nullptr) {
      const int placeholderExtent = std::clamp(
          std::min(option.rect.width(), option.rect.height()) / 3, 24, 64);
      const QRect placeholder(option.rect.center().x() - placeholderExtent / 2,
                              option.rect.center().y() - placeholderExtent / 2,
                              placeholderExtent, placeholderExtent);
      option.widget->style()
          ->standardIcon(QStyle::SP_FileIcon, &option, option.widget)
          .paint(painter, placeholder, Qt::AlignCenter,
                 enabled ? QIcon::Normal : QIcon::Disabled);
    }

    const QString title = index.data(Qt::DisplayRole).toString();
    const QString description =
        index.data(ZzCarouselView::DescriptionRole).toString();
    if (!title.isEmpty() || !description.isEmpty()) {
      const int bandHeight = description.isEmpty() ? 52 : 76;
      const QRect bandRect(
          option.rect.left(),
          std::max(option.rect.top(), option.rect.bottom() - bandHeight + 1),
          option.rect.width(), std::min(bandHeight, option.rect.height()));
      QColor bandColor = option.palette.color(group, QPalette::Window);
      bandColor.setAlpha(232);
      painter->fillRect(bandRect, bandColor);

      const QRect textRect = bandRect.adjusted(16, 8, -16, -8);
      QFont titleFont = option.font;
      titleFont.setWeight(QFont::DemiBold);
      painter->setFont(titleFont);
      painter->setPen(option.palette.color(group, QPalette::WindowText));
      const QFontMetrics titleMetrics(titleFont);
      const int titleHeight = titleMetrics.height();
      painter->drawText(
          QRect(textRect.left(), textRect.top(), textRect.width(), titleHeight),
          Qt::AlignLeading | Qt::AlignVCenter,
          titleMetrics.elidedText(title, option.textElideMode,
                                  textRect.width()));

      if (!description.isEmpty()) {
        painter->setFont(option.font);
        painter->setPen(option.palette.color(group, QPalette::PlaceholderText));
        const QFontMetrics descriptionMetrics(option.font);
        painter->drawText(
            QRect(textRect.left(), textRect.top() + titleHeight + 2,
                  textRect.width(),
                  std::max(0, textRect.height() - titleHeight - 2)),
            Qt::AlignLeading | Qt::AlignTop | Qt::TextWordWrap,
            descriptionMetrics.elidedText(description, option.textElideMode,
                                          textRect.width()));
      }
    }
    painter->restore();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor borderColor =
        focused || selected ? option.palette.color(group, QPalette::Highlight)
                            : option.palette.color(group, QPalette::Mid);
    painter->setPen(QPen(borderColor, focused ? 2.0 : 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(focused ? cardRect.adjusted(0.5, 0.5, -0.5, -0.5)
                                     : cardRect,
                             zzCarouselCornerRadius, zzCarouselCornerRadius);
    painter->restore();
  }
};

} // namespace

ZzCarouselViewPrivate::ZzCarouselViewPrivate(ZzCarouselView *q)
    : q_ptr(q), previousButton(new QToolButton(q)),
      nextButton(new QToolButton(q)), animation(new QVariantAnimation(q)) {
  Q_ASSERT(q_ptr != nullptr);
  q_ptr->setItemDelegate(new ZzCarouselItemDelegate(q_ptr));

  for (QToolButton *button : {previousButton, nextButton}) {
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(16, 16));
    button->resize(zzCarouselButtonExtent, zzCarouselButtonExtent);
  }
  QObject::connect(previousButton, &QToolButton::clicked, q_ptr,
                   &ZzCarouselView::showPrevious);
  QObject::connect(nextButton, &QToolButton::clicked, q_ptr,
                   &ZzCarouselView::showNext);

  animation->setStartValue(0.0);
  animation->setEndValue(1.0);
  animation->setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(animation, &QVariantAnimation::valueChanged, q_ptr,
                   [this](const QVariant &value) {
                     if (!q_ptr->isEnabled() ||
                         q_ptr->style()->styleHint(QStyle::SH_Widget_Animate,
                                                   nullptr, q_ptr) == 0) {
                       finishTransition();
                       return;
                     }
                     transitionProgress = std::clamp(value.toReal(), 0.0, 1.0);
                     q_ptr->viewport()->update();
                   });
  QObject::connect(animation, &QVariantAnimation::finished, q_ptr, [this]() {
    transitionProgress = 1.0;
    previousIndex = QPersistentModelIndex();
    q_ptr->viewport()->update();
  });

  updateButtonText();
  updateButtonIcons();
  updateButtonGeometry();
  updateButtons();
}

ZzCarouselViewPrivate::~ZzCarouselViewPrivate() {
  animation->stop();
  disconnectModel();
}

QRect ZzCarouselViewPrivate::contentRect() const {
  return q_ptr->viewport()->rect().adjusted(
      zzCarouselOuterMargin, zzCarouselOuterMargin, -zzCarouselOuterMargin,
      -(zzCarouselOuterMargin + zzCarouselIndicatorExtent));
}

int ZzCarouselViewPrivate::rowCount() const {
  return q_ptr->model() == nullptr
             ? 0
             : q_ptr->model()->rowCount(q_ptr->rootIndex());
}

QModelIndex ZzCarouselViewPrivate::indexForRow(int row) const {
  if (q_ptr->model() == nullptr || row < 0 || row >= rowCount()) {
    return {};
  }
  return q_ptr->model()->index(row, 0, q_ptr->rootIndex());
}

bool ZzCarouselViewPrivate::isRootItem(const QModelIndex &index) const {
  return index.isValid() && index.model() == q_ptr->model() &&
         index.parent() == q_ptr->rootIndex() && index.column() == 0;
}

int ZzCarouselViewPrivate::currentRow() const noexcept {
  const QModelIndex current = q_ptr->currentIndex();
  return isRootItem(current) ? current.row() : -1;
}

void ZzCarouselViewPrivate::connectModel(QAbstractItemModel *model) {
  if (model == nullptr) {
    return;
  }

  modelConnections.append(
      QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, q_ptr,
                       [this]() { finishTransition(); }));
  modelConnections.append(
      QObject::connect(model, &QAbstractItemModel::modelReset, q_ptr, [this]() {
        initializeCurrent();
        synchronizeCurrentRow();
        updateButtons();
        q_ptr->viewport()->update();
      }));
  modelConnections.append(QObject::connect(
      model, &QAbstractItemModel::rowsAboutToBeRemoved, q_ptr,
      [this](const QModelIndex &, int, int) { finishTransition(); }));
  modelConnections.append(
      QObject::connect(model, &QAbstractItemModel::rowsInserted, q_ptr,
                       [this](const QModelIndex &, int, int) {
                         initializeCurrent();
                         synchronizeCurrentRow();
                         updateButtons();
                         q_ptr->viewport()->update();
                       }));
  modelConnections.append(
      QObject::connect(model, &QAbstractItemModel::rowsRemoved, q_ptr,
                       [this](const QModelIndex &, int, int) {
                         initializeCurrent();
                         synchronizeCurrentRow();
                         updateButtons();
                         q_ptr->viewport()->update();
                       }));
  modelConnections.append(QObject::connect(
      model, &QAbstractItemModel::rowsMoved, q_ptr,
      [this](const QModelIndex &, int, int, const QModelIndex &, int) {
        synchronizeCurrentRow();
        updateButtons();
        q_ptr->viewport()->update();
      }));
  modelConnections.append(
      QObject::connect(model, &QAbstractItemModel::layoutAboutToBeChanged,
                       q_ptr, [this]() { finishTransition(); }));
  modelConnections.append(QObject::connect(
      model, &QAbstractItemModel::layoutChanged, q_ptr, [this]() {
        initializeCurrent();
        synchronizeCurrentRow();
        updateButtons();
        q_ptr->viewport()->update();
      }));
  modelConnections.append(QObject::connect(
      model, &QAbstractItemModel::dataChanged, q_ptr,
      [this](const QModelIndex &, const QModelIndex &, const QList<int> &) {
        updateButtons();
        q_ptr->viewport()->update();
      }));
}

void ZzCarouselViewPrivate::disconnectModel() {
  for (const QMetaObject::Connection &connection : modelConnections) {
    QObject::disconnect(connection);
  }
  modelConnections.clear();
}

void ZzCarouselViewPrivate::initializeCurrent() {
  if (isRootItem(q_ptr->currentIndex())) {
    return;
  }
  if (q_ptr->selectionModel() == nullptr || rowCount() <= 0) {
    if (q_ptr->selectionModel() != nullptr) {
      q_ptr->selectionModel()->clearCurrentIndex();
      q_ptr->selectionModel()->clearSelection();
    }
    return;
  }
  const QModelIndex first = indexForRow(0);
  q_ptr->selectionModel()->setCurrentIndex(first,
                                           QItemSelectionModel::ClearAndSelect);
}

void ZzCarouselViewPrivate::synchronizeCurrentRow() {
  if (changingModelContext) {
    return;
  }
  const int row = currentRow();
  if (lastReportedRow == row) {
    return;
  }
  lastReportedRow = row;
  Q_EMIT q_ptr->currentRowChanged(row);
}

bool ZzCarouselViewPrivate::navigateTo(int row, int direction,
                                       bool requireEnabled) {
  const QModelIndex target = indexForRow(row);
  if (!target.isValid() || q_ptr->selectionModel() == nullptr ||
      target == q_ptr->currentIndex() ||
      (requireEnabled && !target.flags().testFlag(Qt::ItemIsEnabled))) {
    return false;
  }

  finishTransition();
  const int oldRow = currentRow();
  pendingDirection = direction != 0 ? direction : (row >= oldRow ? 1 : -1);
  q_ptr->selectionModel()->setCurrentIndex(target,
                                           QItemSelectionModel::ClearAndSelect);
  if (q_ptr->currentIndex() != target) {
    pendingDirection = 0;
    return false;
  }
  return true;
}

bool ZzCarouselViewPrivate::navigateBy(int delta) {
  if (delta == 0) {
    return false;
  }
  const int count = rowCount();
  const int row = currentRow();
  if (count <= 0 || row < 0) {
    return false;
  }

  const int direction = delta < 0 ? -1 : 1;
  int targetRow = row + direction;
  if (targetRow < 0 || targetRow >= count) {
    if (!wrapAroundEnabled || count <= 1) {
      return false;
    }
    targetRow = targetRow < 0 ? count - 1 : 0;
  }
  return navigateTo(targetRow, direction, true);
}

void ZzCarouselViewPrivate::startTransition(const QModelIndex &current,
                                            const QModelIndex &previous) {
  if (changingModelContext || !isRootItem(current) || !isRootItem(previous) ||
      current == previous) {
    pendingDirection = 0;
    finishTransition();
    return;
  }

  animation->stop();
  previousIndex = previous;
  transitionProgress = 0.0;
  transitionDirection = pendingDirection != 0
                            ? pendingDirection
                            : (current.row() >= previous.row() ? 1 : -1);
  pendingDirection = 0;
  const bool shouldAnimate =
      q_ptr->isVisible() && q_ptr->isEnabled() &&
      animationDurationMilliseconds > 0 &&
      q_ptr->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, q_ptr) != 0;
  if (!shouldAnimate) {
    finishTransition();
    return;
  }
  animation->setDuration(animationDurationMilliseconds);
  animation->setStartValue(0.0);
  animation->setEndValue(1.0);
  animation->start();
}

void ZzCarouselViewPrivate::finishTransition() noexcept {
  animation->stop();
  transitionProgress = 1.0;
  previousIndex = QPersistentModelIndex();
  pendingDirection = 0;
  q_ptr->viewport()->update();
}

void ZzCarouselViewPrivate::paint(QPainter *painter) const {
  if (painter == nullptr || !painter->isActive()) {
    return;
  }
  painter->fillRect(q_ptr->viewport()->rect(),
                    q_ptr->palette().color(q_ptr->isEnabled()
                                               ? QPalette::Active
                                               : QPalette::Disabled,
                                           QPalette::Base));

  const QRect content = contentRect();
  const QModelIndex current = q_ptr->currentIndex();
  if (content.isEmpty() || !isRootItem(current)) {
    return;
  }

  painter->save();
  painter->setClipRect(content);
  if (previousIndex.isValid() && transitionProgress < 1.0) {
    int visualDirection = transitionDirection;
    if (q_ptr->layoutDirection() == Qt::RightToLeft) {
      visualDirection *= -1;
    }
    const qreal width = static_cast<qreal>(content.width());
    const int previousOffset = static_cast<int>(
        std::lround(-visualDirection * width * transitionProgress));
    const int currentOffset = static_cast<int>(
        std::lround(visualDirection * width * (1.0 - transitionProgress)));
    paintIndex(painter, previousIndex, content.translated(previousOffset, 0));
    paintIndex(painter, current, content.translated(currentOffset, 0));
  } else {
    paintIndex(painter, current, content);
  }
  painter->restore();
  paintIndicators(painter);
}

void ZzCarouselViewPrivate::updateButtons() {
  const int count = rowCount();
  const int row = currentRow();
  const bool visible = count > 1 && row >= 0;
  previousButton->setVisible(visible);
  nextButton->setVisible(visible);
  if (!visible) {
    return;
  }

  const int previousRow = row > 0 ? row - 1 : count - 1;
  const int nextRow = row + 1 < count ? row + 1 : 0;
  const bool canWrap = wrapAroundEnabled && count > 1;
  const QModelIndex previous = indexForRow(previousRow);
  const QModelIndex next = indexForRow(nextRow);
  previousButton->setEnabled(q_ptr->isEnabled() && (row > 0 || canWrap) &&
                             previous.flags().testFlag(Qt::ItemIsEnabled));
  nextButton->setEnabled(q_ptr->isEnabled() && (row + 1 < count || canWrap) &&
                         next.flags().testFlag(Qt::ItemIsEnabled));
  previousButton->raise();
  nextButton->raise();
}

void ZzCarouselViewPrivate::updateButtonGeometry() {
  const QRect viewportGeometry = q_ptr->viewport()->geometry();
  const int y = viewportGeometry.top() +
                (viewportGeometry.height() - zzCarouselButtonExtent) / 2;
  const int left = viewportGeometry.left() + zzCarouselButtonMargin;
  const int right = viewportGeometry.right() - zzCarouselButtonMargin -
                    zzCarouselButtonExtent + 1;
  const bool rtl = q_ptr->layoutDirection() == Qt::RightToLeft;
  previousButton->setGeometry(rtl ? right : left, y, zzCarouselButtonExtent,
                              zzCarouselButtonExtent);
  nextButton->setGeometry(rtl ? left : right, y, zzCarouselButtonExtent,
                          zzCarouselButtonExtent);
  previousButton->raise();
  nextButton->raise();
}

void ZzCarouselViewPrivate::updateButtonIcons() {
  const bool rtl = q_ptr->layoutDirection() == Qt::RightToLeft;
  previousButton->setIcon(q_ptr->style()->standardIcon(
      rtl ? QStyle::SP_ArrowRight : QStyle::SP_ArrowLeft, nullptr, q_ptr));
  nextButton->setIcon(q_ptr->style()->standardIcon(
      rtl ? QStyle::SP_ArrowLeft : QStyle::SP_ArrowRight, nullptr, q_ptr));
}

void ZzCarouselViewPrivate::updateButtonText() {
  const QString previousText = ZzCarouselView::tr("上一项");
  const QString nextText = ZzCarouselView::tr("下一项");
  previousButton->setToolTip(previousText);
  previousButton->setAccessibleName(previousText);
  nextButton->setToolTip(nextText);
  nextButton->setAccessibleName(nextText);
}

QStyleOptionViewItem
ZzCarouselViewPrivate::itemOption(const QModelIndex &index,
                                  const QRect &rect) const {
  QStyleOptionViewItem option;
  q_ptr->initViewItemOption(&option);
  option.rect = rect;
  option.widget = q_ptr;
  option.showDecorationSelected = true;
  option.textElideMode = Qt::ElideRight;
  if (index.flags().testFlag(Qt::ItemIsEnabled) && q_ptr->isEnabled()) {
    option.state |= QStyle::State_Enabled;
  } else {
    option.state &= ~QStyle::State_Enabled;
  }
  if (q_ptr->selectionModel() != nullptr &&
      q_ptr->selectionModel()->isSelected(index)) {
    option.state |= QStyle::State_Selected;
  } else {
    option.state &= ~QStyle::State_Selected;
  }
  if (index == q_ptr->currentIndex() && q_ptr->hasFocus()) {
    option.state |= QStyle::State_HasFocus;
  } else {
    option.state &= ~QStyle::State_HasFocus;
  }
  return option;
}

void ZzCarouselViewPrivate::paintIndex(QPainter *painter,
                                       const QModelIndex &index,
                                       const QRect &rect) const {
  if (!index.isValid() || rect.isEmpty()) {
    return;
  }
  QAbstractItemDelegate *delegate = q_ptr->itemDelegateForIndex(index);
  if (delegate == nullptr) {
    return;
  }
  delegate->paint(painter, itemOption(index, rect), index);
}

void ZzCarouselViewPrivate::paintIndicators(QPainter *painter) const {
  const int count = rowCount();
  const int row = currentRow();
  if (painter == nullptr || count <= 1 || row < 0) {
    return;
  }

  const int visibleCount = std::min(count, zzCarouselMaximumIndicators);
  const int halfWindow = visibleCount / 2;
  const int firstRow = std::clamp(row - halfWindow, 0, count - visibleCount);
  constexpr qreal spacing = 14.0;
  const qreal totalWidth = static_cast<qreal>(visibleCount - 1) * spacing;
  const qreal startX =
      static_cast<qreal>(q_ptr->viewport()->width()) / 2.0 - totalWidth / 2.0;
  const qreal y = static_cast<qreal>(q_ptr->viewport()->height()) -
                  static_cast<qreal>(zzCarouselIndicatorExtent) / 2.0;
  const QPalette::ColorGroup group =
      q_ptr->isEnabled() ? QPalette::Active : QPalette::Disabled;

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::NoPen);
  for (int index = 0; index < visibleCount; ++index) {
    const bool current = firstRow + index == row;
    QColor color = current
                       ? q_ptr->palette().color(group, QPalette::Highlight)
                       : q_ptr->palette().color(group, QPalette::ButtonText);
    if (!current) {
      color.setAlpha(112);
    }
    painter->setBrush(color);
    const qreal radius = current ? 3.5 : 2.5;
    painter->drawEllipse(
        QPointF(startX + static_cast<qreal>(index) * spacing, y), radius,
        radius);
  }
  painter->restore();
}

} // namespace ZzFluentUI
