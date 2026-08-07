#pragma once

#include <QtGui/QColor>

#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzIconButton;

/** @brief 保存图标描述并通过当前 Fluent 样式刷新缓存图像。 */
class ZzIconButtonPrivate final
{
public:
    /** @brief 绑定非空 public 对象。 */
    explicit ZzIconButtonPrivate(
        ZzIconButton *publicObject) noexcept;

    /** @brief 使用当前尺寸、DPR、颜色和布局方向刷新图标。 */
    void refreshIcon();

    ZzIconButton *q_ptr = nullptr;
    ZzIconDescriptor descriptor;
    QColor iconColor;
    bool hasDescriptor = false;
};

} // namespace ZzFluentUI
