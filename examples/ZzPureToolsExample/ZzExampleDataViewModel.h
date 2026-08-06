#pragma once

#include <memory>

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QString>

#include "ZzExampleDataPageKind.h"

class QStandardItemModel;

namespace ZzExample {

/** @brief 为列表、表格和树页提供有界源模型与筛选代理。 */
class ZzExampleDataViewModel final : public QSortFilterProxyModel
{
public:
    /**
     * @brief 创建指定结构的确定性本地模型。
     * @param kind 数据页面种类。
     */
    explicit ZzExampleDataViewModel(ZzExampleDataPageKind kind);

    /** @brief 释放代理及其独占源模型。 */
    ~ZzExampleDataViewModel() override;

    /** @brief 禁止复制 QObject 模型。 */
    ZzExampleDataViewModel(const ZzExampleDataViewModel &) = delete;

    /** @brief 禁止复制赋值 QObject 模型。 */
    ZzExampleDataViewModel &operator=(
        const ZzExampleDataViewModel &) = delete;

    /** @brief 禁止移动具有模型索引身份的 QObject。 */
    ZzExampleDataViewModel(ZzExampleDataViewModel &&) = delete;

    /** @brief 禁止移动赋值具有模型索引身份的 QObject。 */
    ZzExampleDataViewModel &operator=(
        ZzExampleDataViewModel &&) = delete;

    /**
     * @brief 对全部展示列应用大小写不敏感的固定文本筛选。
     * @param text 用户输入的筛选文本。
     */
    void applyFilter(const QString &text);

    /** @brief 向当前源模型追加一个确定性示例记录。 */
    void appendSample();

    /** @brief 重建当前类型的初始有界数据集。 */
    void resetSamples();

    /** @brief 返回筛选后顶层可见行数。 */
    [[nodiscard]] int visibleRowCount() const;

private:
    /** @brief 构造列表源数据。 */
    void populateList();

    /** @brief 构造表格源数据。 */
    void populateTable();

    /** @brief 构造三层树源数据。 */
    void populateTree();

    ZzExampleDataPageKind kind_ = ZzExampleDataPageKind::List;
    QStandardItemModel *source_ = nullptr;
    int nextSerial_ = 1;
};

} // namespace ZzExample
