#pragma once

#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPureToolsExport.h>

namespace ZzPureTools {

class ZzPageInstancePrivate;

/**
 * @brief 统一管理一个页面的 View、Presenter、ViewModel 和取消操作。
 *
 * View 始终由 pageParent 的 Qt 父子树拥有，本对象只保存观察指针；Presenter 和
 * ViewModel 由本对象独占。pageParent 必须比页面实例存活更久。
 */
class ZZ_PURE_TOOLS_EXPORT ZzPageInstance final
{
public:
    /** @brief 页面销毁前用于请求取消后台工作的回调。 */
    using ZzCancelCallback = std::function<void()>;

    /**
     * @brief 校验并创建具有固定所有权模型的页面实例。
     * @param pageParent View 的唯一 QWidget 父对象，必须非空。
     * @param view 由 pageParent 直接拥有的非空页面控件。
     * @param viewModel 无 QObject 父对象的独占 ViewModel。
     * @param presenter 无 QObject 父对象的独占 Presenter。
     * @return 页面实例，或参数及所有权错误。
     *
     * 创建失败会在返回前销毁 view、presenter 和 viewModel；调用方不得再次释放。
     */
    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>>
    create(
        QWidget *pageParent,
        QWidget *view,
        std::unique_ptr<QObject> viewModel,
        std::unique_ptr<QObject> presenter);

    /** @brief 按固定清理顺序销毁页面展示对象。 */
    ~ZzPageInstance();

    /** @brief 禁止复制拥有展示对象的页面实例。 */
    ZzPageInstance(const ZzPageInstance &) = delete;

    /** @brief 禁止复制赋值拥有展示对象的页面实例。 */
    ZzPageInstance &operator=(const ZzPageInstance &) = delete;

    /** @brief 禁止移动已绑定 Qt 父对象树的页面实例。 */
    ZzPageInstance(ZzPageInstance &&) = delete;

    /** @brief 禁止移动赋值已绑定 Qt 父对象树的页面实例。 */
    ZzPageInstance &operator=(ZzPageInstance &&) = delete;

    /**
     * @brief 获取非拥有的页面 View。
     * @return 活动 View；View 已被外部父对象销毁时返回 nullptr。
     */
    [[nodiscard]] QWidget *view() const noexcept;

    /**
     * @brief 增加一个页面销毁前执行的取消回调。
     * @param callback 非空回调；页面已清理时立即执行并捕获异常。
     */
    void addCancellation(ZzCancelCallback callback);

    /**
     * @brief 幂等执行取消、断开连接和展示对象销毁。
     *
     * 每个取消回调独立捕获异常，单个失败不会跳过后续清理。
     */
    void prepareForDestruction() noexcept;

private:
    explicit ZzPageInstance(
        QWidget *view,
        std::unique_ptr<QObject> viewModel,
        std::unique_ptr<QObject> presenter);

    std::unique_ptr<ZzPageInstancePrivate> d_ptr;
};

} // namespace ZzPureTools
