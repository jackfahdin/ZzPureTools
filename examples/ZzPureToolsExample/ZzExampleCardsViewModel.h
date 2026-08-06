#pragma once

#include <QtGui/QStandardItemModel>

namespace ZzExample {

/** @brief 为卡片页轮播提供只读本地展示值。 */
class ZzExampleCardsViewModel final : public QStandardItemModel
{
public:
    /** @brief 创建三条不访问应用服务的确定性轮播展示项。 */
    ZzExampleCardsViewModel();

    /** @brief 释放标准项所有权树。 */
    ~ZzExampleCardsViewModel() override;

    /** @brief 禁止复制 QObject 模型。 */
    ZzExampleCardsViewModel(
        const ZzExampleCardsViewModel &) = delete;

    /** @brief 禁止复制赋值 QObject 模型。 */
    ZzExampleCardsViewModel &operator=(
        const ZzExampleCardsViewModel &) = delete;

    /** @brief 禁止移动具有模型索引身份的 QObject。 */
    ZzExampleCardsViewModel(ZzExampleCardsViewModel &&) = delete;

    /** @brief 禁止移动赋值具有模型索引身份的 QObject。 */
    ZzExampleCardsViewModel &operator=(
        ZzExampleCardsViewModel &&) = delete;
};

} // namespace ZzExample
