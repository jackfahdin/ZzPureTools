#pragma once

#include <QtCore/QPoint>
#include <QtCore/QString>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QWidget;

namespace ZzFluentUI {

/**
 * @brief 主题感知的轻量文本提示（QToolTip 的替代实现）。
 *
 * 颜色取自应用调色板的 ToolTipBase/ToolTipText/Mid 角色，随
 * QApplication::setPalette 自动跟随主题切换；控件为半透明顶层窗口，
 * 文字强制走灰度抗锯齿，规避部分平台（如 KDE Wayland）上 QTipLabel
 * 子像素抗锯齿合成后文字发灰的问题。
 *
 * 行为对齐 QToolTip：再次 showText 复用并移动同一实例；鼠标按下、
 * 滚轮、非修饰键按键或 10 秒超时自动关闭。
 */
class ZZ_FLUENT_UI_EXPORT ZzToolTip final
{
public:
    ZzToolTip() = delete;

    /** @brief 在全局坐标 @a globalPos 附近（光标右下方）显示文本提示；
     *  @a parent 销毁时提示一并关闭。@a text 为空等价于 hideText()。 */
    static void showText(const QPoint &globalPos, const QString &text, QWidget *parent);

    /** @brief 立即关闭当前提示（无实例时为空操作）。 */
    static void hideText();

    /** @brief 当前是否有可见的提示。 */
    static bool isVisible();
};

} // namespace ZzFluentUI
