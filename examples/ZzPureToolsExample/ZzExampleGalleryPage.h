#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

namespace ZzExample {

class ZzExampleGalleryPagePrivate;

/** @brief 提供不访问应用服务的首页与基础控件展示 View。 */
class ZzExampleGalleryPage final : public QWidget
{
    Q_OBJECT

public:
    /** @brief 标识当前 View 要创建的页面内容。 */
    enum class ZzPageKind
    {
        Home,
        Controls
    };
    Q_ENUM(ZzPageKind)

    /**
     * @brief 创建指定类型的可滚动展示页。
     * @param kind 页面内容类型。
     * @param title 页面标题。
     * @param parent 必须由页面宿主提供的 QWidget 父对象。
     */
    explicit ZzExampleGalleryPage(
        ZzPageKind kind,
        const QString &title,
        QWidget *parent);

    /** @brief 释放 View 私有状态，全部控件由 QWidget 父子树销毁。 */
    ~ZzExampleGalleryPage() override;

    /** @brief 禁止复制 QWidget View。 */
    ZzExampleGalleryPage(const ZzExampleGalleryPage &) = delete;

    /** @brief 禁止复制赋值 QWidget View。 */
    ZzExampleGalleryPage &operator=(
        const ZzExampleGalleryPage &) = delete;

    /** @brief 禁止移动已经建立 QObject 连接的 View。 */
    ZzExampleGalleryPage(ZzExampleGalleryPage &&) = delete;

    /** @brief 禁止移动赋值已经建立 QObject 连接的 View。 */
    ZzExampleGalleryPage &operator=(ZzExampleGalleryPage &&) = delete;

Q_SIGNALS:
    /**
     * @brief 用户激活首页快捷入口后发出导航意图。
     * @param routeId 不本地化的目标路由文本。
     */
    void routeRequested(const QString &routeId);

private:
    friend class ZzExampleGalleryPagePrivate;
    std::unique_ptr<ZzExampleGalleryPagePrivate> d_ptr;
};

} // namespace ZzExample
