#pragma once

#include <memory>

#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QAbstractItemModel;
class QLineEdit;
class QListView;

namespace ZzFluentUI {

class ZzCommandPalettePrivate;

/** @brief 用于在父工作区内搜索并选择平面命令模型的覆盖面板。 */
class ZZ_FLUENT_UI_EXPORT ZzCommandPalette final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzCommandPalette)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
public:
    /** @brief 创建作为 parent 子控件使用的命令覆盖层。 */
    explicit ZzCommandPalette(QWidget *parent = nullptr);
    /** @brief 关闭覆盖层并安全释放固定内部对象树。 */
    ~ZzCommandPalette() override;
    /** @brief 设置非拥有平面命令模型；销毁时自动清空结果。 */
    void setModel(QAbstractItemModel *model);
    /** @brief 返回当前非拥有命令模型。 */
    [[nodiscard]] QAbstractItemModel *model() const noexcept;
    /** @brief 打开覆盖层，保存当前焦点并将焦点置于搜索框。 */
    void open();
    /** @brief 关闭覆盖层并恢复打开前的焦点。 */
    void close();
    /** @brief 返回覆盖层是否已打开。 */
    [[nodiscard]] bool isOpen() const noexcept;
    /** @brief 设置查询，自动截断到 512 个 UTF-16 code unit。 */
    void setQuery(const QString &query);
    /** @brief 返回规范化前、但已截断的当前查询。 */
    [[nodiscard]] QString query() const;
    /** @brief 返回当前匹配结果数量。 */
    [[nodiscard]] int resultCount() const noexcept;
    /** @brief 返回稳定内部搜索框。 */
    [[nodiscard]] QLineEdit *searchEdit() const noexcept;
    /** @brief 返回稳定内部虚拟化结果视图。 */
    [[nodiscard]] QListView *resultView() const noexcept;
    /** @brief 返回当前结果对应的源模型索引。 */
    [[nodiscard]] QModelIndex currentSourceIndex() const;
    /** @brief 尝试激活当前 enabled 命令；成功时关闭覆盖层。 */
    [[nodiscard]] bool activateCurrent();

Q_SIGNALS:
    /** @brief 源模型变更或销毁后发出。 */
    void modelChanged(QAbstractItemModel *model);
    /** @brief 查询实际变化后发出。 */
    void queryChanged(const QString &query);
    /** @brief 激活 enabled 命令时发出源模型索引。 */
    void commandActivated(const QModelIndex &sourceIndex);
protected:
    /** @brief 处理 Escape 和结果导航键。 */
    bool eventFilter(QObject *watched, QEvent *event) override;
    /** @brief 父级变化时维持覆盖层几何。 */
    void resizeEvent(QResizeEvent *event) override;
private:
    std::unique_ptr<ZzCommandPalettePrivate> d_ptr;
};

} // namespace ZzFluentUI
