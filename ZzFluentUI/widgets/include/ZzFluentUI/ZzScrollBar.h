#pragma once

#include <memory>

#include <QtWidgets/QScrollBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEnterEvent;
class QEvent;
class QHideEvent;

namespace ZzFluentUI {

class ZzFluentStylePrivate;
class ZzScrollBarPrivate;

/**
 * @brief 保留 QScrollBar 范围、输入和无障碍语义的 Fluent 滚动条。
 *
 * 本类只增加不改变布局的悬停呈现动画。范围、值、步长、滚轮、
 * 触控板、键盘、拖动、上下文菜单和辅助技术行为均由 QScrollBar 提供。
 */
class ZZ_FLUENT_UI_EXPORT ZzScrollBar final : public QScrollBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzScrollBar)

public:
    /**
     * @brief 创建垂直 Fluent 滚动条。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzScrollBar(QWidget *parent = nullptr);

    /**
     * @brief 创建指定方向的 Fluent 滚动条。
     * @param orientation 水平或垂直方向。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzScrollBar(
        Qt::Orientation orientation,
        QWidget *parent = nullptr);

    /** @brief 停止并销毁唯一持久呈现动画。 */
    ~ZzScrollBar() override;

protected:
    /** @brief 指针进入时展开滑块视觉，不改变范围和值。 */
    void enterEvent(QEnterEvent *event) override;

    /** @brief 指针离开时收拢滑块视觉。 */
    void leaveEvent(QEvent *event) override;

    /** @brief 隐藏前停止动画，避免后台唤醒。 */
    void hideEvent(QHideEvent *event) override;

    /** @brief 在启用、样式和 palette 变化时同步呈现终态。 */
    void changeEvent(QEvent *event) override;

private:
    friend class ZzFluentStylePrivate;
    std::unique_ptr<ZzScrollBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
