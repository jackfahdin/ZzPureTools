#pragma once

#include <memory>

#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QAbstractItemModel;
class QLineEdit;
class QToolBar;
class QTreeView;

namespace ZzFluentUI {

class ZzExplorerPanePrivate;

/** @brief 提供递归筛选和源索引映射的轻量资源浏览面板。 */
class ZZ_FLUENT_UI_EXPORT ZzExplorerPane final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzExplorerPane)
    Q_PROPERTY(int searchDelay READ searchDelay WRITE setSearchDelay NOTIFY searchDelayChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
public:
    /** @brief 创建含标题、工具栏、搜索框和虚拟化树视图的资源浏览面板。 */
    explicit ZzExplorerPane(QWidget *parent = nullptr);
    /** @brief 销毁固定数量的内部视图、代理模型和延迟定时器。 */
    ~ZzExplorerPane() override;

    /** @brief 设置非拥有树模型；销毁时自动收敛为空状态。 */
    void setModel(QAbstractItemModel *model);
    /** @brief 返回当前非拥有源模型。 */
    [[nodiscard]] QAbstractItemModel *model() const noexcept;
    /** @brief 返回稳定的内部工具栏，调用方可添加 QAction。 */
    [[nodiscard]] QToolBar *toolBar() const noexcept;
    /** @brief 返回稳定的内部虚拟化树视图。 */
    [[nodiscard]] QTreeView *treeView() const noexcept;
    /** @brief 设置筛选延迟，自动收敛到 0 至 500 毫秒。 */
    void setSearchDelay(int milliseconds);
    /** @brief 返回筛选延迟毫秒数，默认 60。 */
    [[nodiscard]] int searchDelay() const noexcept;
    /** @brief 设置搜索文本；变更由单一持久定时器合并。 */
    void setSearchText(const QString &text);
    /** @brief 返回当前待应用或已应用的搜索文本。 */
    [[nodiscard]] QString searchText() const;
    /** @brief 将内部代理索引映射回源模型索引。 */
    [[nodiscard]] QModelIndex sourceIndex(const QModelIndex &proxyIndex) const;
    /** @brief 将源模型索引映射为当前代理索引。 */
    [[nodiscard]] QModelIndex proxyIndex(const QModelIndex &sourceIndex) const;
    /** @brief 返回树视图当前项对应的源模型索引。 */
    [[nodiscard]] QModelIndex currentSourceIndex() const;
    /** @brief 设置树视图的当前源模型索引。 */
    void setCurrentSourceIndex(const QModelIndex &index);

Q_SIGNALS:
    /** @brief 源模型变更或销毁后发出。 */
    void modelChanged(QAbstractItemModel *model);
    /** @brief 搜索延迟变更后发出。 */
    void searchDelayChanged(int milliseconds);
    /** @brief 搜索文本变更后发出。 */
    void searchTextChanged(const QString &text);
    /** @brief 用户激活有效项时发出源模型索引。 */
    void activated(const QModelIndex &sourceIndex);
    /** @brief 当前项变更时发出源模型索引。 */
    void currentSourceIndexChanged(const QModelIndex &sourceIndex);
private:
    std::unique_ptr<ZzExplorerPanePrivate> d_ptr;
};

} // namespace ZzFluentUI
