#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSidePaneMode.h>

class QWidget;

namespace ZzFluentUI {

class ZzSidePane;
class ZzPanelStack;

/** @brief 管理固定多面板容器、折叠宽度和 4 px 把手。 */
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

    /** @brief 设置页面逻辑可见性并维护多页集合。 */
    bool setWidgetVisible(QWidget *widget, bool visible);

    /** @brief 切换页面展示模式。 */
    void setMode(ZzSidePaneMode newMode);

    /** @brief 转发当前页变化，且同步删除后不解引用裸指针。 */
    void syncCurrentWidget();

    /** @brief 删除失效观察值并保留稳定页面顺序。 */
    void sanitizeStackedVisible();

    /** @brief 按范围钳制宽度。 */
    [[nodiscard]] int clampWidth(int width) const noexcept;

    /** @brief 将展开宽度作为布局偏好，并允许窄宿主压缩到配置下限。 */
    void applyExpandedWidth();

    /** @brief 使用全局 x 坐标处理把手拖拽。 */
    bool handleResizeDrag(int globalX, bool begin, bool finish);

    ZzSidePane *const q_ptr;
    QWidget *contentHost = nullptr;
    ZzPanelStack *panelStack = nullptr;
    QWidget *resizeHandle = nullptr;
    QMetaObject::Connection currentPanelConnection;
    QPointer<QWidget> lastNotifiedCurrent;
    QList<QPointer<QWidget>> stackedVisible;
    ZzSidePaneEdge edge = ZzSidePaneEdge::Left;
    ZzSidePaneMode mode = ZzSidePaneMode::Single;
    int minimumWidth = 160;
    int maximumWidth = 640;
    int expandedWidth = 280;
    bool collapsed = false;
    bool resizing = false;
    int resizeStartGlobalX = 0;
    int resizeStartWidth = 280;
};

} // namespace ZzFluentUI
