#pragma once

#include <memory>

#include <QtWidgets/QListView>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QKeyEvent;
class QEvent;

namespace ZzFluentUI {

class ZzNavigationViewPrivate;

/**
 * @brief 只消费 QAbstractItemModel 展示数据并转发导航意图的列表视图。
 *
 * 控件不创建页面、不访问领域对象，宿主根据 navigationRequested 决定路由行为。
 */
class ZZ_FLUENT_UI_EXPORT ZzNavigationView final : public QListView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzNavigationView)
    Q_PROPERTY(
        bool compact
        READ isCompact
        WRITE setCompact
        NOTIFY compactChanged)

public:
    /**
     * @brief 创建批量布局且统一 item 尺寸的导航视图。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzNavigationView(QWidget *parent = nullptr);

    /** @brief 销毁私有状态，model 继续遵循 Qt Model/View 所有权。 */
    ~ZzNavigationView() override;

    /** @brief 返回是否只展示图标的紧凑模式。 */
    [[nodiscard]] bool isCompact() const noexcept;

    /**
     * @brief 切换 48 或 240 逻辑像素宽度，不修改 model。
     * @param compact 为 true 时隐藏 delegate 展示文本。
     */
    void setCompact(bool compact);

Q_SIGNALS:
    /** @brief 紧凑模式实际变化后发出。 */
    void compactChanged(bool compact);

    /**
     * @brief 用户激活有效且启用的展示索引时发出。
     * @param index 非拥有持久模型中的临时 QModelIndex。
     */
    void navigationRequested(const QModelIndex &index);

protected:
    /** @brief 把 Enter、Return 和 Space 转换为导航意图，其他键保留列表行为。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 在紧凑模式下用完整 DisplayRole 补全缺失的 tooltip。 */
    bool viewportEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzNavigationViewPrivate> d_ptr;
};

} // namespace ZzFluentUI
