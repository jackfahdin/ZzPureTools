#pragma once

#include <memory>

#include <QtWidgets/QStyledItemDelegate>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzItemDensity.h>

namespace ZzFluentUI {

class ZzFluentItemDelegatePrivate;

/**
 * @brief 只按当前 QModelIndex 局部绘制 List、Table 和 Tree 展示项。
 *
 * delegate 不读取 rowCount、不缓存模型指针或索引，也不按模型总行数分配内存。
 */
class ZZ_FLUENT_UI_EXPORT ZzFluentItemDelegate final
    : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFluentItemDelegate)

public:
    /**
     * @brief 创建标准密度 delegate。
     * @param parent 可为空的 QObject 所有者，通常为 QAbstractItemView。
     */
    explicit ZzFluentItemDelegate(QObject *parent = nullptr);

    /** @brief 销毁私有密度状态，不改变 model 所有权。 */
    ~ZzFluentItemDelegate() override;

    /**
     * @brief 设置设备无关 item 高度并请求所属视图重新布局。
     * @param density Standard 为 40，Compact 为 32 逻辑像素。
     */
    void setDensity(ZzItemDensity density);

    /** @brief 返回当前 item 密度。 */
    [[nodiscard]] ZzItemDensity density() const noexcept;

    /**
     * @brief 只初始化和绘制当前 index 的栈上 option 副本。
     * @param painter 非空的当前绘制目标。
     * @param option 当前可见 item 的非拥有样式状态。
     * @param index 当前可见的模型索引。
     */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override;

    /**
     * @brief 返回平台内容宽度和确定的逻辑高度。
     * @param option 当前 item 样式状态。
     * @param index 当前模型索引。
     * @return Standard 高 40，Compact 高 32 的逻辑尺寸。
     */
    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override;

private:
    friend class ZzFluentItemDelegatePrivate;
    std::unique_ptr<ZzFluentItemDelegatePrivate> d_ptr;
};

} // namespace ZzFluentUI
