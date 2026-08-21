#include "ZzExampleWorkspaceContent.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

namespace ZzExample {

namespace {

[[nodiscard]] QString zzTranslate(const char *text)
{
    return QCoreApplication::translate("ZzPureToolsExample", text);
}

/** @brief 仅在用户停留于尾部时随活动模型新增行滚动。 */
class ZzActivityLogView final : public QTableView
{
public:
    explicit ZzActivityLogView(QAbstractItemModel *activities)
    {
        setObjectName(QStringLiteral("zzExampleActivityLogView"));
        setAccessibleName(zzTranslate("应用活动"));
        setModel(activities);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setShowGrid(false);
        setWordWrap(false);
        horizontalHeader()->hide();
        verticalHeader()->hide();
        horizontalHeader()->setStretchLastSection(true);
        connect(
            verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
                if (!tailScrollPending_) {
                    followsTail_ = value >= verticalScrollBar()->maximum();
                }
            });
        if (activities != nullptr) {
            connect(
                activities, &QAbstractItemModel::rowsInserted,
                this, [this](const QModelIndex &parent, int, int) {
                    if (parent.isValid() || !followsTail_
                        || tailScrollPending_) {
                        return;
                    }
                    tailScrollPending_ = true;
                    QTimer::singleShot(0, this, [this] {
                        if (followsTail_) {
                            scrollToBottom();
                        }
                        tailScrollPending_ = false;
                    });
                });
        }
    }

private:
    bool followsTail_ = true;
    bool tailScrollPending_ = false;
};

[[nodiscard]] std::unique_ptr<QWidget> zzCreateLabelPanel(
    const QString &objectName,
    const QString &text)
{
    auto panel = std::make_unique<QWidget>();
    panel->setObjectName(objectName);
    auto *layout = new QVBoxLayout(panel.get());
    auto *label = new QLabel(text, panel.get());
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(label);
    return panel;
}

} // namespace

std::unique_ptr<QWidget> ZzExampleWorkspaceContent::createSessionPanel(
    QAbstractItemModel *sessions)
{
    auto panel = std::make_unique<QWidget>();
    panel->setObjectName(QStringLiteral("zzExampleSessionPanel"));
    auto *layout = new QVBoxLayout(panel.get());
    layout->setContentsMargins(8, 8, 8, 8);
    auto *tree = new QTreeView(panel.get());
    tree->setObjectName(QStringLiteral("zzExampleSessionTree"));
    tree->setAccessibleName(zzTranslate("会话"));
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    tree->setModel(sessions);
    layout->addWidget(tree);
    return panel;
}

std::unique_ptr<QWidget> ZzExampleWorkspaceContent::createTerminalPage(
    const QString &sessionName)
{
    auto terminal = std::make_unique<QPlainTextEdit>();
    terminal->setObjectName(QStringLiteral("zzExampleTerminalPage"));
    terminal->setReadOnly(true);
    terminal->setPlainText(QStringLiteral(
        "$ ssh %1\nConnected to local demonstration session.\n$ ")
                               .arg(sessionName));
    return terminal;
}

std::unique_ptr<QWidget> ZzExampleWorkspaceContent::createSftpPanel()
{
    auto tree = std::make_unique<QTreeWidget>();
    tree->setObjectName(QStringLiteral("zzExampleSftpPanel"));
    tree->setHeaderLabels({zzTranslate("名称"), zzTranslate("类型")});
    auto *root = new QTreeWidgetItem(
        tree.get(), {QStringLiteral("/srv/example"), zzTranslate("目录")});
    new QTreeWidgetItem(
        root, {QStringLiteral("releases"), zzTranslate("目录")});
    new QTreeWidgetItem(
        root, {QStringLiteral("README.txt"), zzTranslate("文件")});
    root->setExpanded(true);
    return tree;
}

std::unique_ptr<QWidget>
ZzExampleWorkspaceContent::createActivityLogPanel(
    QAbstractItemModel *activities)
{
    return std::make_unique<ZzActivityLogView>(activities);
}

std::unique_ptr<QWidget>
ZzExampleWorkspaceContent::createPropertiesPanel()
{
    auto panel = std::make_unique<QWidget>();
    panel->setObjectName(QStringLiteral("zzExamplePropertiesPanel"));
    auto *layout = new QFormLayout(panel.get());
    layout->addRow(zzTranslate("主机"), new QLabel(QStringLiteral("localhost")));
    layout->addRow(zzTranslate("端口"), new QLabel(QStringLiteral("22")));
    layout->addRow(zzTranslate("编码"), new QLabel(QStringLiteral("UTF-8")));
    return panel;
}

std::unique_ptr<QWidget> ZzExampleWorkspaceContent::createTasksPanel()
{
    return zzCreateLabelPanel(
        QStringLiteral("zzExampleTasksPanel"),
        zzTranslate("无正在运行的本地演示任务"));
}

} // namespace ZzExample
