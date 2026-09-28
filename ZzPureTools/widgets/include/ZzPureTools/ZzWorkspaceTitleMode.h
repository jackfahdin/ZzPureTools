#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzPureTools {

/** @brief 指定工作区宿主和 Fluent 标题栏的标题组合策略。 */
enum class ZzWorkspaceTitleMode : std::uint8_t
{
    /** @brief 只显示应用标题。 */
    Application,
    /** @brief 显示当前页面标题；Tabbed 模式可回退标签文字，无页面时回退应用标题。 */
    CurrentTab,
    /** @brief 显示“当前页面 - 应用标题”，支持堆叠页面与标签页面。 */
    CurrentTabAndApplication,
    /** @brief 显示显式自定义标题，空值时回退应用标题。 */
    Custom
};

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceTitleMode)
