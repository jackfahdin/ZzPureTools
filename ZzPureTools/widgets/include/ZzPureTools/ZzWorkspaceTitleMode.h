#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzPureTools {

/** @brief 指定工作区宿主和 Fluent 标题栏的标题组合策略。 */
enum class ZzWorkspaceTitleMode : std::uint8_t
{
    /** @brief 只显示应用标题。 */
    Application,
    /** @brief 显示当前标签标题，无标签时回退应用标题。 */
    CurrentTab,
    /** @brief 显示“当前标签 - 应用标题”。 */
    CurrentTabAndApplication,
    /** @brief 显示显式自定义标题，空值时回退应用标题。 */
    Custom
};

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceTitleMode)
