#pragma once

#include <QtCore/QHash>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

#include <ZzFluentUI/ZzSidePaneEdge.h>

class QLabel;
class QStackedWidget;
class QWidget;

namespace ZzFluentUI {

class ZzSidePane;

/** @brief 管理固定标题、页面堆栈、折叠宽度和 4 px 把手。 */
class ZzSidePanePrivate final
{
public:
    /** @brief 创建固定布局和物理边缘对应的把手顺序。 */
    explicit ZzSidePanePrivate(
        ZzSidePane *publicObject,
        ZzSidePaneEdge initialEdge);

    /** @brief 由 QWidget 子对象析构自动清理其余页面。 */
    ~ZzSidePanePrivate();

    /** @brief 在不影响页面所有权的前提下更新把手位置。 */
    void setEdge(ZzSidePaneEdge newEdge);

    /** @brief 在校验父对象后接管页面。 */
    bool addWidget(QWidget *widget, const QString &title);

    /** @brief 解除页面父对象后归还所有权。 */
    [[nodiscard]] QWidget *takeWidget(QWidget *widget);

    /** @brief 只切换已注册页面。 */
    bool setCurrentWidget(QWidget *widget);

    /** @brief 更新标题并转发当前页变化。 */
    void syncCurrentWidget();

    /** @brief 按范围钳制宽度。 */
    [[nodiscard]] int clampWidth(int width) const noexcept;

    /** @brief 将当前展开宽度写入 QWidget 固定宽度。 */
    void applyExpandedWidth();

    /** @brief 使用全局 x 坐标处理把手拖拽。 */
    bool handleResizeDrag(int globalX, bool begin, bool finish);

    ZzSidePane *const q_ptr;
    QWidget *contentHost = nullptr;
    QLabel *titleLabel = nullptr;
    QStackedWidget *stack = nullptr;
    QWidget *resizeHandle = nullptr;
    QHash<QWidget *, QString> pageTitles;
    QHash<QWidget *, QMetaObject::Connection> pageDestroyedConnections;
    QPointer<QWidget> lastNotifiedCurrent;
    ZzSidePaneEdge edge = ZzSidePaneEdge::Left;
    int minimumWidth = 160;
    int maximumWidth = 640;
    int expandedWidth = 280;
    bool collapsed = false;
    bool resizing = false;
    int resizeStartGlobalX = 0;
    int resizeStartWidth = 280;
};

} // namespace ZzFluentUI
