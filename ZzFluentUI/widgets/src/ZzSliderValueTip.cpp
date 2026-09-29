#include <ZzFluentUI/ZzSliderValueTip.h>

#include <QtCore/QTimerEvent>
#include <QtWidgets/QSlider>
#include "private/ZzSliderValueTipPrivate.h"

namespace ZzFluentUI {

ZzSliderValueTip *ZzSliderValueTip::attach(QSlider *slider, ZzSliderValueTipMode mode)
{
    if (slider == nullptr) {
        return nullptr;
    }
    auto *tip = slider->findChild<ZzSliderValueTip *>(QString(), Qt::FindDirectChildrenOnly);
    if (tip == nullptr) {
        tip = new ZzSliderValueTip(slider);
    }
    tip->setMode(mode);
    return tip;
}

ZzSliderValueTip::ZzSliderValueTip(QSlider *slider)
    : QObject(slider), d_ptr(std::make_unique<ZzSliderValueTipPrivate>(this, slider))
{
}

ZzSliderValueTip::~ZzSliderValueTip() = default;

ZzSliderValueTipMode ZzSliderValueTip::mode() const noexcept
{
    return d_ptr->mode;
}

void ZzSliderValueTip::setMode(ZzSliderValueTipMode mode)
{
    if (mode != ZzSliderValueTipMode::Disabled && mode != ZzSliderValueTipMode::Value
        && mode != ZzSliderValueTipMode::Percentage) {
        return;
    }
    if (d_ptr->mode == mode) {
        return;
    }
    d_ptr->mode = mode;
    if (mode == ZzSliderValueTipMode::Disabled) {
        d_ptr->hide();
    } else {
        d_ptr->refresh();
    }
    Q_EMIT modeChanged(mode);
}

bool ZzSliderValueTip::eventFilter(QObject *watched, QEvent *event)
{
    d_ptr->handleEvent(watched, event);
    return QObject::eventFilter(watched, event);
}

void ZzSliderValueTip::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == d_ptr->hideTimer.timerId()) {
        d_ptr->hide();
        return;
    }
    QObject::timerEvent(event);
}

} // namespace ZzFluentUI
