#include "ZzExampleDataViewModel.h"

#include <array>

#include <QtCore/QStringList>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

namespace ZzExample {

namespace {

constexpr int zzInitialListRows = 80;
constexpr int zzInitialTableRows = 72;
constexpr int zzInitialTreeRoots = 8;
constexpr int zzTreeGroupsPerRoot = 4;
constexpr int zzTreeLeavesPerGroup = 3;

/** @brief 创建不可编辑但可选择的标准项。 */
[[nodiscard]] QStandardItem *zzDataItem(const QString &text)
{
    auto *item = new QStandardItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

} // namespace

ZzExampleDataViewModel::ZzExampleDataViewModel(
    ZzExampleDataPageKind kind)
    : kind_(kind)
    , source_(new QStandardItemModel(this))
{
    setSourceModel(source_);
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(-1);
    setRecursiveFilteringEnabled(true);
    resetSamples();
}

ZzExampleDataViewModel::~ZzExampleDataViewModel() = default;

void ZzExampleDataViewModel::applyFilter(const QString &text)
{
    setFilterFixedString(text.trimmed());
}

void ZzExampleDataViewModel::appendSample()
{
    const int serial = nextSerial_++;
    switch (kind_) {
    case ZzExampleDataPageKind::List:
        source_->appendRow(zzDataItem(
            QStringLiteral("增量任务 %1 | 等待调度")
                .arg(serial, 3, 10, QLatin1Char('0'))));
        break;
    case ZzExampleDataPageKind::Table: {
        QList<QStandardItem *> row;
        row.reserve(4);
        row.append(zzDataItem(QStringLiteral("模块-%1").arg(serial)));
        row.append(zzDataItem(QStringLiteral("Linux")));
        row.append(zzDataItem(QStringLiteral("新增")));
        row.append(zzDataItem(QStringLiteral("%1 ms").arg(4 + serial % 17)));
        source_->appendRow(row);
        break;
    }
    case ZzExampleDataPageKind::Tree: {
        auto *root = zzDataItem(QStringLiteral("新增分组 %1").arg(serial));
        for (int child = 0; child < 3; ++child) {
            root->appendRow(zzDataItem(
                QStringLiteral("任务 %1.%2").arg(serial).arg(child + 1)));
        }
        source_->appendRow(root);
        break;
    }
    }
}

void ZzExampleDataViewModel::resetSamples()
{
    source_->clear();
    nextSerial_ = 1;
    switch (kind_) {
    case ZzExampleDataPageKind::List:
        populateList();
        break;
    case ZzExampleDataPageKind::Table:
        populateTable();
        break;
    case ZzExampleDataPageKind::Tree:
        populateTree();
        break;
    }
}

int ZzExampleDataViewModel::visibleRowCount() const
{
    return rowCount();
}

void ZzExampleDataViewModel::populateList()
{
    source_->setHorizontalHeaderLabels({QStringLiteral("任务")});
    const std::array<QString, 4> states{
        QStringLiteral("等待"),
        QStringLiteral("运行"),
        QStringLiteral("完成"),
        QStringLiteral("已缓存")};
    for (int row = 0; row < zzInitialListRows; ++row) {
        source_->appendRow(zzDataItem(
            QStringLiteral("构建任务 %1 | %2")
                .arg(row + 1, 3, 10, QLatin1Char('0'))
                .arg(states.at(static_cast<std::size_t>(row) % states.size()))));
    }
    nextSerial_ = zzInitialListRows + 1;
}

void ZzExampleDataViewModel::populateTable()
{
    source_->setHorizontalHeaderLabels({
        QStringLiteral("组件"),
        QStringLiteral("平台"),
        QStringLiteral("状态"),
        QStringLiteral("耗时")});
    const std::array<QString, 4> components{
        QStringLiteral("ZzCore"),
        QStringLiteral("ZzWindowKit"),
        QStringLiteral("ZzFluentUI"),
        QStringLiteral("ZzPureTools")};
    const std::array<QString, 3> platforms{
        QStringLiteral("Linux"),
        QStringLiteral("Windows"),
        QStringLiteral("macOS")};
    for (int rowIndex = 0; rowIndex < zzInitialTableRows; ++rowIndex) {
        QList<QStandardItem *> row;
        row.reserve(4);
        row.append(zzDataItem(components.at(
            static_cast<std::size_t>(rowIndex) % components.size())));
        row.append(zzDataItem(platforms.at(
            static_cast<std::size_t>(rowIndex) % platforms.size())));
        row.append(zzDataItem(
            rowIndex % 5 == 0
                ? QStringLiteral("检查中")
                : QStringLiteral("通过")));
        row.append(zzDataItem(QStringLiteral("%1 ms").arg(3 + rowIndex % 29)));
        source_->appendRow(row);
    }
    nextSerial_ = zzInitialTableRows + 1;
}

void ZzExampleDataViewModel::populateTree()
{
    source_->setHorizontalHeaderLabels({QStringLiteral("构建工作区")});
    for (int rootIndex = 0; rootIndex < zzInitialTreeRoots; ++rootIndex) {
        auto *root = zzDataItem(
            QStringLiteral("工作区 %1").arg(rootIndex + 1));
        for (int groupIndex = 0;
             groupIndex < zzTreeGroupsPerRoot;
             ++groupIndex) {
            auto *group = zzDataItem(
                QStringLiteral("组件组 %1.%2")
                    .arg(rootIndex + 1)
                    .arg(groupIndex + 1));
            for (int leafIndex = 0;
                 leafIndex < zzTreeLeavesPerGroup;
                 ++leafIndex) {
                group->appendRow(zzDataItem(
                    QStringLiteral("目标 %1.%2.%3")
                        .arg(rootIndex + 1)
                        .arg(groupIndex + 1)
                        .arg(leafIndex + 1)));
            }
            root->appendRow(group);
        }
        source_->appendRow(root);
    }
    nextSerial_ = zzInitialTreeRoots + 1;
}

} // namespace ZzExample
