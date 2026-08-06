#pragma once

#include <functional>

#include <QtCore/QObject>

#include <ZzPureTools/ZzRouteId.h>

namespace ZzExample {

class ZzExampleGalleryPage;

/** @brief 将 Gallery View 的路由意图转发给构造注入的窗口级导航端口。 */
class ZzExampleNavigationPresenter final : public QObject
{
public:
    /** @brief 定义不持有窗口对象的同步导航端口。 */
    using ZzNavigateCallback = std::function<void(
        const ZzPureTools::ZzRouteId &routeId)>;

    /**
     * @brief 连接 View 意图与非空导航回调。
     * @param view 生命周期覆盖 Presenter 的非空 View。
     * @param navigate 注入的窗口级导航回调。
     */
    ZzExampleNavigationPresenter(
        ZzExampleGalleryPage *view,
        ZzNavigateCallback navigate);

    /** @brief 断开 View 连接并释放回调捕获值。 */
    ~ZzExampleNavigationPresenter() override;

    /** @brief 禁止复制持有连接的 Presenter。 */
    ZzExampleNavigationPresenter(
        const ZzExampleNavigationPresenter &) = delete;

    /** @brief 禁止复制赋值持有连接的 Presenter。 */
    ZzExampleNavigationPresenter &operator=(
        const ZzExampleNavigationPresenter &) = delete;

    /** @brief 禁止移动已经注册为 QObject 接收者的 Presenter。 */
    ZzExampleNavigationPresenter(ZzExampleNavigationPresenter &&) = delete;

    /** @brief 禁止移动赋值已经注册为 QObject 接收者的 Presenter。 */
    ZzExampleNavigationPresenter &operator=(
        ZzExampleNavigationPresenter &&) = delete;
};

} // namespace ZzExample
