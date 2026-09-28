#include "ZzExampleFileModel.h"

#include <QtCore/QCoreApplication>

namespace ZzExample {

ZzExampleFileModel::ZzExampleFileModel(QObject *parent)
    : QStandardItemModel(parent)
{
    const auto translate = [](const char *text) {
        return QCoreApplication::translate("ZzPureToolsExample", text);
    };
    const auto readOnlyItem = [](const QString &text) {
        auto *item = new QStandardItem(text);
        item->setEditable(false);
        return item;
    };
    setHorizontalHeaderLabels({translate("名称"), translate("类型")});
    auto *root = readOnlyItem(QStringLiteral("/srv/example"));
    root->appendRow({readOnlyItem(QStringLiteral("releases")), readOnlyItem(translate("目录"))});
    root->appendRow({readOnlyItem(QStringLiteral("README.txt")), readOnlyItem(translate("文件"))});
    appendRow({root, readOnlyItem(translate("目录"))});
}

} // namespace ZzExample
