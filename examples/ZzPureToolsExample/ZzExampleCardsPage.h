#pragma once

#include <memory>

#include <QtWidgets/QWidget>

class QEvent;
class QAbstractItemModel;

namespace ZzExample {

class ZzExampleCardsPagePrivate;

/** @brief 展示卡片、轮播和数字显示能力的纯 QWidget View。 */
class ZzExampleCardsPage final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建不访问文件、网络或应用服务的卡片展示页。
     * @param title 页面标题。
     * @param carouselModel 非空只读轮播展示模型。
     * @param parent 必须由页面宿主提供的 QWidget 父对象。
     */
    explicit ZzExampleCardsPage(
        const QString &title,
        QAbstractItemModel *carouselModel,
        QWidget *parent);

    /** @brief 释放私有状态，控件与模型由 Qt 父子树销毁。 */
    ~ZzExampleCardsPage() override;

    /** @brief 禁止复制 QWidget View。 */
    ZzExampleCardsPage(const ZzExampleCardsPage &) = delete;

    /** @brief 禁止复制赋值 QWidget View。 */
    ZzExampleCardsPage &operator=(
        const ZzExampleCardsPage &) = delete;

    /** @brief 禁止移动已经建立 QObject 连接的 View。 */
    ZzExampleCardsPage(ZzExampleCardsPage &&) = delete;

    /** @brief 禁止移动赋值已经建立 QObject 连接的 View。 */
    ZzExampleCardsPage &operator=(ZzExampleCardsPage &&) = delete;

protected:
    /** @brief 在 palette 或 style 变化后刷新内存预览图。 */
    void changeEvent(QEvent *event) override;

private:
    friend class ZzExampleCardsPagePrivate;
    std::unique_ptr<ZzExampleCardsPagePrivate> d_ptr;
};

} // namespace ZzExample
