#include <ZzFluentUI/ZzRollerPicker.h>

#include <utility>

#include "private/ZzRollerPickerPrivate.h"

namespace ZzFluentUI {

ZzRollerPicker::ZzRollerPicker(QWidget *parent)
    : QPushButton(parent)
    , d_ptr(std::make_unique<ZzRollerPickerPrivate>(this))
{
    setCheckable(false);
    setFocusPolicy(Qt::StrongFocus);
    qRegisterMetaType<ZzRollerColumn>();
}

ZzRollerPicker::~ZzRollerPicker() = default;

void ZzRollerPicker::setColumns(QList<ZzRollerColumn> columns)
{
    static_cast<void>(d_ptr->setColumns(std::move(columns)));
}

QList<ZzRollerColumn> ZzRollerPicker::columns() const
{
    return d_ptr->columns;
}

int ZzRollerPicker::columnCount() const noexcept
{
    return static_cast<int>(d_ptr->columns.size());
}

QString ZzRollerPicker::addColumn(ZzRollerColumn column)
{
    return d_ptr->insertColumn(
        columnCount(),
        std::move(column));
}

bool ZzRollerPicker::insertColumn(
    int index,
    ZzRollerColumn column)
{
    return !d_ptr->insertColumn(index, std::move(column)).isEmpty();
}

bool ZzRollerPicker::removeColumn(const QString &key)
{
    for (int index = 0; index < d_ptr->columns.size(); ++index) {
        if (d_ptr->columns.at(index).key == key) {
            return d_ptr->removeColumnAt(index);
        }
    }
    return false;
}

bool ZzRollerPicker::removeColumnAt(int index)
{
    return d_ptr->removeColumnAt(index);
}

void ZzRollerPicker::clearColumns()
{
    static_cast<void>(d_ptr->setColumns({}));
}

bool ZzRollerPicker::setColumnItems(
    int column,
    QStringList items)
{
    return d_ptr->setColumnItems(column, std::move(items));
}

bool ZzRollerPicker::setCurrentIndex(int column, int index)
{
    return d_ptr->setCurrentIndex(column, index);
}

void ZzRollerPicker::setCurrentIndexes(const QList<int> &indexes)
{
    static_cast<void>(d_ptr->setCurrentIndexes(indexes));
}

int ZzRollerPicker::currentIndex(int column) const noexcept
{
    return column >= 0 && column < d_ptr->columns.size()
        ? d_ptr->columns.at(column).currentIndex
        : -1;
}

QList<int> ZzRollerPicker::currentIndexes() const
{
    return d_ptr->currentIndexes();
}

bool ZzRollerPicker::setCurrentText(
    int column,
    const QString &text)
{
    if (column < 0 || column >= d_ptr->columns.size()) {
        return false;
    }
    const int index = static_cast<int>(
        d_ptr->columns.at(column).items.indexOf(text));
    return index >= 0 && d_ptr->setCurrentIndex(column, index);
}

QString ZzRollerPicker::currentText(int column) const
{
    if (column < 0 || column >= d_ptr->columns.size()) {
        return {};
    }
    const ZzRollerColumn &value = d_ptr->columns.at(column);
    return value.currentIndex >= 0
            && value.currentIndex < value.items.size()
        ? value.items.at(value.currentIndex)
        : QString{};
}

QStringList ZzRollerPicker::currentTexts() const
{
    return d_ptr->currentTexts();
}

QString ZzRollerPicker::currentText() const
{
    return d_ptr->summaryText();
}

void ZzRollerPicker::showPopup()
{
    d_ptr->showPopup();
}

void ZzRollerPicker::acceptPopup()
{
    d_ptr->acceptPopup();
}

void ZzRollerPicker::cancelPopup()
{
    d_ptr->cancelPopup();
}

bool ZzRollerPicker::isPopupVisible() const noexcept
{
    return d_ptr->popupActive;
}

} // namespace ZzFluentUI
