#pragma once

#include <QtCore/QMetaType>
#include <QtGui/QColor>

namespace ZzFluentUI {

/** @brief 标签栏外观；Standard 保留工作区原有指示条样式。 */
enum class ZzTabBarAppearance
{
    Standard,
    Capsule,
    PivotGrow,
    PivotSlide,
    PivotStretch,
    Pill,
    SegmentedSlide,
    SegmentedFade,
    SegmentedWinUI3,
    Navigation
};

/**
 * @brief 分段标签栏的局部颜色；无效颜色使用主题默认值。
 * @note 高对比度忽略覆盖；自定义选中色未指定 selectedText 时自动选择黑/白前景。
 */
struct ZzTabBarColors final
{
    QColor background;
    QColor selected;
    QColor hover;
    QColor pressed;
    QColor text;
    QColor selectedText;
    bool operator==(const ZzTabBarColors &) const = default;
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzTabBarAppearance)
Q_DECLARE_METATYPE(ZzFluentUI::ZzTabBarColors)
