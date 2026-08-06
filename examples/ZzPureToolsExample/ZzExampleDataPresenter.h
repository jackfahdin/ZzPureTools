#pragma once

#include <QtCore/QObject>

#include "ZzExampleDataPageKind.h"

namespace ZzExample {

class ZzExampleDataPage;
class ZzExampleDataViewModel;

/** @brief 将数据页用户意图协调到注入 ViewModel 并回写展示状态。 */
class ZzExampleDataPresenter final : public QObject
{
public:
    /**
     * @brief 连接生命周期由页面实例共同管理的 View 与 ViewModel。
     * @param kind 页面种类。
     * @param view 非空页面 View。
     * @param viewModel 非空展示模型。
     */
    ZzExampleDataPresenter(
        ZzExampleDataPageKind kind,
        ZzExampleDataPage *view,
        ZzExampleDataViewModel *viewModel);

    /** @brief 断开全部数据页意图连接。 */
    ~ZzExampleDataPresenter() override;

    /** @brief 禁止复制持有连接的 Presenter。 */
    ZzExampleDataPresenter(const ZzExampleDataPresenter &) = delete;

    /** @brief 禁止复制赋值持有连接的 Presenter。 */
    ZzExampleDataPresenter &operator=(
        const ZzExampleDataPresenter &) = delete;

    /** @brief 禁止移动已经注册为 QObject 接收者的 Presenter。 */
    ZzExampleDataPresenter(ZzExampleDataPresenter &&) = delete;

    /** @brief 禁止移动赋值已经注册为 QObject 接收者的 Presenter。 */
    ZzExampleDataPresenter &operator=(
        ZzExampleDataPresenter &&) = delete;
};

} // namespace ZzExample
