#pragma once

#include <memory>

#include <QtWidgets/QWidget>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzRangeSliderPrivate;

/** @brief 选择有序整数区间的双手柄 Fluent 范围滑块。 */
class ZZ_FLUENT_UI_EXPORT ZzRangeSlider final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRangeSlider)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int lowerValue READ lowerValue WRITE setLowerValue NOTIFY lowerValueChanged)
    Q_PROPERTY(int upperValue READ upperValue WRITE setUpperValue NOTIFY upperValueChanged)
    Q_PROPERTY(int lowerPosition READ lowerPosition)
    Q_PROPERTY(int upperPosition READ upperPosition)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(ZzSnapMode snapMode READ snapMode WRITE setSnapMode)
    Q_PROPERTY(bool tickPosition READ hasTickPosition WRITE setTickPosition)
    Q_PROPERTY(int tickInterval READ tickInterval WRITE setTickInterval)
    Q_PROPERTY(bool tracking READ hasTracking WRITE setTracking)
    Q_PROPERTY(bool valueTipEnabled READ valueTipEnabled WRITE setValueTipEnabled)
    Q_PROPERTY(bool handleFocusRingEnabled READ handleFocusRingEnabled WRITE setHandleFocusRingEnabled)

public:
    enum class ZzSnapMode { NoSnap, SnapAlways, SnapOnRelease };
    Q_ENUM(ZzSnapMode)
    enum class ZzSliderHandle { NoHandle, LowerHandle, UpperHandle };
    Q_ENUM(ZzSliderHandle)

    explicit ZzRangeSlider(QWidget *parent = nullptr);
    explicit ZzRangeSlider(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~ZzRangeSlider() override;

    [[nodiscard]] int minimum() const noexcept;
    [[nodiscard]] int maximum() const noexcept;
    void setMinimum(int minimum);
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);

    [[nodiscard]] int lowerValue() const noexcept;
    [[nodiscard]] int upperValue() const noexcept;
    void setLowerValue(int value);
    void setUpperValue(int value);
    void setValues(int lower, int upper);
    [[nodiscard]] int lowerPosition() const noexcept;
    [[nodiscard]] int upperPosition() const noexcept;

    [[nodiscard]] int singleStep() const noexcept;
    [[nodiscard]] int pageStep() const noexcept;
    void setSingleStep(int step);
    void setPageStep(int step);
    [[nodiscard]] Qt::Orientation orientation() const noexcept;
    void setOrientation(Qt::Orientation orientation);
    [[nodiscard]] ZzSnapMode snapMode() const noexcept;
    void setSnapMode(ZzSnapMode mode);
    [[nodiscard]] bool hasTickPosition() const noexcept;
    void setTickPosition(bool enabled);
    [[nodiscard]] int tickInterval() const noexcept;
    void setTickInterval(int interval);
    [[nodiscard]] bool hasTracking() const noexcept;
    void setTracking(bool enabled);
    [[nodiscard]] bool valueTipEnabled() const noexcept;
    void setValueTipEnabled(bool enabled);
    [[nodiscard]] bool handleFocusRingEnabled() const noexcept;
    void setHandleFocusRingEnabled(bool enabled);
    [[nodiscard]] ZzSliderHandle activeHandle() const noexcept;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void lowerValueChanged(int value);
    void upperValueChanged(int value);
    void valuesChanged(int lower, int upper);
    void rangeChanged(int minimum, int maximum);
    void sliderPressed(ZzRangeSlider::ZzSliderHandle handle);
    void sliderReleased(ZzRangeSlider::ZzSliderHandle handle);
    void sliderMoved(int lower, int upper);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    std::unique_ptr<ZzRangeSliderPrivate> d_ptr;
};

} // namespace ZzFluentUI
