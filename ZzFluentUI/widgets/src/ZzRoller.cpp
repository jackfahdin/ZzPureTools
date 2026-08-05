#include <ZzFluentUI/ZzRoller.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QSignalBlocker>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionSpinBox>

#include "private/ZzRollerPrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int ZzMinimumItemHeight = 24;
constexpr int ZzMaximumItemHeight = 96;
constexpr int ZzMinimumVisibleItems = 3;
constexpr int ZzMaximumVisibleItems = 9;
constexpr int ZzMinimumRollerWidth = 96;
constexpr int ZzTextHorizontalMargin = 12;
constexpr int ZzWheelStep = 120;

[[nodiscard]] QColor zzWithScaledAlpha(QColor color, qreal scale)
{
    color.setAlpha(std::clamp(
        qRound(static_cast<qreal>(color.alpha()) * scale),
        0,
        255));
    return color;
}

} // namespace

ZzRoller::ZzRoller(QWidget *parent)
    : QSpinBox(parent)
    , d_ptr(std::make_unique<ZzRollerPrivate>(this))
{
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setReadOnly(true);
    setAlignment(Qt::AlignCenter);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QLineEdit *const editor = lineEdit();
    editor->setReadOnly(true);
    editor->setFocusPolicy(Qt::NoFocus);
    editor->setFrame(false);
    editor->hide();
    setFocusProxy(nullptr);
    setFocusPolicy(Qt::StrongFocus);

    const QSignalBlocker blocker(this);
    QSpinBox::setRange(-1, -1);
    QSpinBox::setValue(-1);
    editor->setText({});
    d_ptr->refreshTextWidth();
}

ZzRoller::~ZzRoller() = default;

void ZzRoller::setItems(QStringList items)
{
    if (items.size() > std::numeric_limits<int>::max()) {
        items.resize(std::numeric_limits<int>::max());
    }
    if (d_ptr->items == items) {
        return;
    }

    const int oldIndex = currentIndex();
    d_ptr->items = std::move(items);
    const int targetIndex = d_ptr->items.isEmpty()
        ? -1
        : std::clamp(
              oldIndex < 0 ? 0 : oldIndex,
              0,
              static_cast<int>(d_ptr->items.size()) - 1);
    QSpinBox::setRange(
        targetIndex < 0 ? -1 : 0,
        targetIndex < 0
            ? -1
            : static_cast<int>(d_ptr->items.size()) - 1);
    QSpinBox::setValue(targetIndex);
    lineEdit()->setText(currentText());
    d_ptr->refreshTextWidth();
    d_ptr->notifyCurrentTextIfNeeded();
    update();
    Q_EMIT itemsChanged();
}

QStringList ZzRoller::items() const
{
    return d_ptr->items;
}

int ZzRoller::itemCount() const noexcept
{
    return static_cast<int>(d_ptr->items.size());
}

void ZzRoller::addItem(QString text)
{
    static_cast<void>(insertItem(itemCount(), std::move(text)));
}

bool ZzRoller::insertItem(int index, QString text)
{
    if (index < 0 || index > itemCount()
        || itemCount() == std::numeric_limits<int>::max()) {
        return false;
    }

    const int oldIndex = currentIndex();
    const bool hadItems = !d_ptr->items.isEmpty();
    d_ptr->items.insert(index, std::move(text));
    const int targetIndex = !hadItems
        ? 0
        : oldIndex + (index <= oldIndex ? 1 : 0);
    QSpinBox::setRange(0, itemCount() - 1);
    QSpinBox::setValue(targetIndex);
    lineEdit()->setText(currentText());
    d_ptr->refreshTextWidth();
    d_ptr->notifyCurrentTextIfNeeded();
    update();
    Q_EMIT itemsChanged();
    return true;
}

bool ZzRoller::removeItem(int index)
{
    if (!d_ptr->isValidIndex(index)) {
        return false;
    }

    const int oldIndex = currentIndex();
    d_ptr->items.removeAt(index);
    int targetIndex = -1;
    if (!d_ptr->items.isEmpty()) {
        targetIndex = index < oldIndex
            ? oldIndex - 1
            : std::min(oldIndex, itemCount() - 1);
    }
    QSpinBox::setRange(
        targetIndex < 0 ? -1 : 0,
        targetIndex < 0 ? -1 : itemCount() - 1);
    QSpinBox::setValue(targetIndex);
    lineEdit()->setText(currentText());
    d_ptr->refreshTextWidth();
    d_ptr->notifyCurrentTextIfNeeded();
    update();
    Q_EMIT itemsChanged();
    return true;
}

bool ZzRoller::setItemText(int index, QString text)
{
    if (!d_ptr->isValidIndex(index)
        || d_ptr->items.at(index) == text) {
        return false;
    }
    d_ptr->items[index] = std::move(text);
    lineEdit()->setText(currentText());
    d_ptr->refreshTextWidth();
    d_ptr->notifyCurrentTextIfNeeded();
    update();
    Q_EMIT itemsChanged();
    return true;
}

