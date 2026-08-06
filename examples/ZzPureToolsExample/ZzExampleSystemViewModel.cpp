#include "ZzExampleSystemViewModel.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QStandardItem>

namespace ZzExample {

namespace {

/** @brief 创建不可编辑的系统快照项。 */
[[nodiscard]] QStandardItem *zzSystemItem(const QString &text)
{
    auto *item = new QStandardItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

} // namespace

ZzExampleSystemViewModel::ZzExampleSystemViewModel()
{
    setColumnCount(2);
    setHorizontalHeaderLabels({
        QCoreApplication::translate("ZzPureToolsExample", "项目"),
        QCoreApplication::translate("ZzPureToolsExample", "值")});
}

ZzExampleSystemViewModel::~ZzExampleSystemViewModel() = default;

void ZzExampleSystemViewModel::setRows(
    const QList<QPair<QString, QString>> &rows)
{
    setRowCount(0);
    for (const auto &[name, value] : rows) {
        appendRow({zzSystemItem(name), zzSystemItem(value)});
    }
}

} // namespace ZzExample
