#pragma once

#include <memory>

#include <QtWidgets/QLayout>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzFlowLayoutPrivate;

/**
 * @brief 按可用宽度自动换行的高性能流式布局。
 *
 * 布局保留 QLayoutItem 的标准所有权和 height-for-width 语义，支持普通
 * widget、spacer、嵌套 layout、隐藏项以及从右到左布局。布局只管理展示
 * 几何，不保存业务数据，也不创建逐项动画或计时器。
 */
class ZZ_FLUENT_UI_EXPORT ZzFlowLayout final : public QLayout
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFlowLayout)
    Q_PROPERTY(
        int horizontalSpacing
        READ horizontalSpacing
        WRITE setHorizontalSpacing
        NOTIFY horizontalSpacingChanged)
    Q_PROPERTY(
        int verticalSpacing
        READ verticalSpacing
        WRITE setVerticalSpacing
        NOTIFY verticalSpacingChanged)

public:
    /**
     * @brief 创建使用当前 style 间距的流式布局。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzFlowLayout(QWidget *parent = nullptr);

    /**
     * @brief 创建使用指定轴向间距的流式布局。
     * @param horizontalSpacing 水平逻辑像素间距，-1 表示使用当前 style。
     * @param verticalSpacing 垂直逻辑像素间距，-1 表示使用当前 style。
     * @param parent 可为空的 QWidget 所有者。
     */
    ZzFlowLayout(
        int horizontalSpacing,
        int verticalSpacing,
        QWidget *parent = nullptr);

    /** @brief 删除布局仍拥有的 layout item，不删除父对象拥有的 widget。 */
    ~ZzFlowLayout() override;

    /**
     * @brief 返回配置的水平间距。
     * @return 非负逻辑像素，或表示跟随 style 的 -1。
     */
    [[nodiscard]] int horizontalSpacing() const noexcept;

    /**
     * @brief 设置水平间距并使现有几何和高度缓存失效。
     * @param spacing 非负逻辑像素；小于 -1 的值规范为 -1。
     */
    void setHorizontalSpacing(int spacing);

    /**
     * @brief 返回配置的垂直间距。
     * @return 非负逻辑像素，或表示跟随 style 的 -1。
     */
    [[nodiscard]] int verticalSpacing() const noexcept;

    /**
     * @brief 设置垂直间距并使现有几何和高度缓存失效。
     * @param spacing 非负逻辑像素；小于 -1 的值规范为 -1。
     */
    void setVerticalSpacing(int spacing);

    /**
     * @brief 把 layout item 追加到逻辑流末尾。
     * @param item 所有权转交给布局的非空 item。
     */
    void addItem(QLayoutItem *item) override;

    /**
     * @brief 返回布局当前拥有的 item 数量。
     * @return 包含隐藏项的 item 数量。
     */
    [[nodiscard]] int count() const override;

    /**
     * @brief 按逻辑添加顺序返回 item。
     * @param index 从零开始的索引。
     * @return 非拥有指针；索引无效时返回空。
     */
    [[nodiscard]] QLayoutItem *itemAt(int index) const override;

    /**
     * @brief 从布局移除 item 并把所有权交还调用方。
     * @param index 从零开始的索引。
     * @return 被移除的 item；索引无效时返回空。
     */
    [[nodiscard]] QLayoutItem *takeAt(int index) override;

    /**
     * @brief 返回布局不会主动扩展的方向。
     * @return 空方向集合。
     */
    [[nodiscard]] Qt::Orientations expandingDirections() const override;

    /**
     * @brief 返回布局高度取决于可用宽度。
     * @return 始终返回 true。
     */
    [[nodiscard]] bool hasHeightForWidth() const override;

    /**
     * @brief 计算指定总宽度下包含 contents margins 的布局高度。
     * @param width 布局总宽度。
     * @return 非负逻辑像素高度。
     */
    [[nodiscard]] int heightForWidth(int width) const override;

    /**
     * @brief 返回至少容纳一个有效 item 的尺寸下界。
     * @return 包含 contents margins 的最小尺寸。
     */
    [[nodiscard]] QSize minimumSize() const override;

    /**
     * @brief 返回全部有效 item 单行排列时的首选尺寸。
     * @return 包含 contents margins 的饱和首选尺寸。
     */
    [[nodiscard]] QSize sizeHint() const override;

    /**
     * @brief 同步计算换行并更新全部有效 item 的几何。
     * @param rect 布局获得的总矩形。
     */
    void setGeometry(const QRect &rect) override;

    /** @brief 清除尺寸与几何缓存并通知 Qt 重新布局。 */
    void invalidate() override;

Q_SIGNALS:
    /**
     * @brief 水平间距配置实际变化后发出。
     * @param spacing 新配置值。
     */
    void horizontalSpacingChanged(int spacing);

    /**
     * @brief 垂直间距配置实际变化后发出。
     * @param spacing 新配置值。
     */
    void verticalSpacingChanged(int spacing);

private:
    std::unique_ptr<ZzFlowLayoutPrivate> d_ptr;
};

} // namespace ZzFluentUI
