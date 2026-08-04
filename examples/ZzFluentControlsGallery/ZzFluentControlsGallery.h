#pragma once

#include <memory>

#include <QtWidgets/QWidget>

namespace ZzFluentUI {

class ZzThemeController;

} // namespace ZzFluentUI

namespace ZzExamples {

class ZzFluentControlsGalleryPrivate;

/**
 * @brief 展示并操作第一阶段 Fluent Widgets 控件的无业务画廊。
 *
 * 画廊只持有本地展示模型并转发窗口交互，不访问领域、存储或网络对象。
 */
class ZzFluentControlsGallery final : public QWidget
{
    Q_DISABLE_COPY_MOVE(ZzFluentControlsGallery)

public:
    /**
     * @brief 创建全控件展示窗口。
     * @param themeController 非空、非拥有，必须与画廊同属 GUI 线程且寿命更长。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzFluentControlsGallery(
        ZzFluentUI::ZzThemeController *themeController,
        QWidget *parent = nullptr);

    /** @brief 销毁私有布局状态，Qt 子控件按 parent 关系释放。 */
    ~ZzFluentControlsGallery() override;

private:
    std::unique_ptr<ZzFluentControlsGalleryPrivate> d_ptr;
};

} // namespace ZzExamples
