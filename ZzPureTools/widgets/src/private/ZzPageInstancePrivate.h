#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <ZzPureTools/ZzPageInstance.h>

namespace ZzPureTools {

/** @brief 实现页面任务取消和展示对象的确定性销毁顺序。 */
class ZzPageInstancePrivate final
{
public:
    /** @brief 接管 Presenter 和 ViewModel，并观察 Qt 父子树拥有的 View。 */
    ZzPageInstancePrivate(
        QWidget *view,
        std::unique_ptr<QObject> viewModel,
        std::unique_ptr<QObject> presenter);

    /** @brief 增加或立即执行页面取消回调。 */
    void addCancellation(ZzPageInstance::ZzCancelCallback callback);

    /** @brief 幂等执行取消、断开连接和对象销毁。 */
    void prepareForDestruction() noexcept;

    QPointer<QWidget> view;

private:
    /** @brief 捕获并记录单个取消回调抛出的异常。 */
    static void invokeCancellation(
        ZzPageInstance::ZzCancelCallback &callback) noexcept;

    std::unique_ptr<QObject> viewModel_;
    std::unique_ptr<QObject> presenter_;
    std::vector<ZzPageInstance::ZzCancelCallback> cancellations_;
    bool prepared_ = false;
};

} // namespace ZzPureTools
