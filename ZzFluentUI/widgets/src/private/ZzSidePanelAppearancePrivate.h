#pragma once

class QLayout;
class QTreeView;

namespace ZzFluentUI {

/** @brief 集中实现侧面板内容外观，不保存控件指针或建立主题连接。 */
class ZzSidePanelAppearancePrivate final
{
public:
    /** @brief 配置树视图并复用已经安装的 Fluent 委托。 */
    static void applyTreeView(QTreeView *view);

    /** @brief 配置表单内容的统一边距。 */
    static void applyFormLayout(QLayout *layout);
};

} // namespace ZzFluentUI
