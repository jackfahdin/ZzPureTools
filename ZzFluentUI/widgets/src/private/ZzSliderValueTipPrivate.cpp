#include "ZzSliderValueTipPrivate.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyleOptionSlider>
#include <ZzFluentUI/ZzSliderValueTip.h>

namespace ZzFluentUI {

/** @brief 复用公共提示面板绘制，不抢焦点或拦截鼠标操作。 */
class ZzSliderValueTipPopup final : public QLabel
{
public:
    explicit ZzSliderValueTipPopup(QWidget *parent)
        : QLabel(parent, Qt::ToolTip | Qt::FramelessWindowHint
              | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
    {
        setObjectName(QStringLiteral("zzSliderValueTip"));
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setFocusPolicy(Qt::NoFocus);
        setTextFormat(Qt::PlainText);
        setAlignment(Qt::AlignCenter);
        setForegroundRole(QPalette::ToolTipText);
    }

protected:
    /** @brief 让当前样式绘制完整表面，再由 QLabel 绘制数值文本。 */
    void paintEvent(QPaintEvent *event) override
    {
        {
            QPainter painter(this);
            QStyleOption option;
            option.initFrom(this);
            style()->drawPrimitive(QStyle::PE_PanelTipLabel, &option, &painter, this);
        }
        QLabel::paintEvent(event);
    }
};

ZzSliderValueTipPrivate::ZzSliderValueTipPrivate(ZzSliderValueTip *q, QSlider *target)
    : q_ptr(q), slider(target)
{
    target->installEventFilter(q_ptr);
    // sliderPressed 也可能由 setSliderDown(true) 触发；只由输入事件主动显示。
    QObject::connect(target, &QSlider::sliderReleased, q_ptr, [this] { hide(); });
    QObject::connect(target, &QSlider::sliderMoved, q_ptr, [this] { refresh(); });
    QObject::connect(target, &QSlider::valueChanged, q_ptr, [this] { refresh(); });
    QObject::connect(target, &QSlider::rangeChanged, q_ptr, [this] { refresh(); });
    QObject::connect(target, &QSlider::actionTriggered, q_ptr, [this] { refresh(); });
}

ZzSliderValueTipPrivate::~ZzSliderValueTipPrivate()
{
    hide();
    if (slider != nullptr) {
        slider->removeEventFilter(q_ptr);
    }
    delete tip.data();
}

void ZzSliderValueTipPrivate::show(bool continuous)
{
    if (slider == nullptr || mode == ZzSliderValueTipMode::Disabled
        || !slider->isEnabled() || !slider->isVisible()) {
        return;
    }
    if (tip == nullptr) {
        tip = new ZzSliderValueTipPopup(slider);
    }
    if (ancestors.isEmpty()) {
        for (QWidget *parent = slider->parentWidget(); parent != nullptr;
             parent = parent->parentWidget()) {
            parent->installEventFilter(q_ptr);
            ancestors.append(parent);
        }
    }
    updateTip();
    tip->show();
    if (continuous) {
        hideTimer.stop();
    } else {
        hideTimer.start(900, q_ptr);
    }
}

void ZzSliderValueTipPrivate::refresh()
{
    if (tip != nullptr && tip->isVisible()) {
        updateTip();
    }
}

void ZzSliderValueTipPrivate::hide()
{
    hideTimer.stop();
    if (tip != nullptr) {
        tip->hide();
    }
    for (const auto &ancestor : std::as_const(ancestors)) {
        if (ancestor != nullptr) {
            ancestor->removeEventFilter(q_ptr);
        }
    }
    ancestors.clear();
}

void ZzSliderValueTipPrivate::updateTip()
{
    if (slider == nullptr || tip == nullptr) {
        return;
    }
    const std::int64_t position = slider->sliderPosition();
    const std::int64_t minimum = slider->minimum();
    const std::int64_t span = static_cast<std::int64_t>(slider->maximum()) - minimum;
    const std::int64_t percentage = span > 0 ? ((position - minimum) * 100 + span / 2) / span : 0;
    tip->setText(mode == ZzSliderValueTipMode::Percentage
        ? QString::number(percentage) + QLatin1Char('%') : QString::number(position));
    if (tip->style() != slider->style()) {
        tip->setStyle(slider->style());
    }
    tip->setPalette(slider->palette());
    tip->setFont(slider->font());
    tip->setMargin(slider->style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, nullptr, slider) + 1);
    tip->adjustSize();

    QStyleOptionSlider option;
    option.initFrom(slider);
    option.orientation = slider->orientation();
    option.minimum = slider->minimum();
    option.maximum = slider->maximum();
    option.sliderPosition = slider->sliderPosition();
    option.sliderValue = slider->value();
    option.singleStep = slider->singleStep();
    option.pageStep = slider->pageStep();
    option.tickPosition = slider->tickPosition();
    option.tickInterval = slider->tickInterval();
    const bool horizontal = slider->orientation() == Qt::Horizontal;
    option.upsideDown = horizontal
        ? slider->invertedAppearance() != (slider->layoutDirection() == Qt::RightToLeft)
        : !slider->invertedAppearance();
    option.direction = Qt::LeftToRight;
    option.state.setFlag(QStyle::State_Horizontal, horizontal);
    const QRect localHandle = slider->style()->subControlRect(
        QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider);
    const QRect handle(slider->mapToGlobal(localHandle.topLeft()), localHandle.size());
    QScreen *screen = QGuiApplication::screenAt(handle.center());
    if (screen == nullptr) {
        screen = slider->screen();
    }
    if (screen == nullptr) {
        return;
    }
    const QRect bounds = screen->availableGeometry();
    constexpr int gap = 6;
    QPoint anchor = horizontal
        ? QPoint(handle.center().x() - tip->width() / 2, handle.top() - gap - tip->height())
        : QPoint(handle.right() + gap, handle.center().y() - tip->height() / 2);
    if (horizontal && anchor.y() < bounds.top()) {
        anchor.setY(handle.bottom() + gap);
    } else if (!horizontal && anchor.x() + tip->width() > bounds.right() + 1) {
        anchor.setX(handle.left() - gap - tip->width());
    }
    anchor.setX(std::clamp(anchor.x(), bounds.left(),
        std::max(bounds.left(), bounds.right() - tip->width() + 1)));
    anchor.setY(std::clamp(anchor.y(), bounds.top(),
        std::max(bounds.top(), bounds.bottom() - tip->height() + 1)));
    tip->move(anchor);
    tip->update();
}

void ZzSliderValueTipPrivate::handleEvent(QObject *watched, QEvent *event)
{
    if (slider == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::Hide:
    case QEvent::WindowDeactivate:
    case QEvent::FocusOut:
    case QEvent::ParentAboutToChange:
        hide();
        break;
    case QEvent::EnabledChange:
        if (!slider->isEnabled()) {
            hide();
        }
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::DevicePixelRatioChange:
        refresh();
        break;
    default:
        break;
    }
    if (watched != slider) {
        return;
    }
    if (event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        show(true);
    } else if (event->type() == QEvent::MouseButtonRelease
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        hide();
    } else if (event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Home:
        case Qt::Key_End:
            show(false);
            break;
        default:
            break;
        }
    } else if (event->type() == QEvent::Wheel) {
        const auto *wheel = static_cast<QWheelEvent *>(event);
        if (!wheel->angleDelta().isNull() || !wheel->pixelDelta().isNull()) {
            show(false);
        }
    }
}

} // namespace ZzFluentUI
