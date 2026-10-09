#pragma once

#include <QColor>
#include <QFrame>
#include <ZzFluentUI/ZzFluentUIExport.h>
#include <memory>

namespace ZzFluentUI {

class ZzBorderBeamPrivate;

/** @brief 可承载原生布局、沿圆角边框匀速播放渐变光束的 Fluent 容器。 */
class ZZ_FLUENT_UI_EXPORT ZzBorderBeam final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzBorderBeam)
    Q_PROPERTY(qreal beamLength READ beamLength WRITE setBeamLength NOTIFY beamLengthChanged)
    Q_PROPERTY(qreal beamWidth READ beamWidth WRITE setBeamWidth NOTIFY beamWidthChanged)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(
        QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QColor startColor READ startColor WRITE setStartColor NOTIFY startColorChanged)
    Q_PROPERTY(QColor endColor READ endColor WRITE setEndColor NOTIFY endColorChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY
            animationDurationChanged)
    Q_PROPERTY(
        qreal initialProgress READ initialProgress WRITE setInitialProgress NOTIFY initialProgressChanged)
    Q_PROPERTY(ZzBeamDirection direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(int beamCount READ beamCount WRITE setBeamCount NOTIFY beamCountChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY
            animationEnabledChanged)
    Q_PROPERTY(ZzBeamThemeMode themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(qreal progress READ progress)

public:
    enum ZzBeamDirection : int { Clockwise, CounterClockwise };
    Q_ENUM(ZzBeamDirection)
    enum ZzBeamThemeMode : int { AutoTheme, LightTheme, DarkTheme };
    Q_ENUM(ZzBeamThemeMode)

    /** @brief 无效颜色分别回退到 Window、Mid、Accent 及强调色的浅色变体。 */
    struct ZzBeamThemeConfig
    {
        QColor backgroundColor;
        QColor borderColor;
        QColor startColor;
        QColor endColor;
        bool operator==(const ZzBeamThemeConfig &) const = default;
    };

    explicit ZzBorderBeam(QWidget *parent = nullptr);
    ~ZzBorderBeam() override;

    /** @brief 光束长度，逻辑像素，范围 0–10000。 */
    [[nodiscard]] qreal beamLength() const;
    void setBeamLength(qreal value);
    /** @brief 光束线宽，逻辑像素，范围 0.5–32。 */
    [[nodiscard]] qreal beamWidth() const;
    void setBeamWidth(qreal value);
    /** @brief 圆角半径，逻辑像素，范围 0–1000。 */
    [[nodiscard]] qreal cornerRadius() const;
    void setCornerRadius(qreal value);
    /** @brief 背景颜色；无效颜色恢复主题配置。 */
    [[nodiscard]] QColor backgroundColor() const;
    void setBackgroundColor(QColor value);
    /** @brief 静态边框颜色；无效颜色恢复主题配置。 */
    [[nodiscard]] QColor borderColor() const;
    void setBorderColor(QColor value);
    /** @brief 拖尾起始颜色；无效颜色恢复主题配置。 */
    [[nodiscard]] QColor startColor() const;
    void setStartColor(QColor value);
    /** @brief 光束头部颜色；无效颜色恢复主题配置。 */
    [[nodiscard]] QColor endColor() const;
    void setEndColor(QColor value);
    /** @brief 绕行周期，毫秒，范围 100–600000。 */
    [[nodiscard]] int animationDuration() const;
    void setAnimationDuration(int value);
    /** @brief 初始位置，范围 0–1；设置时立即定位。 */
    [[nodiscard]] qreal initialProgress() const;
    void setInitialProgress(qreal value);
    /** @brief 运动方向；修改时保持头部位置。 */
    [[nodiscard]] ZzBeamDirection direction() const;
    void setDirection(ZzBeamDirection value);
    /** @brief 均匀分布的光束数量，范围 1–8。 */
    [[nodiscard]] int beamCount() const;
    void setBeamCount(int value);
    /** @brief 播放意图；隐藏、禁用或减少动态效果时仍暂停。 */
    [[nodiscard]] bool isAnimationEnabled() const;
    void setAnimationEnabled(bool value);
    /** @brief 跟随应用或固定浅色、深色主题。 */
    [[nodiscard]] ZzBeamThemeMode themeMode() const;
    void setThemeMode(ZzBeamThemeMode value);

    /** @brief 当前归一化头部位置 [0, 1)，暂停时保持不变。 */
    [[nodiscard]] qreal progress() const;
    /** @brief 是否实际运行动画。 */
    [[nodiscard]] bool isRunning() const;
    /** @brief 将当前位置恢复到 initialProgress，保留播放意图。 */
    void restartAnimation();
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    [[nodiscard]] static ZzBeamThemeConfig defaultLightTheme();
    [[nodiscard]] static ZzBeamThemeConfig defaultDarkTheme();
    [[nodiscard]] ZzBeamThemeConfig lightTheme() const;
    [[nodiscard]] ZzBeamThemeConfig darkTheme() const;
    [[nodiscard]] ZzBeamThemeConfig activeTheme() const;
    void setLightTheme(const ZzBeamThemeConfig &config);
    void setDarkTheme(const ZzBeamThemeConfig &config);

Q_SIGNALS:
    void beamLengthChanged(qreal value);
    void beamWidthChanged(qreal value);
    void cornerRadiusChanged(qreal value);
    void backgroundColorChanged(QColor value);
    void borderColorChanged(QColor value);
    void startColorChanged(QColor value);
    void endColorChanged(QColor value);
    void animationDurationChanged(int value);
    void initialProgressChanged(qreal value);
    void directionChanged(ZzBeamDirection value);
    void beamCountChanged(int value);
    void animationEnabledChanged(bool value);
    void themeModeChanged(ZzBeamThemeMode value);
    void runningChanged(bool running);
    void lightThemeChanged();
    void darkThemeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updateAnimationState();
    std::unique_ptr<ZzBorderBeamPrivate> d_ptr;
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzBorderBeam::ZzBeamThemeConfig)
