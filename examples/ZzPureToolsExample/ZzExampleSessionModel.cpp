#include "ZzExampleSessionModel.h"

#include <array>

#include <QtCore/QCoreApplication>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>

#include <ZzFluentUI/ZzCommandItemRole.h>

namespace ZzExample {

namespace {

constexpr int zzCommandIdRole = Qt::UserRole + 0x520;

struct ZzCommandDescriptor final
{
    const char *title;
    const char *keywords;
    ZzExampleCommandId id;
    int priority;
};

constexpr std::array<ZzCommandDescriptor, 6> zzCommands{{
    {"新建终端", "terminal new shell", ZzExampleCommandId::NewTerminal, 100},
    {"关闭当前终端", "terminal close", ZzExampleCommandId::CloseTerminal, 90},
    {"显示 SFTP", "files sftp", ZzExampleCommandId::ShowSftp, 80},
    {"显示日志", "log activity", ZzExampleCommandId::ShowActivityLog, 70},
    {"显示属性", "properties details", ZzExampleCommandId::ShowProperties, 60},
    {"显示任务", "tasks jobs", ZzExampleCommandId::ShowTasks, 50},
}};

[[nodiscard]] QString zzTranslate(const char *text)
{
    return QCoreApplication::translate("ZzPureToolsExample", text);
}

} // namespace

ZzExampleSessionModel::ZzExampleSessionModel(QObject *parent)
    : QAbstractListModel(parent)
    , sessions_({
          {QStringLiteral("dev-local"),
           zzTranslate("本地开发环境"), QStringLiteral("local")},
          {QStringLiteral("staging"),
           zzTranslate("预发布只读会话"), QStringLiteral("staging")},
          {QStringLiteral("production"),
           zzTranslate("生产审计入口"), QStringLiteral("production")},
      })
    , commands_(std::make_unique<QStandardItemModel>())
{
    commands_->setObjectName(QStringLiteral("zzExampleWorkspaceCommands"));
    for (const ZzCommandDescriptor &descriptor : zzCommands) {
        auto *item = new QStandardItem(zzTranslate(descriptor.title));
        item->setEditable(false);
        item->setData(
            QString::fromLatin1(descriptor.keywords).split(u' '),
            static_cast<int>(ZzFluentUI::ZzCommandItemRole::Keywords));
        item->setData(
            zzTranslate("工作区"),
            static_cast<int>(ZzFluentUI::ZzCommandItemRole::Group));
        item->setData(
            descriptor.priority,
            static_cast<int>(ZzFluentUI::ZzCommandItemRole::Priority));
        item->setData(static_cast<int>(descriptor.id), zzCommandIdRole);
        commands_->appendRow(item);
    }
}

ZzExampleSessionModel::~ZzExampleSessionModel() = default;

int ZzExampleSessionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(sessions_.size());
}

QVariant ZzExampleSessionModel::data(
    const QModelIndex &index,
    int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(sessions_.size())) {
        return {};
    }
    const ZzSession &session = sessions_.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return session.name;
    case Qt::ToolTipRole:
        return session.description;
    case Qt::UserRole:
        return session.id;
    default:
        return {};
    }
}

QAbstractItemModel *ZzExampleSessionModel::commandModel() const noexcept
{
    return commands_.get();
}

ZzExampleCommandId ZzExampleSessionModel::commandId(
    const QModelIndex &index) noexcept
{
    if (!index.isValid() || index.column() != 0
        || index.model() != commands_.get()) {
        return ZzExampleCommandId::NewTerminal;
    }
    bool valid = false;
    const int value = index.data(zzCommandIdRole).toInt(&valid);
    if (!valid || value < static_cast<int>(ZzExampleCommandId::NewTerminal)
        || value > static_cast<int>(ZzExampleCommandId::ShowTasks)) {
        return ZzExampleCommandId::NewTerminal;
    }
    return static_cast<ZzExampleCommandId>(value);
}

} // namespace ZzExample
