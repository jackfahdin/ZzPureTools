#pragma once

#include <array>

#include <QtCore/QString>

class QAbstractItemModel;

namespace ZzFluentUI {
class ZzImageCard;
}

namespace ZzExample {

class ZzExampleCardsPage;

/** @brief 实现卡片页面控件树和 palette 派生预览图。 */
class ZzExampleCardsPagePrivate final
{
public:
    /** @brief 保存非空 View 观察指针。 */
    explicit ZzExampleCardsPagePrivate(ZzExampleCardsPage *page);

    /** @brief 创建卡片、轮播和数字显示区域。 */
    void initialize(
        const QString &title,
        QAbstractItemModel *carouselModel);

    /** @brief 以当前 palette 重建全部确定性内存预览图。 */
    void refreshPalettePreviews();

    ZzExampleCardsPage *q_ptr = nullptr;
    std::array<ZzFluentUI::ZzImageCard *, 3> imageCards{};
};

} // namespace ZzExample
