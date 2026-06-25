#ifndef ZZPURETOOLS_CORE_ZZTHEME_HPP
#define ZZPURETOOLS_CORE_ZZTHEME_HPP

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Core/ZzToken.hpp"

#include <QObject>

/**
 * @brief Fluent UI 主题管理类。
 *
 * 提供 Light / Dark / System 三种主题模式，并支持自定义强调色。
 * 主题变化时会通过信号通知所有订阅者，以便控件重新绘制。
 */
class ZZ_PURE_TOOLS_EXPORT ZzTheme : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造主题对象。
     * @param parent 父对象，默认为 nullptr。
     */
    explicit ZzTheme(QObject* parent = nullptr);

    /**
     * @brief 获取当前设置的主题模式。
     * @return 用户设置的模式，可能为 System。
     */
    [[nodiscard]] ZzThemeMode mode() const noexcept;

    /**
     * @brief 获取实际生效的主题模式。
     * @return 解析 System 后的实际模式，始终为 Light 或 Dark。
     */
    [[nodiscard]] ZzThemeMode effectiveMode() const noexcept;

    /**
     * @brief 获取当前颜色令牌。
     * @return 颜色令牌常量引用。
     */
    [[nodiscard]] const ZzColorTokens& colors() const noexcept;

    /**
     * @brief 获取当前尺寸令牌。
     * @return 尺寸令牌常量引用。
     */
    [[nodiscard]] const ZzMetricTokens& metrics() const noexcept;

    /**
     * @brief 设置主题模式。
     * @param mode 目标主题模式。
     */
    void setMode(ZzThemeMode mode);

    /**
     * @brief 从操作系统同步主题模式。
     *
     * 当 mode() 为 System 时，根据系统当前主题解析为 Light 或 Dark。
     */
    void syncFromSystem();

    /**
     * @brief 获取指定状态下控件的填充色。
     * @param state 控件状态。
     * @return 对应状态的填充色。
     */
    [[nodiscard]] QColor controlFill(ZzControlState state) const;

    /**
     * @brief 获取指定状态下控件的边框色。
     * @param state 控件状态。
     * @return 对应状态的边框色。
     */
    [[nodiscard]] QColor controlStroke(ZzControlState state) const;

    /**
     * @brief 获取指定状态下控件的文本色。
     * @param state 控件状态。
     * @return 对应状态的文本色。
     */
    [[nodiscard]] QColor textColor(ZzControlState state) const;

public slots:
    /**
     * @brief 设置自定义强调色。
     * @param color 强调色。
     */
    void setAccentColor(const QColor& color);

signals:
    /**
     * @brief 主题模式发生变化时触发。
     * @param mode 新的主题模式。
     */
    void modeChanged(ZzThemeMode mode);

    /**
     * @brief 颜色令牌发生变化时触发。
     */
    void colorsChanged();

    /**
     * @brief 尺寸令牌发生变化时触发。
     */
    void metricsChanged();

private:
    void applyResolvedMode(ZzThemeMode resolvedMode);
    [[nodiscard]] ZzThemeMode resolveSystemMode() const;

    ZzThemeMode m_mode = ZzThemeMode::System;            ///< 用户设置的主题模式
    ZzThemeMode m_effectiveMode = ZzThemeMode::Light;    ///< 解析后的实际主题模式
    ZzColorTokens m_colors = zzLightColorTokens();       ///< 当前颜色令牌
    ZzMetricTokens m_metrics = zzDefaultMetricTokens();  ///< 当前尺寸令牌
    QColor m_customAccent;                               ///< 自定义强调色
    bool m_hasCustomAccent = false;                      ///< 是否设置了自定义强调色
};

#endif  // ZZPURETOOLS_CORE_ZZTHEME_HPP
