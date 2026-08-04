#pragma once

#include <QtCore/QString>

namespace ZzPureTools {
namespace Internal {

/** @brief 提供不依赖展示层的良好 AppCore 测试类型。 */
class ZzGoodAppCore final
{
public:
    /** @brief 返回用于边界契约的稳定文本。 */
    [[nodiscard]] QString value() const;
};

} // namespace Internal
} // namespace ZzPureTools
