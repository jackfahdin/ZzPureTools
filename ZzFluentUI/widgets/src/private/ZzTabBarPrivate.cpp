#include "ZzTabBarPrivate.h"

#include <algorithm>
#include <memory>

#include <QtCore/QEvent>
#include <QtGui/QCursor>
#include <QtGui/QDrag>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzTabWidget.h>

namespace ZzFluentUI {

namespace {

constexpr auto zzTabMimeFormat = "application/x-zz-fluent-tab-v1";
constexpr int zzTabDropIndicatorExtent = 2;

/** @brief 在一次嵌套拖拽循环内记录 Escape 取消意图。 */
class ZzTabDragCancelFilter final : public QObject
{
public:
    /** @brief 返回拖拽期间是否观察到 Escape。 */
    [[nodiscard]] bool wasCanceled() const noexcept
    {
        return canceled;
    }

protected:
    /** @brief 只观察按键，不消费应用事件。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (event != nullptr
            && (event->type() == QEvent::KeyPress
                || event->type() == QEvent::ShortcutOverride)) {
            const auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                canceled = true;
            }
        }
        return false;
    }

private:
    bool canceled = false;
};

} // namespace

ZzTabMimeData::ZzTabMimeData(
    ZzTabWidget *sourceWidget,
    QWidget *draggedPage,
    int draggedSourceIndex)
    : source(sourceWidget)
    , page(draggedPage)
    , sourceIndex(draggedSourceIndex)
{
    setData(format(), QByteArrayLiteral("1"));
}

QString ZzTabMimeData::format()
{
    static const QString value = QString::fromLatin1(zzTabMimeFormat);
    return value;
}

ZzTabBarPrivate::ZzTabBarPrivate(ZzTabBar *q) noexcept
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
}

void ZzTabBarPrivate::setHost(ZzTabWidget *hostWidget) noexcept
{
    host = hostWidget;
    clearPressState();
}

void ZzTabBarPrivate::clearPressState() noexcept
{
    pressedPage.clear();
    pressPosition = {};
    pressedIndex = -1;
}

void ZzTabBarPrivate::startDrag()
{
    if (dragging || host.isNull() || pressedPage.isNull()) {
        clearPressState();
        return;
    }

    const int sourceIndex = host->indexOf(pressedPage);
    if (sourceIndex < 0 || sourceIndex >= q_ptr->count()) {
        clearPressState();
        return;
    }

    QPointer<QWidget> guardedPage = pressedPage;
    QPointer<ZzTabWidget> guardedHost = host;
    const QRect sourceRect = q_ptr->tabRect(sourceIndex);
    const QPoint hotSpot = pressPosition - sourceRect.topLeft();

    auto drag = std::make_unique<QDrag>(q_ptr);
    drag->setMimeData(new ZzTabMimeData(host, pressedPage, sourceIndex));
    if (!sourceRect.isEmpty()) {
        drag->setPixmap(q_ptr->grab(sourceRect));
        drag->setHotSpot(hotSpot);
    }

    ZzTabDragCancelFilter cancelFilter;
    QApplication::instance()->installEventFilter(&cancelFilter);
    dragging = true;
    const Qt::DropAction result = drag->exec(
        Qt::MoveAction,
        Qt::MoveAction);
    dragging = false;
    QApplication::instance()->removeEventFilter(&cancelFilter);
    drag.reset();

    clearPressState();
    if (result != Qt::IgnoreAction || cancelFilter.wasCanceled()
        || !tearOffEnabled || guardedHost.isNull()
        || guardedPage.isNull()) {
        return;
    }

    const int currentIndex = guardedHost->indexOf(guardedPage);
    if (currentIndex >= 0) {
        Q_EMIT q_ptr->tearOffRequested(currentIndex, QCursor::pos());
    }
}

const ZzTabMimeData *ZzTabBarPrivate::validPayload(
    const QMimeData *mimeData) const noexcept
{
    if (!tabTransferEnabled || host.isNull() || mimeData == nullptr
        || !mimeData->hasFormat(ZzTabMimeData::format())
        || mimeData->data(ZzTabMimeData::format()) != QByteArrayLiteral("1")) {
        return nullptr;
    }

    const auto *payload = dynamic_cast<const ZzTabMimeData *>(mimeData);
    if (payload == nullptr || payload->source.isNull()
        || payload->page.isNull()) {
        return nullptr;
    }

    const int currentSourceIndex = payload->source->indexOf(payload->page);
    if (currentSourceIndex < 0
        || payload->source->widget(currentSourceIndex) != payload->page) {
        return nullptr;
    }
    return payload;
}

int ZzTabBarPrivate::insertionIndex(const QPoint &position) const
{
    return zzTabInsertionIndex(q_ptr, position);
}

QRect ZzTabBarPrivate::insertionIndicatorRect() const
{
    if (dropIndex < 0 || dropIndex > q_ptr->count()) {
        return {};
    }

    const bool vertical = zzIsVerticalTabShape(q_ptr->shape());
    if (q_ptr->count() == 0) {
        return vertical
            ? QRect(0, 0, q_ptr->width(), zzTabDropIndicatorExtent)
            : QRect(0, 0, zzTabDropIndicatorExtent, q_ptr->height());
    }

    const int referenceIndex = std::min(dropIndex, q_ptr->count() - 1);
    const QRect reference = q_ptr->tabRect(referenceIndex);
    if (vertical) {
        const int y = dropIndex < q_ptr->count()
            ? reference.top()
            : reference.bottom() - zzTabDropIndicatorExtent + 1;
        return QRect(
            reference.left(),
            y,
            reference.width(),
            zzTabDropIndicatorExtent);
    }

    int x = 0;
    if (q_ptr->layoutDirection() == Qt::RightToLeft) {
        x = dropIndex < q_ptr->count()
            ? reference.right() - zzTabDropIndicatorExtent + 1
            : reference.left();
    } else {
        x = dropIndex < q_ptr->count()
            ? reference.left()
            : reference.right() - zzTabDropIndicatorExtent + 1;
    }
    return QRect(
        x,
        reference.top(),
        zzTabDropIndicatorExtent,
        reference.height());
}

} // namespace ZzFluentUI
