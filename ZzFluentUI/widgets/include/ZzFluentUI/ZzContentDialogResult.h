#pragma once

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 描述内容对话框关闭时对应的用户操作。 */
enum class ZzContentDialogResult
{
    None = -1,
    Close = 0,
    Primary = 1,
    Secondary = 2
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzContentDialogResult)
