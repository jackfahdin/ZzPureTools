#pragma once

#include "ZzExampleControlKind.h"
#include <QtWidgets/QWidget>
#include <memory>

namespace ZzExample {

class ZzExampleControlPagePrivate;

/** @brief 按需创建一种控件的独立演示页，不访问应用业务模型。 */
class ZzExampleControlPage final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzExampleControlPage)
public:
    /**
     * @brief 创建统一排版的可滚动演示页。
     * @param kind 本页展示的主要控件类型。
     * @param routeId 用于识别页面的稳定路由。
     * @param title 中英文页面标题。
     * @param parent 页面宿主提供的父控件。
     */
    ZzExampleControlPage(
        ZzExampleControlKind kind, const QString &routeId, const QString &title, QWidget *parent);
    /** @brief 释放私有展示状态和所属控件。 */
    ~ZzExampleControlPage() override;

protected:
    /** @brief 主题或屏幕缩放变化后刷新工具按钮的演示图标。 */
    bool event(QEvent *event) override;

private:
    std::unique_ptr<ZzExampleControlPagePrivate> d_ptr;
};

} // namespace ZzExample
