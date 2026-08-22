#pragma once

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtGui/QIcon>

#include <ZzFluentUI/ZzIconDescriptor.h>

#include "ZzWidgetTheme.h"

class QStackedWidget;
class QToolButton;
class QWidget;

namespace ZzFluentUI {

class ZzBottomPane;
class ZzPivot;

/** @brief 管理固定工具标题、页面堆栈、折叠高度和 4 px 把手。 */
class ZzBottomPanePrivate final
{
public:
    static constexpr int DefaultMinimumHeight = 120;
    static constexpr int DefaultHeight = 240;
    static constexpr int DefaultMaximumHeight = 640;

    /** @brief 创建固定对象树及所有切换连接。 */
    explicit ZzBottomPanePrivate(ZzBottomPane *publicObject);

    /** @brief 由 QWidget 子对象析构自动清理仍被接管的工具。 */
    ~ZzBottomPanePrivate();

    /** @brief 在校验父对象和重复项后接管工具。 */
    bool addWidget(
        QWidget *widget,
        const QString &title,
        const ZzIconDescriptor &icon);

    /** @brief 解除工具父对象并归还所有权。 */
    [[nodiscard]] QWidget *takeWidget(QWidget *widget);

    /** @brief 只切换已注册工具。 */
    bool setCurrentWidget(QWidget *widget);

    /** @brief 同步外部销毁或显式移除后的 Pivot 项。 */
    void removeWidgetAt(int index);

    /** @brief 在当前工具实际变化时转发公开信号。 */
    void syncCurrentWidget();

    /** @brief 按范围钳制展开高度。 */
    [[nodiscard]] int clampHeight(int height) const noexcept;

    /** @brief 将当前展开高度写入 QWidget 固定高度。 */
    void applyExpandedHeight();

    /** @brief 使用全局 y 坐标处理把手拖拽。 */
    bool handleResizeDrag(int globalY, bool begin, bool finish);

    /** @brief 按图标描述生成供 QTabBar 使用的图标。 */
    [[nodiscard]] QIcon pivotIcon(const ZzIconDescriptor &descriptor) const;

    ZzBottomPane *const q_ptr;
    QWidget *header = nullptr;
    ZzPivot *pivot = nullptr;
    QToolButton *closeButton = nullptr;
    QStackedWidget *stackedWidget = nullptr;
    QWidget *resizeHandle = nullptr;
    ZzWidgetTheme theme;
    QList<QWidget *> widgets;
    QPointer<QWidget> lastNotifiedCurrent;
    int minimumHeight = DefaultMinimumHeight;
    int maximumHeight = DefaultMaximumHeight;
    int expandedHeight = DefaultHeight;
    bool collapsed = false;
    bool resizing = false;
    int resizeStartGlobalY = 0;
    int resizeStartHeight = DefaultHeight;
};

} // namespace ZzFluentUI
