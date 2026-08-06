#pragma once

#include <QtCore/QString>

#include "ZzExampleShowcasePage.h"

class QVBoxLayout;
class QWidget;

namespace ZzExample {

/** @brief 实现导航、反馈和图标页的本地 UI 交互。 */
class ZzExampleShowcasePagePrivate final
{
public:
    /** @brief 保存非空 View 观察指针。 */
    explicit ZzExampleShowcasePagePrivate(
        ZzExampleShowcasePage *page);

    /** @brief 创建页面滚动骨架并装配指定内容。 */
    void initialize(
        ZzExampleShowcasePage::ZzPageKind kind,
        const QString &title);

    /** @brief 创建面包屑与可转移标签。 */
    void buildNavigation(QVBoxLayout *layout, QWidget *parent);

    /** @brief 创建菜单、消息、对话框与搜索建议。 */
    void buildFeedback(QVBoxLayout *layout, QWidget *parent);

    /** @brief 创建 Qt 跨平台标准图标集合。 */
    void buildIcons(QVBoxLayout *layout, QWidget *parent);

    ZzExampleShowcasePage *q_ptr = nullptr;
};

} // namespace ZzExample
