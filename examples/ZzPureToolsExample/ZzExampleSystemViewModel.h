#pragma once

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtGui/QStandardItemModel>

namespace ZzExample {

/** @brief 保存系统页由 Presenter 生成的只读名称和值快照。 */
class ZzExampleSystemViewModel final : public QStandardItemModel
{
public:
    /** @brief 创建两列空展示模型。 */
    ZzExampleSystemViewModel();

    /** @brief 释放标准项所有权树。 */
    ~ZzExampleSystemViewModel() override;

    /** @brief 禁止复制 QObject 模型。 */
    ZzExampleSystemViewModel(
        const ZzExampleSystemViewModel &) = delete;

    /** @brief 禁止复制赋值 QObject 模型。 */
    ZzExampleSystemViewModel &operator=(
        const ZzExampleSystemViewModel &) = delete;

    /** @brief 禁止移动具有索引身份的 QObject 模型。 */
    ZzExampleSystemViewModel(ZzExampleSystemViewModel &&) = delete;

    /** @brief 禁止移动赋值具有索引身份的 QObject 模型。 */
    ZzExampleSystemViewModel &operator=(
        ZzExampleSystemViewModel &&) = delete;

    /**
     * @brief 原子替换当前两列展示快照。
     * @param rows 名称和值组成的有序行。
     */
    void setRows(const QList<QPair<QString, QString>> &rows);
};

} // namespace ZzExample
