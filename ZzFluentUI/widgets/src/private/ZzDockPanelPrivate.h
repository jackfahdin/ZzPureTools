#pragma once

#include <ZzFluentUI/ZzIconDescriptor.h>

class QLabel;
class QWidget;

namespace ZzFluentUI {

class ZzDockPanel;
class ZzIconButton;

/** @brief 管理稳定 Dock 标题栏子控件及原生浮动、关闭意图。 */
class ZzDockPanelPrivate final
{
public:
    /** @brief 创建图标、标题、浮动和关闭控件并连接 Qt 原生协议。 */
    explicit ZzDockPanelPrivate(ZzDockPanel *publicObject);

    /** @brief 按 Dock features 和浮动状态刷新标题栏。 */
    void refreshPresentation();

    /** @brief 使用 Qt setFloating 在浮动与原停靠位置之间切换。 */
    void toggleFloating();

    ZzDockPanel *const q_ptr;
    QWidget *titleBar = nullptr;
    ZzIconButton *iconWidget = nullptr;
    QLabel *titleLabel = nullptr;
    ZzIconButton *floatButton = nullptr;
    ZzIconButton *closeButton = nullptr;
    ZzIconDescriptor iconDescriptor;
};

} // namespace ZzFluentUI
