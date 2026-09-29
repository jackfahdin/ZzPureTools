#include "ZzDoubleSpinBoxPrivate.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <ZzFluentUI/ZzDoubleSpinBox.h>

namespace ZzFluentUI {

bool ZzDoubleSpinBoxPrivate::setButtonLayout(ZzDoubleSpinBox *spinBox, ZzSpinBoxButtonLayout layout)
{
    switch (layout) {
    case ZzSpinBoxButtonLayout::Vertical:
    case ZzSpinBoxButtonLayout::HorizontalSides:
    case ZzSpinBoxButtonLayout::HorizontalRight:
    case ZzSpinBoxButtonLayout::PlusMinusHorizontalSides:
        break;
    default:
        return false;
    }
    if (layout == buttonLayout) {
        return false;
    }
    buttonLayout = layout;
    if (spinBox->buttonSymbols() != QAbstractSpinBox::NoButtons) {
        spinBox->setButtonSymbols(layout == ZzSpinBoxButtonLayout::PlusMinusHorizontalSides
            ? QAbstractSpinBox::PlusMinus : QAbstractSpinBox::UpDownArrows);
    }
    // StyleChange 同时使 Qt 的尺寸缓存失效并重排原生编辑器。
    QEvent refresh(QEvent::StyleChange);
    QCoreApplication::sendEvent(spinBox, &refresh);
    spinBox->updateGeometry();
    spinBox->update();
    return true;
}

} // namespace ZzFluentUI
