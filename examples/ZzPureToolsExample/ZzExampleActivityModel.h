#pragma once

#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QString>

namespace ZzExample {

class ZzExampleActivityModelPrivate;

/** @brief 保存跨窗口共享且有界的只读活动展示记录。 */
class ZzExampleActivityModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief 创建具有固定容量的空活动模型。
     * @param capacity 最大保留记录数，至少为一。
     * @param parent 可选 QObject 所有者。
     */
    explicit ZzExampleActivityModel(
        qsizetype capacity = 200,
        QObject *parent = nullptr);

    /** @brief 释放有界字符串记录。 */
    ~ZzExampleActivityModel() override;

    /** @brief 禁止复制 QObject 模型。 */
    ZzExampleActivityModel(const ZzExampleActivityModel &) = delete;

    /** @brief 禁止复制赋值 QObject 模型。 */
    ZzExampleActivityModel &operator=(
        const ZzExampleActivityModel &) = delete;

    /** @brief 禁止移动具有模型索引身份的 QObject。 */
    ZzExampleActivityModel(ZzExampleActivityModel &&) = delete;

    /** @brief 禁止移动赋值具有模型索引身份的 QObject。 */
    ZzExampleActivityModel &operator=(
        ZzExampleActivityModel &&) = delete;

    /** @brief 返回顶层活动记录数。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = {}) const override;

    /** @brief 返回活动展示文本与可访问文本。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    /**
     * @brief 追加一条非空展示记录并按容量移除最旧项。
     * @param text 已格式化且不包含业务对象引用的活动文本。
     */
    void append(QString text);

    /** @brief 清空全部活动记录。 */
    void clear();

private:
    std::unique_ptr<ZzExampleActivityModelPrivate> d_ptr;
};

} // namespace ZzExample
