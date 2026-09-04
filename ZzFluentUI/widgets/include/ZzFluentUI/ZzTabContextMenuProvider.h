#pragma once

#include <functional>

class QMenu;
class QWidget;

namespace ZzFluentUI {

/** @brief 同步扩展标签上下文菜单，不取得菜单或动作所有权。 */
using ZzTabContextMenuProvider = std::function<void(
    QMenu &menu, int index, QWidget *page)>;

} // namespace ZzFluentUI
