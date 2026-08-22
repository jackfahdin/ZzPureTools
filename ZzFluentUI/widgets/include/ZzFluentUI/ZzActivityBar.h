#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>

class QAbstractItemModel;
class QEvent;

namespace ZzFluentUI {

class ZzActivityBarPrivate;

/**
 * @brief 将一个平面模型投影为固定主次两组的活动入口栏。
 *
 * 每行通过 Qt::DisplayRole 提供标题，通过 Qt::DecorationRole 提供
 * ZzIconDescriptor，并通过 ZzActivityItemRole 提供分组与 badge。
 * model 是非拥有观察值。组件只发出激活、折叠和移动意图，绝不修改外部模型；
 * 调用方必须在 GUI 线程中设置模型并提交后续业务状态。
 */
class ZZ_FLUENT_UI_EXPORT ZzActivityBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzActivityBar)
    Q_PROPERTY(
        ZzFluentUI::ZzSidePaneEdge edge
        READ edge
        WRITE setEdge
        NOTIFY edgeChanged)
    Q_PROPERTY(
        bool multiActiveEnabled
        READ isMultiActiveEnabled
        WRITE setMultiActiveEnabled
        NOTIFY multiActiveEnabledChanged)

public:
    /**
     * @brief 创建投影指定物理侧的空 Activity Bar。
     * @param edge 只展示该侧的 Primary 与 Secondary 两组。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzActivityBar(
        ZzSidePaneEdge edge = ZzSidePaneEdge::Left,
        QWidget *parent = nullptr);

    /** @brief 解除外部模型观察并销毁固定数量的内部视图和投影。 */
    ~ZzActivityBar() override;

    /** @brief 返回当前物理侧。 */
    [[nodiscard]] ZzSidePaneEdge edge() const noexcept;

    /**
     * @brief 切换物理侧并重建两个固定投影，不修改源模型的 Area 数据。
     * @param edge 新的物理侧；RTL 不改变此值的语义。
     */
    void setEdge(ZzSidePaneEdge edge);

    /**
     * @brief 设置非拥有的活动模型。
     * @param model 可为空；模型销毁后组件自动清空投影和当前索引。
     */
    void setModel(QAbstractItemModel *model);

    /** @brief 返回当前非拥有模型；模型销毁后返回 nullptr。 */
    [[nodiscard]] QAbstractItemModel *model() const noexcept;

    /**
     * @brief 从 Shell 同步当前源索引。
     * @param index 当前模型的 column 0 顶层索引；不属于当前两组时清空选择。
     */
    void setCurrentSourceIndex(const QModelIndex &index);

    /** @brief 返回最近同步或由用户激活的源模型索引。 */
    [[nodiscard]] QModelIndex currentSourceIndex() const;

    /** @brief 返回是否显示多个活动入口指示条。 */
    [[nodiscard]] bool isMultiActiveEnabled() const noexcept;

    /** @brief 启用或关闭多个活动入口指示条。 */
    void setMultiActiveEnabled(bool enabled);

    /** @brief 返回按输入顺序去重后的当前有效活动源索引。 */
    [[nodiscard]] QList<QModelIndex> activeSourceIndexes() const;

    /** @brief 设置当前模型和物理侧投影内的活动源索引集合。 */
    void setActiveSourceIndexes(const QList<QModelIndex> &indexes);

Q_SIGNALS:
    /** @brief 物理侧实际变化后发出。 */
    void edgeChanged(ZzSidePaneEdge edge);

    /** @brief 非拥有模型变化或被销毁后发出。 */
    void modelChanged(QAbstractItemModel *model);

    /** @brief 当前源索引变化后发出。 */
    void currentSourceIndexChanged(const QModelIndex &sourceIndex);

    /** @brief 多活动指示条开关实际变化后发出。 */
    void multiActiveEnabledChanged(bool enabled);

    /** @brief 活动源索引集合实际变化后发出。 */
    void activeSourceIndexesChanged(const QList<QModelIndex> &sourceIndexes);

    /** @brief 用户激活非当前有效入口时发出，不执行业务路由。 */
    void activationRequested(const QModelIndex &sourceIndex);

    /** @brief 用户再次激活当前有效入口时发出，由 Shell 决定是否折叠面板。 */
    void collapseRequested(const QModelIndex &sourceIndex);

    /**
     * @brief 接收到经进程内令牌验证的拖放后发出移动意图。
     * @param sourceIndex 非拥有源模型索引。
     * @param targetArea 目标分组。
     * @param targetRow 目标分组中的插入行。
     */
    void moveRequested(
        const QModelIndex &sourceIndex,
        ZzActivityArea targetArea,
        int targetRow);

protected:
    /** @brief 处理固定两个视图的键盘和进程内拖放事件。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class ZzActivityBarPrivate;
    std::unique_ptr<ZzActivityBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
