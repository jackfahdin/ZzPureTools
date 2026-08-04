#pragma once

#include <memory>

#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzIconButtonPrivate;

/**
 * @brief 使用主题图标缓存绘制的仅图标按钮。
 *
 * 调用者必须设置非空 accessibleName；控件在 GUI 线程使用，由 QObject parent 所有。
 */
class ZZ_FLUENT_UI_EXPORT ZzIconButton final : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzIconButton)

public:
    /**
     * @brief 创建尚未绑定图标描述的按钮。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzIconButton(QWidget *parent = nullptr);

    /** @brief 销毁私有图标描述，不改变 QObject parent 所有权。 */
    ~ZzIconButton() override;

    /**
     * @brief 设置 Foundation 图标描述并刷新当前 DPR 的缓存图像。
     * @param descriptor 按值复制、不转移所有权的图标描述。
     */
    void setIconDescriptor(const ZzIconDescriptor &descriptor);

protected:
    /** @brief 在影响图标颜色或比例的 Qt 状态变化后刷新图标。 */
    void changeEvent(QEvent *event) override;

    /** @brief 按新的逻辑尺寸刷新有界图标缓存项。 */
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<ZzIconButtonPrivate> d_ptr;
};

} // namespace ZzFluentUI
