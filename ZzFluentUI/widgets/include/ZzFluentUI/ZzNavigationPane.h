#pragma once

#include <memory>

#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzNavigationDisplayMode.h>

class QAbstractItemModel;
class QEvent;
class QTreeView;

namespace ZzFluentUI {

class ZzNavigationPanePrivate;

/**
 * @brief 将一个平面展示模型投影为主导航、分区和固定页脚的 Fluent 面板。
 *
 * model 是非拥有观察值；控件不创建页面、不执行路由，也不访问业务对象。
 * 全部方法只能在 GUI 线程调用。
 */
class ZZ_FLUENT_UI_EXPORT ZzNavigationPane final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzNavigationPane)
    Q_PROPERTY(
        ZzFluentUI::ZzNavigationDisplayMode displayMode
        READ displayMode
        WRITE setDisplayMode
        NOTIFY displayModeChanged)
    Q_PROPERTY(
        bool compact
        READ isCompact
        NOTIFY effectiveCompactChanged)
    Q_PROPERTY(
        int adaptiveThreshold
        READ adaptiveThreshold
        WRITE setAdaptiveThreshold
        NOTIFY adaptiveThresholdChanged)

public:
    /**
     * @brief 创建使用自适应模式的空导航面板。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzNavigationPane(QWidget *parent = nullptr);

    /** @brief 解除顶层窗口观察并销毁固定数量的内部视图和投影。 */
    ~ZzNavigationPane() override;

    /**
     * @brief 设置非拥有的平面展示模型。
     * @param model 可为空；调用方必须使非空模型与面板同属 GUI 线程。
     */
    void setModel(QAbstractItemModel *model);

    /** @brief 返回当前非拥有 source model；模型销毁后返回 nullptr。 */
    [[nodiscard]] QAbstractItemModel *model() const noexcept;

    /** @brief 设置常规、紧凑或自适应展示策略。 */
    void setDisplayMode(ZzNavigationDisplayMode mode);

    /** @brief 返回当前展示策略。 */
    [[nodiscard]] ZzNavigationDisplayMode displayMode() const noexcept;

    /** @brief 返回当前实际使用的紧凑状态。 */
    [[nodiscard]] bool isCompact() const noexcept;

    /**
     * @brief 设置自适应模式切换到紧凑状态的顶层窗口宽度。
     * @param logicalWidth 逻辑像素宽度，自动收敛到 480 至 4096。
     */
    void setAdaptiveThreshold(int logicalWidth);

    /** @brief 返回自适应逻辑宽度阈值。 */
    [[nodiscard]] int adaptiveThreshold() const noexcept;

    /**
     * @brief 按 source model 索引同步主区或页脚的唯一当前项。
     * @param index 当前 model 的 column 0 顶层索引；无效值清除选择。
     */
    void setCurrentSourceIndex(const QModelIndex &index);

    /** @brief 返回最近一次同步或激活的 source model 索引。 */
    [[nodiscard]] QModelIndex currentSourceIndex() const;

    /**
     * @brief 切换为按分区展示的纵向 Tree View 导航。
     * @param enabled 为 true 时隐藏旧的主区/页脚列表并显示树视图。
     */
    void setTreeMode(bool enabled);

    /** @brief 返回是否启用了纵向 Tree View 导航模式。 */
    [[nodiscard]] bool isTreeMode() const noexcept;

    /** @brief 返回 Tree View 模式使用的非拥有树视图。 */
    [[nodiscard]] QTreeView *treeView() const noexcept;

Q_SIGNALS:
    /** @brief 非拥有 source model 实际变化或被销毁后发出。 */
    void modelChanged(QAbstractItemModel *model);

    /** @brief 展示策略实际变化后发出。 */
    void displayModeChanged(ZzNavigationDisplayMode mode);

    /** @brief 自适应计算得到的实际紧凑状态变化后发出。 */
    void effectiveCompactChanged(bool compact);

    /** @brief 自适应宽度阈值实际变化后发出。 */
    void adaptiveThresholdChanged(int logicalWidth);

    /**
     * @brief 用户激活主区或页脚中的有效目标时发出 source index。
     * @param sourceIndex 当前非拥有 model 中的临时顶层索引。
     */
    void navigationRequested(const QModelIndex &sourceIndex);

    /** @brief Tree View 导航模式实际变化后发出。 */
    void treeModeChanged(bool enabled);

protected:
    /** @brief 在父级或显示状态变化后重新绑定自适应顶层窗口。 */
    bool event(QEvent *event) override;

    /** @brief 只观察当前顶层窗口的尺寸和状态变化。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class ZzNavigationPanePrivate;
    std::unique_ptr<ZzNavigationPanePrivate> d_ptr;
};

} // namespace ZzFluentUI
