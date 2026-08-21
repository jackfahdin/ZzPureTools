#pragma once

#include <memory>

#include <QtCore/QString>

class QAbstractItemModel;
class QWidget;

namespace ZzExample {

/** @brief 创建 SSH 风格工作区的纯本地 QWidget 展示内容。 */
class ZzExampleWorkspaceContent final
{
public:
    /** @brief 禁止实例化无状态内容工厂。 */
    ZzExampleWorkspaceContent() = delete;

    /** @brief 创建展示给定会话模型的无父侧栏页面。 */
    [[nodiscard]] static std::unique_ptr<QWidget> createSessionPanel(
        QAbstractItemModel *sessions);

    /** @brief 创建不执行命令的无父终端展示页。 */
    [[nodiscard]] static std::unique_ptr<QWidget> createTerminalPage(
        const QString &sessionName);

    /** @brief 创建固定远端目录样例的无父 SFTP 页面。 */
    [[nodiscard]] static std::unique_ptr<QWidget> createSftpPanel();

    /** @brief 创建跟随活动模型尾部的无父日志页面。 */
    [[nodiscard]] static std::unique_ptr<QWidget> createActivityLogPanel(
        QAbstractItemModel *activities);

    /** @brief 创建固定会话属性的无父页面。 */
    [[nodiscard]] static std::unique_ptr<QWidget> createPropertiesPanel();

    /** @brief 创建固定本地任务状态的无父页面。 */
    [[nodiscard]] static std::unique_ptr<QWidget> createTasksPanel();
};

} // namespace ZzExample
