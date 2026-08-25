#include <ZzFluentUI/ZzCommandBar.h>

#include <QtCore/QEvent>
#include <QtGui/QAction>
#include <QtGui/QResizeEvent>

#include "private/ZzCommandBarPrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzCommandBarMinimumHeight = 32;

} // namespace

ZzCommandBar::ZzCommandBar(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzCommandBarPrivate>(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(zzCommandBarMinimumHeight);
}

ZzCommandBar::~ZzCommandBar() = default;

QList<QAction *> ZzCommandBar::primaryActions() const
{
    return d_ptr->actions(d_ptr->primaryRecords);
}

QList<QAction *> ZzCommandBar::secondaryActions() const
{
    return d_ptr->actions(d_ptr->secondaryRecords);
}

ZzCommandBarDisplayMode ZzCommandBar::displayMode() const noexcept
{
    return d_ptr->displayMode;
}

void ZzCommandBar::addPrimaryAction(QAction *action)
{
    insertPrimaryAction(static_cast<int>(d_ptr->primaryRecords.size()), action);
}

void ZzCommandBar::addSecondaryAction(QAction *action)
{
    insertSecondaryAction(
        static_cast<int>(d_ptr->secondaryRecords.size()), action);
}

QAction *ZzCommandBar::addPrimaryAction(
    const QIcon &icon,
    const QString &text)
{
    return insertPrimaryAction(
        static_cast<int>(d_ptr->primaryRecords.size()), icon, text);
}

QAction *ZzCommandBar::addSecondaryAction(
    const QIcon &icon,
    const QString &text)
{
    return insertSecondaryAction(
        static_cast<int>(d_ptr->secondaryRecords.size()), icon, text);
}

int ZzCommandBar::visiblePrimaryActionCount() const noexcept
{
    return d_ptr->visiblePrimaryActionCount;
}

bool ZzCommandBar::insertPrimaryAction(int index, QAction *action)
{
    return d_ptr->insertAction(&d_ptr->primaryRecords, index, action);
}

QAction *ZzCommandBar::insertPrimaryAction(
    int index,
    const QIcon &icon,
    QString text)
{
    auto *action = new QAction(icon, std::move(text), this);
    if (insertPrimaryAction(index, action)) {
        return action;
    }
    delete action;
    return nullptr;
}

bool ZzCommandBar::insertSecondaryAction(int index, QAction *action)
{
    return d_ptr->insertAction(&d_ptr->secondaryRecords, index, action);
}

QAction *ZzCommandBar::insertSecondaryAction(
    int index,
    const QIcon &icon,
    QString text)
{
    auto *action = new QAction(icon, std::move(text), this);
    if (insertSecondaryAction(index, action)) {
        return action;
    }
    delete action;
    return nullptr;
}

bool ZzCommandBar::removeAction(QAction *action)
{
    return d_ptr->removeAction(action);
}

void ZzCommandBar::setDisplayMode(ZzCommandBarDisplayMode mode)
{
    if (d_ptr->displayMode == mode) {
        return;
    }
    d_ptr->displayMode = mode;
    d_ptr->rebuildPresentation();
    Q_EMIT displayModeChanged(mode);
}

void ZzCommandBar::setVisiblePrimaryActionCount(int count)
{
    if (d_ptr->visiblePrimaryActionCount == count) {
        return;
    }
    d_ptr->visiblePrimaryActionCount = count;
    Q_EMIT visiblePrimaryActionCountChanged(count);
}

void ZzCommandBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    d_ptr->rebuildPresentation();
}

void ZzCommandBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::LayoutDirectionChange:
        d_ptr->invalidateWidths();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
