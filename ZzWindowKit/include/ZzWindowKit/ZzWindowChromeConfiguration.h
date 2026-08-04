#pragma once

#include <QtCore/QList>

class QWidget;

namespace ZzWindowKit {

/**
 * @brief 描述一次完整的无边框标题栏绑定。
 *
 * 所有指针均为非拥有输入；调用完成后由代理使用 QPointer 跟踪宿主窗口。每次更换
 * titleBar 都必须重新提供全部系统按钮与交互控件。
 */
struct ZzWindowChromeConfiguration final
{
    /** @brief 宿主窗口内的自定义标题栏，不能为空。 */
    QWidget *titleBar = nullptr;
    /** @brief 可选的窗口图标控件。 */
    QWidget *windowIcon = nullptr;
    /** @brief 可选的最小化按钮。 */
    QWidget *minimizeButton = nullptr;
    /** @brief 可选的最大化或恢复按钮。 */
    QWidget *maximizeButton = nullptr;
    /** @brief 可选的关闭按钮。 */
    QWidget *closeButton = nullptr;
    /** @brief 标题栏内不参与窗口拖动的交互控件。 */
    QList<QWidget *> interactiveWidgets;
};

} // namespace ZzWindowKit
