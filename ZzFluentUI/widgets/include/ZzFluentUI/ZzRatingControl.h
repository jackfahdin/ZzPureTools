#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzRatingPrecision.h>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace ZzFluentUI {

class ZzRatingControlPrivate;

/**
 * @brief 提供整星或半星精度的 Fluent 评分输入。
 *
 * 控件只维护可量化的展示值并发出输入意图，不访问评分模型或业务服务。
 * 鼠标悬停仅预览，鼠标拖动、键盘和公开 setter 共享同一数值约束。
 */
class ZZ_FLUENT_UI_EXPORT ZzRatingControl final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRatingControl)
    Q_PROPERTY(
        qreal rating
        READ rating
        WRITE setRating
        NOTIFY ratingChanged)
    Q_PROPERTY(
        int maximumRating
        READ maximumRating
        WRITE setMaximumRating
        NOTIFY maximumRatingChanged)
    Q_PROPERTY(
        ZzFluentUI::ZzRatingPrecision precision
        READ precision
        WRITE setPrecision
        NOTIFY precisionChanged)
    Q_PROPERTY(
        bool readOnly
        READ isReadOnly
        WRITE setReadOnly
        NOTIFY readOnlyChanged)

public:
    /**
     * @brief 创建默认范围为 0..5 的整星评分控件。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzRatingControl(QWidget *parent = nullptr);

    /** @brief 销毁两张图标缓存和无障碍接口。 */
    ~ZzRatingControl() override;

    /** @brief 返回已经过范围约束和精度量化的评分。 */
    [[nodiscard]] qreal rating() const noexcept;

    /**
     * @brief 设置评分；非有限值被拒绝，有限值按范围和精度量化。
     * @param rating 待设置的浮点评分。
     */
    void setRating(qreal rating);

    /** @brief 返回当前最大星数，取值范围为 1..10。 */
    [[nodiscard]] int maximumRating() const noexcept;

    /**
     * @brief 设置最大星数，输入值收敛到 1..10。
     * @param maximum 新最大星数。
     */
    void setMaximumRating(int maximum);

    /** @brief 返回当前整星或半星精度。 */
    [[nodiscard]] ZzRatingPrecision precision() const noexcept;

    /**
     * @brief 设置评分精度并重新量化当前值。
     * @param precision 新精度。
     */
    void setPrecision(ZzRatingPrecision precision);

    /** @brief 返回控件是否只展示评分而拒绝用户输入。 */
    [[nodiscard]] bool isReadOnly() const noexcept;

    /**
     * @brief 设置只读状态；不改变现有评分。
     * @param readOnly 为 true 时拒绝鼠标、键盘和无障碍写入。
     */
    void setReadOnly(bool readOnly);

    /** @brief 返回当前最大星数所需的稳定建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回至少容纳一个星形和焦点环的尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /** @brief 有效评分实际变化后发出。 */
    void ratingChanged(qreal rating);

    /** @brief 收敛后的最大星数实际变化后发出。 */
    void maximumRatingChanged(int maximum);

    /** @brief 评分精度实际变化后发出。 */
    void precisionChanged(ZzRatingPrecision precision);

    /** @brief 只读状态实际变化后发出。 */
    void readOnlyChanged(bool readOnly);

protected:
    /** @brief 绘制空星、按值裁剪的实星和键盘焦点环。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 提交按下位置映射的评分并开始连续拖动。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 更新悬停预览，拖动期间同步提交评分。 */
    void mouseMoveEvent(QMouseEvent *event) override;

    /** @brief 提交释放位置并结束连续拖动。 */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /** @brief 清除仅用于绘制的悬停预览。 */
    void leaveEvent(QEvent *event) override;

    /** @brief 处理方向键、Home 和 End 的可访问数值输入。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 在主题、DPR、方向和启用状态变化后失效派生缓存。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzRatingControlPrivate> d_ptr;
};

} // namespace ZzFluentUI
