#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>

class QLayout;
class QTreeView;

namespace ZzFluentUI {

/** @brief 侧面板内容的无状态外观配置入口；仅在 GUI 线程调用。 */
class ZZ_FLUENT_UI_EXPORT ZzSidePanelAppearance final
{
public:
    /** @brief 禁止实例化无状态外观配置工具。 */
    ZzSidePanelAppearance() = delete;

    /**
     * @brief 应用无边框树视图、统一 Fluent 条目与滚动外观。
     * @param view 非拥有树视图指针；空指针忽略。
     * @note 安装或复用 ZzFluentItemDelegate，统一标准行高、整行选择和逐像素滚动。
     * 保留模型、当前选择、列宽、表头、编辑策略与节点展开状态。
     * 自定义委托应在此调用之后安装；重复应用会重新选用 Fluent 委托。
     */
    static void applyTreeView(QTreeView *view);

    /**
     * @brief 应用表单型侧面板的统一内容边距与间距。
     * @param layout 非拥有布局指针；空指针忽略。树视图宿主应使用零边距。
     */
    static void applyFormLayout(QLayout *layout);
};

} // namespace ZzFluentUI
