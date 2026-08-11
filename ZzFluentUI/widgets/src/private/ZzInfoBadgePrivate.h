#pragma once

#include <QtCore/QString>
#include <QtGui/QIcon>

#include <ZzFluentUI/ZzInfoBadgeKind.h>
#include <ZzFluentUI/ZzMessageSeverity.h>

#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

class ZzInfoBadge;
enum class ZzColorToken : std::uint16_t;

/** @brief 保存信息徽章纯展示状态和主题回退缓存。 */
class ZzInfoBadgePrivate final
{
public:
    /** @brief 绑定公开控件并完成首个展示同步。 */
    explicit ZzInfoBadgePrivate(ZzInfoBadge *q);

    /** @brief 返回数字模式经过上限收敛的文本。 */
    [[nodiscard]] QString displayText() const;

    /** @brief 返回当前严重性对应的语义颜色令牌。 */
    [[nodiscard]] ZzColorToken fillToken() const noexcept;

    /** @brief 同步 QLabel 文本、字体、无障碍名称和几何。 */
    void refreshPresentation();

    ZzInfoBadge *const q_ptr;
    ZzWidgetTheme theme;
    ZzInfoBadgeKind kind = ZzInfoBadgeKind::Dot;
    int value = 0;
    int maximumValue = 99;
    ZzMessageSeverity severity = ZzMessageSeverity::Information;
    QIcon icon;
    QString generatedAccessibleName;
};

} // namespace ZzFluentUI
