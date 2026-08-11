#pragma once

#include <memory>

#include <QtCore/QtTypes>

namespace ZzFluentUI {

class ZzThemeSnapshot;
class ZzFluentStyle;

} // namespace ZzFluentUI

class QWidget;

namespace ZzFluentUI {

/** @brief 为组合控件缓存非 Fluent 样式下的只读主题回退。 */
class ZzWidgetTheme final
{
public:
    /** @brief 绑定非拥有控件并建立首个回退快照。 */
    explicit ZzWidgetTheme(QWidget *widget);

    /** @brief 返回应用 Fluent 快照或缓存的回退快照。 */
    [[nodiscard]] std::shared_ptr<const ZzThemeSnapshot> snapshot() const;

    /** @brief 按当前 palette 重建只在环境变化时使用的回退快照。 */
    void refreshFallback();

private:
    QWidget *const widget_;
    std::shared_ptr<const ZzThemeSnapshot> fallback_;
    quint64 fallbackRevision_ = 0;
};

} // namespace ZzFluentUI
