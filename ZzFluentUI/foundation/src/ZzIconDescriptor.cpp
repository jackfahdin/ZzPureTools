#include <ZzFluentUI/ZzIconDescriptor.h>

#include <utility>

namespace ZzFluentUI {

namespace {

/** @brief 返回内嵌 SVG 的稳定 Qt Resource 路径。 */
QString zzBundledSvgResourceId(ZzBundledSvgIcon icon)
{
    switch (icon) {
    case ZzBundledSvgIcon::Close:
        return QStringLiteral(":/zzfluent/icons/Close.svg");
    case ZzBundledSvgIcon::ComputerSystem:
        return QStringLiteral(":/zzfluent/icons/ComputerSystem.svg");
    case ZzBundledSvgIcon::FullScreen:
        return QStringLiteral(":/zzfluent/icons/FullScreen.svg");
    case ZzBundledSvgIcon::Maximize:
        return QStringLiteral(":/zzfluent/icons/Maximize.svg");
    case ZzBundledSvgIcon::Minimize:
        return QStringLiteral(":/zzfluent/icons/Minimize.svg");
    case ZzBundledSvgIcon::Moon:
        return QStringLiteral(":/zzfluent/icons/Moon.svg");
    case ZzBundledSvgIcon::MoreLine:
        return QStringLiteral(":/zzfluent/icons/MoreLine.svg");
    case ZzBundledSvgIcon::Pin:
        return QStringLiteral(":/zzfluent/icons/Pin.svg");
    case ZzBundledSvgIcon::PinFill:
        return QStringLiteral(":/zzfluent/icons/PinFill.svg");
    case ZzBundledSvgIcon::Restore:
        return QStringLiteral(":/zzfluent/icons/Restore.svg");
    case ZzBundledSvgIcon::Sun:
        return QStringLiteral(":/zzfluent/icons/Sun.svg");
    }
    return {};
}

} // namespace

ZzIconDescriptor::ZzIconDescriptor(
    QString resourceIdValue,
    bool mirroredInRightToLeftValue)
    : resourceId(std::move(resourceIdValue))
    , mirroredInRightToLeft(mirroredInRightToLeftValue)
{
}

ZzIconDescriptor ZzIconDescriptor::fromSvgResource(
    QString resourceId,
    bool mirroredInRightToLeft,
    ZzIconColorMode colorMode,
    QColor customColor)
{
    ZzIconDescriptor result(
        std::move(resourceId), mirroredInRightToLeft);
    result.source = ZzIconSource::SvgResource;
    result.colorMode = colorMode;
    result.customColor = customColor;
    return result;
}

ZzIconDescriptor ZzIconDescriptor::fromBundledSvg(
    ZzBundledSvgIcon icon,
    bool mirroredInRightToLeft,
    ZzIconColorMode colorMode,
    QColor customColor)
{
    return fromSvgResource(
        zzBundledSvgResourceId(icon),
        mirroredInRightToLeft,
        colorMode,
        customColor);
}

ZzIconDescriptor ZzIconDescriptor::fromFontIcon(
    ZzFontIcon icon,
    bool mirroredInRightToLeft,
    ZzIconColorMode colorMode,
    QColor customColor)
{
    ZzIconDescriptor result;
    result.mirroredInRightToLeft = mirroredInRightToLeft;
    result.source = ZzIconSource::FontGlyph;
    result.fontIcon = icon;
    result.colorMode = colorMode;
    result.customColor = customColor;
    return result;
}

} // namespace ZzFluentUI
