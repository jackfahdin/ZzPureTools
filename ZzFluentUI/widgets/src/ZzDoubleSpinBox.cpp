#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include "private/ZzDoubleSpinBoxPrivate.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>

namespace ZzFluentUI {

ZzDoubleSpinBox::ZzDoubleSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
    , d_ptr(std::make_unique<ZzDoubleSpinBoxPrivate>())
{
    // Qt 基类构造期间尚不能识别派生布局，完整构造后刷新编辑器几何。
    QEvent refresh(QEvent::StyleChange);
    QCoreApplication::sendEvent(this, &refresh);
}

ZzDoubleSpinBox::~ZzDoubleSpinBox() = default;

ZzSpinBoxButtonLayout ZzDoubleSpinBox::buttonLayout() const noexcept
{
    return d_ptr->buttonLayout;
}

void ZzDoubleSpinBox::setButtonLayout(ZzSpinBoxButtonLayout layout)
{
    if (d_ptr->setButtonLayout(this, layout)) {
        Q_EMIT buttonLayoutChanged(layout);
    }
}

} // namespace ZzFluentUI
