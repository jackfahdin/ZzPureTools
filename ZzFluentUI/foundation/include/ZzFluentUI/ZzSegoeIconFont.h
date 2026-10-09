#pragma once

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QIcon>
#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief Segoe Fluent Icons 原始字体码点，与 FluentUIStyle 共用同一字形。 */
enum class ZzSegoeIcon : char16_t
{
    Home = 0xe80f,
    Search = 0xe721,
    Settings = 0xe713,
    Help = 0xe897,
    Info = 0xe946,
    Folder = 0xe8b7,
    History = 0xe81c,
    CompanionApp = 0xec64,
    PlayerSettings = 0xef58,
    Robot = 0xe99a,
    RingerSilent = 0xe7ed,
    TrafficCongestionSolid = 0xf163,
    Camera = 0xe722,
    Video = 0xe714,
    MusicInfo = 0xe90b,
    Cloud = 0xe753,
    Unknown = 0xe9ce,
    Close = 0xe894,
    ChevronLeft = 0xe76b,
    ChevronRight = 0xe76c
};

/** @brief 注册内嵌 Segoe Fluent Icons，并按目标尺寸和 DPR 绘制字体图标。
 *  @pre 注册、创建及绘制图标均在 QGuiApplication 的 GUI 线程进行。
 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzSegoeIconFont final
{
public:
    ZzSegoeIconFont() = delete;
    /** @brief 注册随库分发的原始 TTF；不依赖系统是否安装该字体。 */
    [[nodiscard]] static bool ensureRegistered();
    /** @brief 返回字体 family：Segoe Fluent Icons。 */
    [[nodiscard]] static QString familyName();
    /** @brief 返回禁止字形回退、常规字重的字体，pixelSize 必须大于零。 */
    [[nodiscard]] static QFont font(int pixelSize);
    /** @brief 创建可缩放图标；无效 color 表示绘制时取应用调色板文字色。
     *  自定义标签样式会让自动颜色图标跟随标签前景色；显式颜色保持不变。
     */
    [[nodiscard]] static QIcon icon(ZzSegoeIcon glyph, const QColor &color = {});
    /** @brief 创建与 FluentUIStyle Gallery 同比例的主题图标。
     *  默认采用 25 px 字形 / 30 px 画布；pixelSize > 0 时默认画布同尺寸（Pivot 为 22）。
     *  canvasSize > 0 可单独指定画布，例如原版关闭按钮为 27 / 30。
     *  颜色随应用调色板刷新，不随标签选中状态着色；高 DPI 按设备比例生成原始画布。
     */
    [[nodiscard]] static QIcon galleryIcon(ZzSegoeIcon glyph, int pixelSize = 0, int canvasSize = 0);
    /** @brief 判断是否为本接口创建的自动颜色图标，供控件样式按前景色着色。 */
    [[nodiscard]] static bool usesForegroundColor(const QIcon &icon);
    /** @brief 为自动颜色或 Gallery 主题图标指定最终前景色；固定颜色和其他来源图标原样返回。
     *  直接以最终颜色绘制字形，避免调色板与控件前景色的透明度重复相乘。
     */
    [[nodiscard]] static QIcon withForegroundColor(const QIcon &icon, const QColor &color);
};

} // namespace ZzFluentUI
