#pragma once

#include <cstdint>
#include <memory>

#include <QtCore/QList>
#include <QtGui/QIcon>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QAction;
class QEvent;
class QResizeEvent;

namespace ZzFluentUI {

/** @brief 定义命令栏按可用宽度展示 action 的策略。 */
enum class ZzCommandBarDisplayMode : std::uint8_t {
    Auto,
    Compact,
    Expanded
};

class ZzCommandBarPrivate;

/**
 * @brief 按宽度在工具栏与更多菜单间迁移原生 QAction 的命令栏。
 *
 * 外部 QAction 始终由调用方拥有，命令栏只观察其生命周期；便利重载
 * 创建的 QAction 则由命令栏拥有。每个命令仅以同一 QAction 实例展示，
 * 因而 checked、enabled、快捷键和子菜单状态不会在 overflow 中分叉。
 */
class ZZ_FLUENT_UI_EXPORT ZzCommandBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzCommandBar)
    Q_PROPERTY(
        ZzCommandBarDisplayMode displayMode
        READ displayMode
        WRITE setDisplayMode
        NOTIFY displayModeChanged)

public:
    /**
     * @brief 创建空命令栏。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzCommandBar(QWidget *parent = nullptr);

    /** @brief 销毁固定展示控件和私有 action 观察状态。 */
    ~ZzCommandBar() override;

    /** @brief 返回主命令的逻辑顺序，不含已析构 action。 */
    [[nodiscard]] QList<QAction *> primaryActions() const;

    /** @brief 返回次要命令的逻辑顺序，不含已析构 action。 */
    [[nodiscard]] QList<QAction *> secondaryActions() const;

    /** @brief 返回当前按宽度选择的展示策略。 */
    [[nodiscard]] ZzCommandBarDisplayMode displayMode() const noexcept;

    /**
     * @brief 将外部拥有的 action 插入主命令组。
     * @param index 逻辑插入位置，范围为 0 到 primaryActions().size()。
     * @param action 非空且尚未属于任一命令组的原生 QAction。
     * @return 插入成功返回 true；重复、跨组或越界输入返回 false。
     */
    bool insertPrimaryAction(int index, QAction *action);

    /**
     * @brief 创建由命令栏拥有的主命令并插入指定位置。
     * @param index 逻辑插入位置，范围为 0 到 primaryActions().size()。
     * @param icon 命令图标，可为空。
     * @param text 可本地化的命令文本。
     * @return 新 action；越界时返回 nullptr。
     */
    QAction *insertPrimaryAction(int index, const QIcon &icon, QString text);

    /**
     * @brief 将外部拥有的 action 插入次要命令组。
     * @param index 逻辑插入位置，范围为 0 到 secondaryActions().size()。
     * @param action 非空且尚未属于任一命令组的原生 QAction。
     * @return 插入成功返回 true；重复、跨组或越界输入返回 false。
     */
    bool insertSecondaryAction(int index, QAction *action);

    /**
     * @brief 创建由命令栏拥有的次要命令并插入指定位置。
     * @param index 逻辑插入位置，范围为 0 到 secondaryActions().size()。
     * @param icon 命令图标，可为空。
     * @param text 可本地化的命令文本。
     * @return 新 action；越界时返回 nullptr。
     */
    QAction *insertSecondaryAction(int index, const QIcon &icon, QString text);

    /**
     * @brief 从所在命令组移除 action，但不删除外部拥有的对象。
     * @param action 当前属于主或次要命令组的 QAction。
     * @return 找到并移除时返回 true。
     */
    bool removeAction(QAction *action);

public Q_SLOTS:
    /** @brief 设置展示策略并立即按当前宽度重建展示。 */
    void setDisplayMode(ZzCommandBarDisplayMode mode);

Q_SIGNALS:
    /** @brief 展示策略实际变化后发出。 */
    void displayModeChanged(ZzCommandBarDisplayMode mode);

protected:
    /** @brief 尺寸变化时复用缓存宽度重新选择 overflow。 */
    void resizeEvent(QResizeEvent *event) override;

    /** @brief 字体、样式、调色板和布局方向变化时使宽度缓存失效。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzCommandBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
