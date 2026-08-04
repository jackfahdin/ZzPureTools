#pragma once

#include <memory>

#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;

namespace ZzFluentUI {

class ZzBreadcrumbBarPrivate;

/** @brief 展示少量路径文本并发出原逻辑索引意图的面包屑。 */
class ZZ_FLUENT_UI_EXPORT ZzBreadcrumbBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzBreadcrumbBar)

public:
    /**
     * @brief 创建空路径面包屑。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzBreadcrumbBar(QWidget *parent = nullptr);

    /** @brief 销毁私有状态，展示按钮由 QObject parent 释放。 */
    ~ZzBreadcrumbBar() override;

    /**
     * @brief 替换纯展示路径项并重建少量按钮。
     * @param items 按逻辑根到叶顺序排列的可本地化文本。
     */
    void setItems(QStringList items);

    /** @brief 返回逻辑根到叶顺序的展示文本副本。 */
    [[nodiscard]] QStringList items() const;

    /**
     * @brief 标记当前逻辑项，越界输入收敛为 -1。
     * @param index 原 items 中的逻辑索引。
     */
    void setCurrentIndex(int index);

    /** @brief 返回当前逻辑索引；没有有效项时返回 -1。 */
    [[nodiscard]] int currentIndex() const noexcept;

Q_SIGNALS:
    /**
     * @brief 用户请求导航到原 items 中的逻辑索引。
     * @param index 不受 RTL 视觉顺序影响的原逻辑索引。
     */
    void indexRequested(int index);

protected:
    /** @brief 布局方向变化时重建视觉顺序并保留逻辑索引。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzBreadcrumbBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
