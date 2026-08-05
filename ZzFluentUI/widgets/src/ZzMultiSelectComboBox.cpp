#include <ZzFluentUI/ZzMultiSelectComboBox.h>

#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>

#include "private/ZzMultiSelectComboBoxPrivate.h"

namespace ZzFluentUI {

ZzMultiSelectComboBox::ZzMultiSelectComboBox(QWidget *parent)
    : QComboBox(parent)
    , d_ptr(std::make_unique<ZzMultiSelectComboBoxPrivate>(this))
{
    qRegisterMetaType<ZzMultiSelectOption>();
}

ZzMultiSelectComboBox::~ZzMultiSelectComboBox() = default;

void ZzMultiSelectComboBox::setOptions(
    QList<ZzMultiSelectOption> options)
{
    const QList<ZzMultiSelectOption> previousSelection =
        d_ptr->selectedOptions();
    if (!d_ptr->setOptions(std::move(options))) {
        return;
    }
    d_ptr->refreshSummary();
    Q_EMIT optionsChanged();
    if (previousSelection != d_ptr->selectedOptions()) {
        Q_EMIT selectionChanged();
    }
}

QList<ZzMultiSelectOption> ZzMultiSelectComboBox::options() const
{
    return d_ptr->options();
}

int ZzMultiSelectComboBox::optionCount() const noexcept
{
    return d_ptr->optionCount();
}

QString ZzMultiSelectComboBox::addOption(
    QString text,
    QVariant payload,
    QIcon icon,
    bool selected)
{
    return addOption({{}, std::move(text), std::move(icon),
                      std::move(payload), true, selected});
}

QString ZzMultiSelectComboBox::addOption(ZzMultiSelectOption option)
{
    const QList<ZzMultiSelectOption> previousSelection =
        d_ptr->selectedOptions();
    const QString key = d_ptr->addOption(std::move(option));
    if (key.isEmpty()) {
        return {};
    }
    d_ptr->refreshSummary();
    Q_EMIT optionsChanged();
    if (previousSelection != d_ptr->selectedOptions()) {
        Q_EMIT selectionChanged();
    }
    return key;
}

bool ZzMultiSelectComboBox::removeOption(const QString &key)
{
    const QList<ZzMultiSelectOption> previousSelection =
        d_ptr->selectedOptions();
    if (!d_ptr->removeOption(key)) {
        return false;
    }
    d_ptr->refreshSummary();
    Q_EMIT optionsChanged();
    if (previousSelection != d_ptr->selectedOptions()) {
        Q_EMIT selectionChanged();
    }
    return true;
}

bool ZzMultiSelectComboBox::removeOptionAt(int index)
{
    const QList<ZzMultiSelectOption> previousSelection =
        d_ptr->selectedOptions();
    if (!d_ptr->removeOptionAt(index)) {
        return false;
    }
    d_ptr->refreshSummary();
    Q_EMIT optionsChanged();
    if (previousSelection != d_ptr->selectedOptions()) {
        Q_EMIT selectionChanged();
    }
    return true;
}

void ZzMultiSelectComboBox::clearOptions()
{
    const bool hadSelection = d_ptr->selectionCount() != 0;
    if (!d_ptr->clearOptions()) {
        return;
    }
    d_ptr->refreshSummary();
    Q_EMIT optionsChanged();
    if (hadSelection) {
        Q_EMIT selectionChanged();
    }
}

bool ZzMultiSelectComboBox::setOptionSelected(
    const QString &key,
    bool selected)
{
    if (!d_ptr->setOptionSelected(key, selected)) {
        return false;
    }
    d_ptr->refreshSummary();
    Q_EMIT selectionChanged();
    return true;
}

bool ZzMultiSelectComboBox::setOptionSelectedAt(
    int index,
    bool selected)
{
    if (!d_ptr->setOptionSelectedAt(index, selected)) {
        return false;
    }
    d_ptr->refreshSummary();
    Q_EMIT selectionChanged();
    return true;
}

void ZzMultiSelectComboBox::setSelectedKeys(QStringList keys)
{
    if (!d_ptr->setSelectedKeys(keys)) {
        return;
    }
    d_ptr->refreshSummary();
    Q_EMIT selectionChanged();
}

void ZzMultiSelectComboBox::setSelectedIndexes(QList<int> indexes)
{
    if (!d_ptr->setSelectedIndexes(indexes)) {
        return;
    }
    d_ptr->refreshSummary();
    Q_EMIT selectionChanged();
}

void ZzMultiSelectComboBox::selectAll()
{
    if (!d_ptr->selectAll()) {
        return;
    }
    d_ptr->refreshSummary();
    Q_EMIT selectionChanged();
}

void ZzMultiSelectComboBox::clearSelection()
{
    if (!d_ptr->clearSelection()) {
        return;
    }
    d_ptr->refreshSummary();
    Q_EMIT selectionChanged();
}

QList<ZzMultiSelectOption> ZzMultiSelectComboBox::selectedOptions() const
{
    return d_ptr->selectedOptions();
}

QStringList ZzMultiSelectComboBox::selectedKeys() const
{
    return d_ptr->selectedKeys();
}

QList<int> ZzMultiSelectComboBox::selectedIndexes() const
{
    return d_ptr->selectedIndexes();
}

int ZzMultiSelectComboBox::selectionCount() const noexcept
{
    return d_ptr->selectionCount();
}

QString ZzMultiSelectComboBox::selectedText() const
{
    return d_ptr->editor->text();
}

void ZzMultiSelectComboBox::setPlaceholderText(const QString &text)
{
    QComboBox::setPlaceholderText(text);
    d_ptr->editor->setPlaceholderText(text);
}

void ZzMultiSelectComboBox::showPopup()
{
    if (d_ptr->optionCount() == 0) {
        return;
    }
    QComboBox::showPopup();
    d_ptr->installEventFilters();
    if (!d_ptr->view->currentIndex().isValid()) {
        d_ptr->view->setCurrentIndex(d_ptr->view->model()->index(0, 0));
    }
}

bool ZzMultiSelectComboBox::eventFilter(
    QObject *watched,
    QEvent *event)
{
    if (d_ptr->handleEvent(watched, event)) {
        return true;
    }
    return QComboBox::eventFilter(watched, event);
}

void ZzMultiSelectComboBox::keyPressEvent(QKeyEvent *event)
{
    if (event == nullptr) {
        return;
    }
    const bool altDown = event->key() == Qt::Key_Down
        && event->modifiers().testFlag(Qt::AltModifier);
    const bool opensPopup = altDown
        || event->key() == Qt::Key_F4
        || event->key() == Qt::Key_Space
        || event->key() == Qt::Key_Return
        || event->key() == Qt::Key_Enter
        || (event->modifiers() == Qt::NoModifier
            && (event->key() == Qt::Key_Down
                || event->key() == Qt::Key_Up));
    if (opensPopup) {
        showPopup();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        hidePopup();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ZzMultiSelectComboBox::wheelEvent(QWheelEvent *event)
{
    if (event != nullptr) {
        event->ignore();
    }
}

} // namespace ZzFluentUI
