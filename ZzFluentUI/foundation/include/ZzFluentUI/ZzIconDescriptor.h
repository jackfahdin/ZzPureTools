#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTypeInfo>
#include <QtCore/QtTypes>
#include <QtGui/QColor>

#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 标识图标的矢量数据来源。 */
enum class ZzIconSource : quint8
{
    SvgResource,
    FontGlyph,
};

/** @brief 指定图标如何选择最终颜色。 */
enum class ZzIconColorMode : quint8
{
    /** @brief 使用调用控件当前状态的调色板颜色。 */
    Palette,

    /** @brief 使用 descriptor 中的 customColor。 */
    Custom,

    /** @brief 保留 SVG 文件中的原始颜色。 */
    Original,
};

/** @brief 描述可由 Widgets 或未来 Quick 前端渲染的统一图标。 */
struct ZZ_FLUENT_FOUNDATION_EXPORT ZzIconDescriptor final
{
    /** @brief 创建空的 SVG 资源描述。 */
    ZzIconDescriptor() = default;

    /**
     * @brief 创建兼容原有调用方式的 SVG 资源描述。
     * @param resourceId 以 :/ 开头的 Qt Resource 路径。
     * @param mirroredInRightToLeft RTL 布局中是否水平镜像。
     */
    ZzIconDescriptor(
        QString resourceId,
        bool mirroredInRightToLeft = false);

    /** @brief Qt 资源路径或前端可识别的稳定资源标识。 */
    QString resourceId;

    /** @brief 从右到左布局中是否水平镜像。 */
    bool mirroredInRightToLeft = false;

    /** @brief 图标数据来自 SVG 资源或内嵌字体字形。 */
    ZzIconSource source = ZzIconSource::SvgResource;

    /** @brief source 为 FontGlyph 时使用的字形。 */
    ZzFontIcon fontIcon = ZzFontIcon::None;

    /** @brief 选择主题色、自定义色或 SVG 原始色。 */
    ZzIconColorMode colorMode = ZzIconColorMode::Palette;

    /** @brief colorMode 为 Custom 时使用的颜色。 */
    QColor customColor;

    /**
     * @brief 创建任意 Qt Resource SVG 描述。
     * @param resourceId 以 :/ 开头的资源路径。
     * @param mirroredInRightToLeft RTL 布局中是否水平镜像。
     * @param colorMode 颜色选择策略。
     * @param customColor 自定义颜色；仅 Custom 模式使用。
     * @return 可直接交给图标控件或样式渲染的描述。
     */
    [[nodiscard]] static ZzIconDescriptor fromSvgResource(
        QString resourceId,
        bool mirroredInRightToLeft = false,
        ZzIconColorMode colorMode = ZzIconColorMode::Palette,
        QColor customColor = {});

    /**
     * @brief 创建项目内嵌 SVG 描述。
     * @param icon 内嵌 SVG 名称。
     * @param mirroredInRightToLeft RTL 布局中是否水平镜像。
     * @param colorMode 颜色选择策略。
     * @param customColor 自定义颜色；仅 Custom 模式使用。
     * @return 指向稳定 Qt Resource 路径的描述。
     */
    [[nodiscard]] static ZzIconDescriptor fromBundledSvg(
        ZzBundledSvgIcon icon,
        bool mirroredInRightToLeft = false,
        ZzIconColorMode colorMode = ZzIconColorMode::Palette,
        QColor customColor = {});

    /**
     * @brief 创建 ZzAwesome 字体字形描述。
     * @param icon 字体图标。
     * @param mirroredInRightToLeft RTL 布局中是否水平镜像。
     * @param colorMode 主题色或自定义色；Original 按 Palette 处理。
     * @param customColor 自定义颜色；仅 Custom 模式使用。
     * @return 不依赖文件路径的字体图标描述。
     */
    [[nodiscard]] static ZzIconDescriptor fromFontIcon(
        ZzFontIcon icon,
        bool mirroredInRightToLeft = false,
        ZzIconColorMode colorMode = ZzIconColorMode::Palette,
        QColor customColor = {});
};

} // namespace ZzFluentUI

Q_DECLARE_TYPEINFO(ZzFluentUI::ZzIconDescriptor, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzFluentUI::ZzIconSource)
Q_DECLARE_METATYPE(ZzFluentUI::ZzIconColorMode)
Q_DECLARE_METATYPE(ZzFluentUI::ZzIconDescriptor)
