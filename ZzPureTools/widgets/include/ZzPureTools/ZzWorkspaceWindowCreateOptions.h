#pragma once

#include <cstdint>

#include <QtCore/QMetaType>
#include <QtCore/QPointer>

#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>

namespace ZzPureTools {

class ZzApplicationWindow;

/** @brief 指定创建窗口时完整配置的来源。 */
enum class ZzWorkspaceConfigurationSource : std::uint8_t
{
    /** @brief 使用协调器提供的默认配置。 */
    CoordinatorDefaults,
    /** @brief 从来源窗口复制配置。 */
    SourceWindow,
    /** @brief 使用调用方显式提供的配置。 */
    Explicit
};

/** @brief 指定新建应用窗口的初始可见性。 */
enum class ZzApplicationWindowVisibility : std::uint8_t
{
    /** @brief 创建后立即可见。 */
    Visible,
    /** @brief 创建后暂不显示。 */
    Deferred
};

/** @brief 描述创建工作区窗口所需的来源、覆盖配置和展示选项。 */
struct ZzWorkspaceWindowCreateOptions final
{
    /** @brief 完整配置的选择来源。 */
    ZzWorkspaceConfigurationSource configurationSource =
        ZzWorkspaceConfigurationSource::CoordinatorDefaults;
    /** @brief 复制配置时使用的非拥有来源窗口观察值。 */
    QPointer<ZzApplicationWindow> sourceWindow;
    /** @brief 应用于来源配置的字段级覆盖值。 */
    ZzWorkspaceWindowConfigurationPatch configuration;
    /** @brief 新窗口的初始可见性。 */
    ZzApplicationWindowVisibility visibility =
        ZzApplicationWindowVisibility::Visible;
    /** @brief 是否在创建完成后激活窗口。 */
    bool activate = true;
};

} // namespace ZzPureTools

Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceConfigurationSource)
Q_DECLARE_METATYPE(ZzPureTools::ZzApplicationWindowVisibility)
Q_DECLARE_METATYPE(ZzPureTools::ZzWorkspaceWindowCreateOptions)