void ZzRoller::clearItems()
{
    if (d_ptr->items.isEmpty()) {
        return;
    }
    setItems({});
}

QString ZzRoller::itemText(int index) const
{
    return d_ptr->isValidIndex(index)
        ? d_ptr->items.at(index)
        : QString{};
}

void ZzRoller::setCurrentIndex(int index)
{
    if (!d_ptr->isValidIndex(index)) {
        return;
    }
    QSpinBox::setValue(index);
}

int ZzRoller::currentIndex() const noexcept
{
    return d_ptr->items.isEmpty() ? -1 : QSpinBox::value();
}

bool ZzRoller::setCurrentText(const QString &text)
{
    const int index = static_cast<int>(d_ptr->items.indexOf(text));
    if (index < 0) {
        return false;
    }
    setCurrentIndex(index);
    return true;
}

QString ZzRoller::currentText() const
{
    return itemText(currentIndex());
}

void ZzRoller::setItemHeight(int height)
{
    const int normalized = std::clamp(
        height,
        ZzMinimumItemHeight,
        ZzMaximumItemHeight);
    if (d_ptr->itemHeight == normalized) {
        return;
    }
    d_ptr->itemHeight = normalized;
    updateGeometry();
    update();
    Q_EMIT itemHeightChanged(normalized);
}

int ZzRoller::itemHeight() const noexcept
{
    return d_ptr->itemHeight;
}

void ZzRoller::setVisibleItemCount(int count)
{
    int normalized = std::clamp(
        count,
        ZzMinimumVisibleItems,
        ZzMaximumVisibleItems);
    if ((normalized % 2) == 0) {
        normalized = std::min(
            normalized + 1,
            ZzMaximumVisibleItems);
    }
    if (d_ptr->visibleItemCount == normalized) {
        return;
    }
    d_ptr->visibleItemCount = normalized;
    updateGeometry();
    update();
    Q_EMIT visibleItemCountChanged(normalized);
}

int ZzRoller::visibleItemCount() const noexcept
{
    return d_ptr->visibleItemCount;
}

QSize ZzRoller::sizeHint() const
{
    const int width = std::max(
        ZzMinimumRollerWidth,
        d_ptr->longestTextWidth + (2 * ZzTextHorizontalMargin));
    return {width, d_ptr->itemHeight * d_ptr->visibleItemCount};
}

QSize ZzRoller::minimumSizeHint() const
{
    return sizeHint();
}

QString ZzRoller::textFromValue(int value) const
{
    return itemText(value);
}

int ZzRoller::valueFromText(const QString &text) const
{
    const int index = static_cast<int>(d_ptr->items.indexOf(text));
    return index >= 0 ? index : currentIndex();
}

void ZzRoller::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QStyleOptionSpinBox option;
    initStyleOption(&option);
    option.buttonSymbols = QAbstractSpinBox::NoButtons;
    option.subControls = QStyle::SC_SpinBoxFrame
        | QStyle::SC_SpinBoxEditField;
    option.activeSubControls = QStyle::SC_None;
    style()->drawComplexControl(
        QStyle::CC_SpinBox,
        &option,
        &painter,
        this);

    if (d_ptr->items.isEmpty()) {
        return;
    }

    const QRect editRect = style()->subControlRect(
        QStyle::CC_SpinBox,
        &option,
        QStyle::SC_SpinBoxEditField,
        this);
    const int half = d_ptr->visibleItemCount / 2;
    const QPalette::ColorGroup group = isEnabled()
        ? QPalette::Normal
        : QPalette::Disabled;
    painter.save();
    painter.setClipRect(rect().adjusted(1, 1, -1, -1));

    for (int offset = -half; offset <= half; ++offset) {
        const std::int64_t candidate =
            static_cast<std::int64_t>(currentIndex()) + offset;
        int index = -1;
        if (wrapping()) {
            const std::int64_t count = d_ptr->items.size();
            index = static_cast<int>(
                ((candidate % count) + count) % count);
        } else if (candidate >= 0
                   && candidate < itemCount()) {
            index = static_cast<int>(candidate);
        }
        if (!d_ptr->isValidIndex(index)) {
            continue;
        }

        const int visualRow = offset + half;
        QRect rowRect(
            editRect.left(),
            visualRow * d_ptr->itemHeight,
            editRect.width(),
            d_ptr->itemHeight);
        if (offset == 0) {
            QColor highlight = palette().color(group, QPalette::Highlight);
            if (!isEnabled()) {
                highlight = zzWithScaledAlpha(highlight, 0.45);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(highlight);
            painter.drawRoundedRect(rowRect.adjusted(2, 2, -2, -2), 4, 4);
        } else if (offset == d_ptr->hoverOffset && isEnabled()) {
            QColor hover = zzWithScaledAlpha(
                palette().color(group, QPalette::Midlight),
                0.6);
            painter.fillRect(rowRect.adjusted(2, 2, -2, -2), hover);
        }

        QColor textColor = offset == 0
            ? palette().color(group, QPalette::HighlightedText)
            : palette().color(group, QPalette::Text);
        if (offset != 0) {
            const qreal opacity = std::max(
                0.45,
                1.0 - (0.18 * std::abs(offset)));
            textColor = zzWithScaledAlpha(textColor, opacity);
        }
        painter.setPen(textColor);
        const QString elided = fontMetrics().elidedText(
            d_ptr->items.at(index),
            Qt::ElideRight,
            std::max(0, rowRect.width() - 8));
        painter.drawText(
            rowRect.adjusted(4, 0, -4, 0),
            Qt::AlignCenter | Qt::TextSingleLine,
            elided);
    }
    painter.restore();
}

