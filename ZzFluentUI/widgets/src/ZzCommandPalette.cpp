#include <ZzFluentUI/ZzCommandPalette.h>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>

#include "private/ZzCommandPalettePrivate.h"

namespace ZzFluentUI {

ZzCommandPalette::ZzCommandPalette(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzCommandPalettePrivate>(this))
{
}

ZzCommandPalette::~ZzCommandPalette() = default;
void ZzCommandPalette::setModel(QAbstractItemModel *model) { d_ptr->setModel(model); }
QAbstractItemModel *ZzCommandPalette::model() const noexcept { return d_ptr->sourceModel.data(); }
void ZzCommandPalette::open()
{
    if (d_ptr->opened) return;
    d_ptr->opened = true;
    d_ptr->previousFocus = QApplication::focusWidget();
    d_ptr->syncGeometry();
    show();
    raise();
    d_ptr->searchEdit->setFocus();
}
void ZzCommandPalette::close()
{
    if (!d_ptr->opened) return;
    d_ptr->opened = false;
    hide();
    d_ptr->restoreFocus();
}
bool ZzCommandPalette::isOpen() const noexcept { return d_ptr->opened; }
void ZzCommandPalette::setQuery(const QString &query) { d_ptr->setQuery(query); }
QString ZzCommandPalette::query() const { return d_ptr->query; }
int ZzCommandPalette::resultCount() const noexcept { return d_ptr->proxy->rowCount(); }
QLineEdit *ZzCommandPalette::searchEdit() const noexcept { return d_ptr->searchEdit; }
QListView *ZzCommandPalette::resultView() const noexcept { return d_ptr->resultView; }
QModelIndex ZzCommandPalette::currentSourceIndex() const { return d_ptr->proxy->mapToSource(d_ptr->resultView->currentIndex()); }
bool ZzCommandPalette::activateCurrent()
{
    const QModelIndex index = currentSourceIndex();
    if (!index.isValid() || !index.flags().testFlag(Qt::ItemIsEnabled)) return false;
    Q_EMIT commandActivated(index);
    close();
    return true;
}

bool ZzCommandPalette::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == d_ptr->searchEdit || watched == d_ptr->resultView) && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) { close(); return true; }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) { return activateCurrent(); }
    }
    if (watched == this && event->type() == QEvent::MouseButtonPress) {
        close();
        return true;
    }
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        d_ptr->syncGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void ZzCommandPalette::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

} // namespace ZzFluentUI
