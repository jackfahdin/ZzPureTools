#pragma once

#include <QtCore/QString>

#include "ZzExampleGalleryPage.h"

namespace ZzExample {

/** @brief 实现首页与基础控件页的纯展示控件树。 */
class ZzExampleGalleryPagePrivate final
{
public:
    /** @brief 保存非空 View 观察指针。 */
    explicit ZzExampleGalleryPagePrivate(ZzExampleGalleryPage *page);

    /** @brief 按页面类型创建完整且稳定尺寸的控件树。 */
    void initialize(
        ZzExampleGalleryPage::ZzPageKind kind,
        const QString &title);

    /** @brief 创建首页品牌、快捷入口与最近状态。 */
    void buildHome(const QString &title);

    /** @brief 创建基础输入、选择、数值、日期和进度控件。 */
    void buildControls(const QString &title);

    ZzExampleGalleryPage *q_ptr = nullptr;
};

} // namespace ZzExample
