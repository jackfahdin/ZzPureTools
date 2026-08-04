#pragma once

#include <QtCore/QObject>

namespace ZzPureTools {
namespace Internal {

/** @brief 提供不访问业务对象的良好展示测试类型。 */
class ZzGoodWidget final : public QObject
{
public:
    /** @brief 创建良好展示测试对象。 */
    ZzGoodWidget() = default;

    /** @brief 返回固定的展示状态。 */
    [[nodiscard]] bool isReady() const;
};

} // namespace Internal
} // namespace ZzPureTools
