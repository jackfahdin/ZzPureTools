#pragma once

#include <QtWidgets/QStyleOptionButton>

#include <ZzFluentUI/ZzButtonAppearance.h>

namespace ZzFluentUI {

class ZzPushButton;

/** @brief 保存按钮外观并构造无业务数据的绘制 option。 */
class ZzPushButtonPrivate final
{
public:
    /** @brief 绑定非空 public 对象。 */
    explicit ZzPushButtonPrivate(
        ZzPushButton *publicObject) noexcept;

    /** @brief 复制 QPushButton 原生状态并覆盖外观调色板。 */
    void initStyleOption(QStyleOptionButton *option) const;

    ZzPushButton *q_ptr = nullptr;
    ZzButtonAppearance appearance = ZzButtonAppearance::Standard;
};

} // namespace ZzFluentUI
