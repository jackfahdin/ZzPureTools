#pragma once

#include <QtCore/QMimeData>
#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtCore/QRect>

class QDragMoveEvent;
class QMouseEvent;
class QWidget;

namespace ZzFluentUI {

class ZzTabBar;
class ZzTabWidget;

/** @brief 保存仅限本进程使用的安全标签拖拽引用。 */
class ZzTabMimeData final : public QMimeData
{
public:
    /** @brief 创建带版本标识和受保护 QObject 引用的载荷。 */
    ZzTabMimeData(
        ZzTabWidget *source,
        QWidget *page,
        int sourceIndex);

    /** @brief 返回固定、版本化且不包含地址的 MIME 格式。 */
    [[nodiscard]] static QString format();

    QPointer<ZzTabWidget> source;
    QPointer<QWidget> page;
    int sourceIndex = -1;
};

/** @brief 持有标签栏能力开关和一次拖拽的短生命周期状态。 */
class ZzTabBarPrivate final
{
public:
    /** @brief 绑定公开对象，不取得 QObject 所有权。 */
    explicit ZzTabBarPrivate(ZzTabBar *q) noexcept;

    /** @brief 绑定唯一页面宿主。 */
    void setHost(ZzTabWidget *hostWidget) noexcept;

    /** @brief 清理按下状态，不改变当前页面。 */
    void clearPressState() noexcept;

    /** @brief 启动一次 Qt MoveAction 并在外部释放时发出拖出意图。 */
    void startDrag();

    /** @brief 校验 MIME 类型、来源、页面和当前宿主能力。 */
    [[nodiscard]] const ZzTabMimeData *validPayload(
        const QMimeData *mimeData) const noexcept;

    /** @brief 返回布局方向感知的目标插入槽位。 */
    [[nodiscard]] int insertionIndex(const QPoint &position) const;

    /** @brief 返回当前插入槽位对应的指示线。 */
    [[nodiscard]] QRect insertionIndicatorRect() const;

    ZzTabBar *const q_ptr;
    QPointer<ZzTabWidget> host;
    QPointer<QWidget> pressedPage;
    QPoint pressPosition;
    int pressedIndex = -1;
    int dropIndex = -1;
    bool tearOffEnabled = true;
    bool tabTransferEnabled = true;
    bool dragging = false;
};

} // namespace ZzFluentUI
