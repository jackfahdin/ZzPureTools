#pragma once

#include <QtGui/QColor>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzProgressBarAppearance.h>

class QAbstractButton;
class QProgressBar;
class QWidget;

namespace ZzFluentUI {

/** @brief 统一设置原生 Qt 与 Fluent 控件的外观；所有方法在 GUI 线程调用。 */
class ZZ_FLUENT_UI_EXPORT ZzControlAppearance final
{
public:
    /**
     * @brief 设置使用 ZzFluentStyle 的线性进度条外观并请求重绘。
     * @param progress 非拥有进度条指针；空指针忽略，不影响其他控件。
     * @param appearance 细线或粗线；未知值按 Thin 处理。
     * @note 不改变尺寸、范围、值、文字或动画。环形 ZzProgressRing 不使用此设置。
     */
    static void setProgressBarAppearance(QProgressBar *progress, ZzProgressBarAppearance appearance);

    /**
     * @brief 查询线性进度条外观。
     * @param progress 非拥有进度条指针，可以为空。
     * @return 当前外观；空指针或未设置时返回 Thin。
     */
    [[nodiscard]] static ZzProgressBarAppearance progressBarAppearance(const QProgressBar *progress);

    /**
     * @brief 设置按钮外观；不改变 checked、default、点击或菜单语义。
     * @param button 非拥有按钮指针；空指针忽略。
     * @param appearance 标准、强调色或轻量外观。
     */
    static void setButtonAppearance(QAbstractButton *button, ZzButtonAppearance appearance);

    /**
     * @brief 查询显式按钮外观。
     * @param button 非拥有按钮指针，可以为空。
     * @return 显式外观；空指针返回 Standard。
     */
    [[nodiscard]] static ZzButtonAppearance buttonAppearance(const QAbstractButton *button);

    /**
     * @brief 设置控件局部强调色，同时同步 Accent、Highlight 与前景角色。
     * @param widget 非拥有控件指针；子控件遵循 Qt 原有 palette 继承规则。
     * @param color 有效颜色覆盖全局主题；无效颜色等同 resetAccentColor。
     * @note 不改变禁用态配色，不改变背景、字体或尺寸。
     */
    static void setAccentColor(QWidget *widget, const QColor &color);

    /**
     * @brief 仅清除强调色角色的局部覆盖，恢复父级或应用主题；保留其他 palette 角色。
     * @param widget 非拥有控件指针；空指针忽略。
     */
    static void resetAccentColor(QWidget *widget);

    /**
     * @brief 查询当前有效强调色。
     * @param widget 非拥有控件指针，可以为空。
     * @return 当前 palette 中的有效强调色；空指针返回无效颜色。
     */
    [[nodiscard]] static QColor accentColor(const QWidget *widget);
};

} // namespace ZzFluentUI
