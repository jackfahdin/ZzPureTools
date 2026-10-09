#pragma once

class QWidget;
namespace ZzFluentUI {
class ZzThemeController;
}
namespace ZzExample {

/** @brief 验证时间轴页面的实际编辑、布局切换与交互。 */
class ZzExampleTimelineSmoke final
{
public:
    /** @brief 验证时间轴页面的实际编辑、布局切换与交互。 */
    static bool isPageReady(const QWidget &window, ZzFluentUI::ZzThemeController *theme);
};

} // namespace ZzExample
