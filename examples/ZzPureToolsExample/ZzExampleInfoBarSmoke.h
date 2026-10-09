#pragma once
class QWidget;
namespace ZzFluentUI {
class ZzThemeController;
}
namespace ZzExample {

/** @brief 验证信息栏页面的实际编辑、按钮操作与通知接入。 */
class ZzExampleInfoBarSmoke final
{
public:
    /** @brief 验证信息栏页面的实际编辑、按钮操作与通知接入。 */
    static bool isPageReady(const QWidget &window, ZzFluentUI::ZzThemeController *theme);
};

} // namespace ZzExample
