#pragma once

#include <array>
#include <cstddef>

#include <QtCore/QtTypes>
#include <QtGui/QFont>

#include <ZzFluentUI/ZzFluentFoundationExport.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemePalette.h>
#include <ZzFluentUI/ZzTypographyToken.h>

namespace ZzFluentUI {

/** @brief 提供一次主题 revision 的完整只读令牌快照。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzThemeSnapshot final
{
public:
    /**
     * @brief 使用当前应用字体构造完整快照。
     * @param mode 必须解析为 Light、Dark 或 HighContrast。
     * @param accent 强调色，无效值使用默认色。
     * @param revision 单调递增的主题版本。
     * @param reducedMotion 是否关闭非必要动画。
     * @return 线程可读的不可变值。
     * @pre 已构造 QGuiApplication，且调用发生在应用所属 GUI 线程。
     * @note 构造阶段读取 QGuiApplication::font()；构造完成后可跨线程只读。
     */
    [[nodiscard]] static ZzThemeSnapshot create(
        ZzThemeMode mode,
        QColor accent,
        quint64 revision,
        bool reducedMotion);

    /**
     * @brief 返回指定颜色令牌。
     * @param token 必须小于 Count。
     * @return 当前快照中的颜色值。
     */
    [[nodiscard]] QColor color(ZzColorToken token) const noexcept;

    /**
     * @brief 返回指定尺寸令牌。
     * @param token 必须小于 Count。
     * @return 与设备无关的逻辑像素值。
     */
    [[nodiscard]] qreal metric(ZzMetricToken token) const noexcept;

    /**
     * @brief 返回指定排版令牌。
     * @param token 必须小于 Count。
     * @return 当前快照中的字体值。
     */
    [[nodiscard]] QFont font(ZzTypographyToken token) const;

    /**
     * @brief 返回指定动效令牌的标准时长。
     * @param token 必须小于 Count。
     * @return 动画时长，单位为毫秒。
     */
    [[nodiscard]] int duration(ZzMotionToken token) const noexcept;

    /**
     * @brief 返回快照的单调版本号。
     * @return 主题 revision。
     */
    [[nodiscard]] quint64 revision() const noexcept;

    /**
     * @brief 返回是否关闭非必要动画。
     * @return 启用减少动效时返回 true。
     */
    [[nodiscard]] bool reducedMotion() const noexcept;

private:
    ZzThemeSnapshot(
        const ZzThemePalette &palette,
        const std::array<
            qreal,
            static_cast<std::size_t>(ZzMetricToken::Count)> &metrics,
        std::array<
            QFont,
            static_cast<std::size_t>(ZzTypographyToken::Count)> fonts,
        const std::array<
            int,
            static_cast<std::size_t>(ZzMotionToken::Count)> &durations,
        quint64 revision,
        bool reducedMotion);

    ZzThemePalette palette_;
    std::array<
        qreal,
        static_cast<std::size_t>(ZzMetricToken::Count)> metrics_;
    std::array<
        QFont,
        static_cast<std::size_t>(ZzTypographyToken::Count)> fonts_;
    std::array<
        int,
        static_cast<std::size_t>(ZzMotionToken::Count)> durations_;
    quint64 revision_;
    bool reducedMotion_;
};

} // namespace ZzFluentUI
