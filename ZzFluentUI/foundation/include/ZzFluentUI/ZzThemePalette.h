#pragma once

#include <array>
#include <cstddef>

#include <QtGui/QColor>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentFoundationExport.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace ZzFluentUI {

/** @brief 保存一个完整且不可变的主题调色板。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzThemePalette final
{
public:
    /**
     * @brief 根据明确模式和强调色构造完整调色板。
     * @param mode 必须是 Light、Dark 或 HighContrast。
     * @param accent 有效强调色；无效颜色使用平台无关蓝色。
     * @return 可复制的定长调色板，调用线程不限。
     */
    [[nodiscard]] static ZzThemePalette create(
        ZzThemeMode mode,
        QColor accent);

    /**
     * @brief 按令牌读取颜色。
     * @param token 必须小于 Count；越界时断言并返回透明色。
     * @return 调色板拥有值的副本，调用线程不限。
     */
    [[nodiscard]] QColor color(ZzColorToken token) const noexcept;

private:
    explicit ZzThemePalette(
        std::array<
            QColor,
            static_cast<std::size_t>(ZzColorToken::Count)> colors);

    std::array<
        QColor,
        static_cast<std::size_t>(ZzColorToken::Count)> colors_;
};

} // namespace ZzFluentUI
