#pragma once
class QWidget;
namespace ZzExample {

/** @brief 验证轮播页实际交互，并固定当前帧供截图。 */
class ZzExampleCarouselSmoke final
{
public:
    /** @brief 验证轮播页实际交互，并固定当前帧供截图。 */
    [[nodiscard]] static bool isPageReady(const QWidget &window);
};

} // namespace ZzExample
