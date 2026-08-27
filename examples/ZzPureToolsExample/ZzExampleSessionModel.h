#pragma once

#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QList>
#include <QtCore/QString>

class QStandardItemModel;

namespace ZzExample {

/** @brief 标识示例命令面板中的稳定本地意图。 */
enum class ZzExampleCommandId : int
{
    NewTerminal,
    CloseTerminal,
    ShowSftp,
    ShowActivityLog,
    ShowProperties,
    ShowTasks,
    ShowSettings,
};

/**
 * @brief 提供 SSH 风格示例使用的固定本地会话和命令模型。
 *
 * 模型不访问网络、数据库或设置；所有行均在构造时从进程内常量创建。
 */
class ZzExampleSessionModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzExampleSessionModel)

public:
    /** @brief 创建三条固定会话和七条工作区命令。 */
    explicit ZzExampleSessionModel(QObject *parent = nullptr);

    /** @brief 释放内部命令模型。 */
    ~ZzExampleSessionModel() override;

    /** @brief 返回固定会话数量，合法父索引下返回零。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = {}) const override;

    /** @brief 返回会话名称、描述和稳定本地标识。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    /** @brief 返回由本对象拥有的平面命令模型。 */
    [[nodiscard]] QAbstractItemModel *commandModel() const noexcept;

    /**
     * @brief 从本对象命令模型的源索引读取稳定意图。
     * @param index commandModel() 返回模型中的有效索引。
     * @return 无效、错误列或外来模型索引回退为 NewTerminal。
     */
    [[nodiscard]] ZzExampleCommandId commandId(
        const QModelIndex &index) noexcept;

private:
    struct ZzSession final
    {
        QString name;
        QString description;
        QString id;
    };

    QList<ZzSession> sessions_;
    std::unique_ptr<QStandardItemModel> commands_;
};

} // namespace ZzExample

Q_DECLARE_METATYPE(ZzExample::ZzExampleCommandId)