void ZzRoller::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    if (key == Qt::Key_Escape
        || key == Qt::Key_Enter
        || key == Qt::Key_Return) {
        event->ignore();
        return;
    }

    bool handled = true;
    bool changed = false;
    switch (key) {
    case Qt::Key_Up:
        changed = d_ptr->applyUserStep(1);
        break;
    case Qt::Key_Down:
        changed = d_ptr->applyUserStep(-1);
        break;
    case Qt::Key_PageUp:
        changed = d_ptr->applyUserStep(d_ptr->visibleItemCount);
        break;
    case Qt::Key_PageDown:
        changed = d_ptr->applyUserStep(-d_ptr->visibleItemCount);
        break;
    case Qt::Key_Home:
        changed = d_ptr->applyUserIndex(0);
        break;
    case Qt::Key_End:
        changed = d_ptr->applyUserIndex(itemCount() - 1);
        break;
    default:
        handled = false;
        break;
    }

    if (!handled) {
        QSpinBox::keyPressEvent(event);
        return;
    }
    event->accept();
    if (changed) {
        Q_EMIT activated(currentIndex(), currentText());
    }
}

void ZzRoller::wheelEvent(QWheelEvent *event)
{
    if (!isEnabled() || d_ptr->items.isEmpty()) {
        event->ignore();
        return;
    }

    int delta = event->angleDelta().y();
    if (event->inverted()) {
        delta = -delta;
    }
    bool changed = false;
    if (delta != 0) {
        const std::int64_t combined = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(d_ptr->wheelRemainder) + delta,
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max());
        const int steps = static_cast<int>(combined / ZzWheelStep);
        d_ptr->wheelRemainder = static_cast<int>(combined % ZzWheelStep);
        changed = steps != 0 && d_ptr->applyUserStep(steps);
    } else {
        int pixelDelta = event->pixelDelta().y();
        if (event->inverted()) {
            pixelDelta = -pixelDelta;
        }
        if (pixelDelta == 0) {
            event->ignore();
            return;
        }
        changed = d_ptr->applyUserStep(pixelDelta > 0 ? 1 : -1);
    }

    event->accept();
    if (changed) {
        Q_EMIT activated(currentIndex(), currentText());
    }
}

void ZzRoller::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton
        || !isEnabled()
        || d_ptr->items.isEmpty()
        || !rect().contains(event->position().toPoint())) {
        QSpinBox::mousePressEvent(event);
        return;
    }
    d_ptr->dragging = true;
    d_ptr->dragMoved = false;
    d_ptr->dragStartY = event->position().toPoint().y();
    d_ptr->dragStartIndex = currentIndex();
    event->accept();
}

void ZzRoller::mouseMoveEvent(QMouseEvent *event)
{
    const int offset = d_ptr->rowOffsetAt(event->position().toPoint().y());
    if (d_ptr->hoverOffset != offset) {
        d_ptr->hoverOffset = offset;
        update();
    }
    if (!d_ptr->dragging
        || !(event->buttons() & Qt::LeftButton)) {
        QSpinBox::mouseMoveEvent(event);
        return;
    }

    const int delta = d_ptr->dragStartY - event->position().toPoint().y();
    const int steps = delta / d_ptr->itemHeight;
    if (steps != 0) {
        d_ptr->dragMoved = true;
    }
    const int target = d_ptr->steppedIndex(d_ptr->dragStartIndex, steps);
    static_cast<void>(d_ptr->applyUserIndex(target));
    event->accept();
}

void ZzRoller::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !d_ptr->dragging) {
        QSpinBox::mouseReleaseEvent(event);
        return;
    }

    if (!d_ptr->dragMoved) {
        const int offset = d_ptr->rowOffsetAt(
            event->position().toPoint().y());
        if (offset != std::numeric_limits<int>::max()) {
            static_cast<void>(d_ptr->applyUserStep(offset));
        }
    }
    d_ptr->dragging = false;
    event->accept();
    if (currentIndex() != d_ptr->dragStartIndex) {
        Q_EMIT activated(currentIndex(), currentText());
    }
}

void ZzRoller::leaveEvent(QEvent *event)
{
    d_ptr->hoverOffset = std::numeric_limits<int>::max();
    update();
    QSpinBox::leaveEvent(event);
}

void ZzRoller::changeEvent(QEvent *event)
{
    QSpinBox::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
        d_ptr->refreshTextWidth();
        update();
        break;
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        updateGeometry();
        update();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
